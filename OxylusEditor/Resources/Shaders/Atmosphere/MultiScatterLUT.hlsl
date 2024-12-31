#define HAS_TRANSMITTANCE_LUT
#include "SkyCommon.hlsli"

static const float MULTIPLE_SCATTERING_FACTOR = 1.0;

groupshared float3 MultiScatAs1SharedMem[64];
groupshared float3 LSharedMem[64];

[numthreads(1, 1, 64)]
void main(uint3 threadID : SV_DispatchThreadID) {
  RWTexture2D<float4> MultiScatterLut = get_sky_multi_scatter_lut_texturerw();

  float2 pixelPosition = float2(threadID.xy) + 0.5;

  int width, height;
  MultiScatterLut.GetDimensions(width, height);

  const float u = float(threadID.x) / width;
  const float v = float(threadID.y) / height;

  AtmosphereParameters atmosphereParameters;
  InitAtmosphereParameters(atmosphereParameters);

  float cosSunZenithAngle = u * 2.0 - 1.0;
  float3 sunDirection = float3(0.0, sqrt(saturate(1.0 - cosSunZenithAngle * cosSunZenithAngle)), cosSunZenithAngle);
  // We adjust again viewHeight according to PLANET_RADIUS_OFFSET to be in a valid range.
  float viewHeight = atmosphereParameters.bottomRadius + saturate(v + PLANET_RADIUS_OFFSET) * (atmosphereParameters.topRadius - atmosphereParameters.bottomRadius - PLANET_RADIUS_OFFSET);

  float3 worldPosition = float3(0.0, 0.0, viewHeight);
  float3 worldDirection = float3(0.0, 0.0, 1.0);

  // When building the scattering factor, we assume light illuminance is 1 to compute a transfert function relative to identity illuminance of 1.
  // This make the scattering factor independent of the light. It is now only linked to the atmosphere properties.
  float3 sunIlluminance = 1.0;

  const float tDepth = 0.0;
  const float sampleCountIni = 20.0;
  const bool variableSampleCount = false;
  const bool perPixelNoise = false;
  const bool opaque = false;
  const bool ground = true;
  const bool mieRayPhase = false;
  const bool multiScatteringApprox = false;
  const bool volumetricCloudShadow = false;
  const bool opaqueShadow = false;

  const float sphereSolidAngle = 4.0 * PI;
  const float isotropicPhase = 1.0 / sphereSolidAngle;

  // Reference. Since there are many sample, it requires MULTI_SCATTERING_POWER_SERIE to be true for accuracy and to avoid divergences (see declaration for explanations)
#define SQRTSAMPLECOUNT 8
  const float sqrtSample = float(SQRTSAMPLECOUNT);
  const float i = 0.5f + float(threadID.z / SQRTSAMPLECOUNT);
  const float j = 0.5f + float(threadID.z - float((threadID.z / SQRTSAMPLECOUNT) * SQRTSAMPLECOUNT));
  {
    const float randA = i / sqrtSample;
    const float randB = j / sqrtSample;
    const float theta = 2.0f * PI * randA;
    const float phi = PI * randB;
    const float cosPhi = cos(phi);
    const float sinPhi = sin(phi);
    const float cosTheta = cos(theta);
    const float sinTheta = sin(theta);
    worldDirection.x = cosTheta * sinPhi;
    worldDirection.y = sinTheta * sinPhi;
    worldDirection.z = cosPhi;
    const SingleScatteringResult result = IntegrateScatteredLuminance(
      atmosphereParameters,
      pixelPosition,
      worldPosition,
      worldDirection,
      sunDirection,
      sunIlluminance,
      tDepth,
      sampleCountIni,
      variableSampleCount,
      perPixelNoise,
      opaque,
      ground,
      mieRayPhase,
      multiScatteringApprox,
      volumetricCloudShadow,
      opaqueShadow,
      get_sky_transmittance_lut_texture()
    );

    MultiScatAs1SharedMem[threadID.z] = result.multiScatAs1 * sphereSolidAngle / (sqrtSample * sqrtSample);
    LSharedMem[threadID.z] = result.L * sphereSolidAngle / (sqrtSample * sqrtSample);
  }
#undef SQRTSAMPLECOUNT

  GroupMemoryBarrierWithGroupSync();

  // 64 to 32
  if (threadID.z < 32) {
    MultiScatAs1SharedMem[threadID.z] += MultiScatAs1SharedMem[threadID.z + 32];
    LSharedMem[threadID.z] += LSharedMem[threadID.z + 32];
  }
  GroupMemoryBarrierWithGroupSync();

  // 32 to 16
  if (threadID.z < 16) {
    MultiScatAs1SharedMem[threadID.z] += MultiScatAs1SharedMem[threadID.z + 16];
    LSharedMem[threadID.z] += LSharedMem[threadID.z + 16];
  }
  GroupMemoryBarrierWithGroupSync();

  // 16 to 8 (16 is thread group min hardware size with intel, no sync required from there)
  if (threadID.z < 8) {
    MultiScatAs1SharedMem[threadID.z] += MultiScatAs1SharedMem[threadID.z + 8];
    LSharedMem[threadID.z] += LSharedMem[threadID.z + 8];
  }
  GroupMemoryBarrierWithGroupSync();
  if (threadID.z < 4) {
    MultiScatAs1SharedMem[threadID.z] += MultiScatAs1SharedMem[threadID.z + 4];
    LSharedMem[threadID.z] += LSharedMem[threadID.z + 4];
  }
  GroupMemoryBarrierWithGroupSync();
  if (threadID.z < 2) {
    MultiScatAs1SharedMem[threadID.z] += MultiScatAs1SharedMem[threadID.z + 2];
    LSharedMem[threadID.z] += LSharedMem[threadID.z + 2];
  }
  GroupMemoryBarrierWithGroupSync();
  if (threadID.z < 1) {
    MultiScatAs1SharedMem[threadID.z] += MultiScatAs1SharedMem[threadID.z + 1];
    LSharedMem[threadID.z] += LSharedMem[threadID.z + 1];
  }
  GroupMemoryBarrierWithGroupSync();
  if (threadID.z > 0)
    return;

  float3 MultiScatAs1 = MultiScatAs1SharedMem[0] * isotropicPhase; // Equation 7 f_ms
  float3 InScatteredLuminance = LSharedMem[0] * isotropicPhase;    // Equation 5 L_2ndOrder

  // MultiScatAs1 represents the amount of luminance scattered as if the integral of scattered luminance over the sphere would be 1.
  //  - 1st order of scattering: one can ray-march a straight path as usual over the sphere. That is InScatteredLuminance.
  //  - 2nd order of scattering: the inscattered luminance is InScatteredLuminance at each of samples of fist order integration. Assuming a uniform phase function that is represented by MultiScatAs1,
  //  - 3nd order of scattering: the inscattered luminance is (InScatteredLuminance * MultiScatAs1 * MultiScatAs1)
  //  - etc.
#if	MULTI_SCATTERING_POWER_SERIE==0 // from IntegrateScatteredLuminance
  float3 MultiScatAs1SQR = MultiScatAs1 * MultiScatAs1;
  float3 L = InScatteredLuminance * (1.0 + MultiScatAs1 + MultiScatAs1SQR + MultiScatAs1 * MultiScatAs1SQR + MultiScatAs1SQR * MultiScatAs1SQR);
#else
  // For a serie, sum_{n=0}^{n=+inf} = 1 + r + r^2 + r^3 + ... + r^n = 1 / (1.0 - r), see https://en.wikipedia.org/wiki/Geometric_series 
  const float3 r = MultiScatAs1;
  const float3 SumOfAllMultiScatteringEventsContribution = 1.0f / (1.0 - r);
  float3 L = InScatteredLuminance * SumOfAllMultiScatteringEventsContribution; // Equation 10 Psi_ms
#endif

  MultiScatterLut[threadID.xy] = float4(MULTIPLE_SCATTERING_FACTOR * L, 1.0f);
}
