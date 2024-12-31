#include "SkyCommon.hlsli"

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
  float2 pixelPosition = float2(threadID.xy) + 0.5;

  RWTexture2D<float4> image = get_sky_transmittance_lut_texturerw();

  int width, height;
  image.GetDimensions(width, height);

  const float2 uv = float2(float(threadID.x) / width, float(threadID.y) / height);

  AtmosphereParameters atmosphere;
  InitAtmosphereParameters(atmosphere);

  // Compute camera position from LUT coords
  float viewHeight;
  float viewZenithCosAngle;
  UvToLutTransmittanceParams(atmosphere, viewHeight, viewZenithCosAngle, uv);

  // A few ekstra needed constants
  float3 worldPosition = float3(0.0, 0.0, viewHeight);
  float3 worldDirection = float3(0.0f, sqrt(1.0 - viewZenithCosAngle * viewZenithCosAngle), viewZenithCosAngle);
  float3 sunDirection = get_scene().sun_direction.unpack();
  float3 sunIlluminance = get_scene().sun_color.unpack();

  const float tDepth = 0.0;
  const float sampleCountIni = 40.0;
  const bool variableSampleCount = false;
  const bool perPixelNoise = false;
  const bool opaque = false;
  const bool ground = false;
  const bool mieRayPhase = false;
  const bool multiScatteringApprox = false;
  const bool volumetricCloudShadow = false;
  const bool opaqueShadow = false;
  SingleScatteringResult ss = IntegrateScatteredLuminance(
    atmosphere,
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
    opaqueShadow);

  float3 transmittance = exp(-ss.opticalDepth);

  image[threadID.xy] = float4(transmittance, 1.0);
}
