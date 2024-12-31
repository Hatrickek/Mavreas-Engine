#ifndef SKYCOMMON_HLSLI
#define SKYCOMMON_HLSLI

#include "../Globals.hlsli"

#define PLANET_RADIUS_OFFSET 0.001f // Float accuracy offset in Sky unit (km, so this is 1m)
#define USE_CORNETTE_SHANKS

static const float M_TO_SKY_UNIT = 0.001f; // Engine units are in meters
static const float SKY_UNIT_TO_M = rcp(M_TO_SKY_UNIT);
static const float AP_START_OFFSET_KM = 0.1f;

static const float2 MULTI_SCATTERING_LUT_RES = float2(32, 32);
static const float2 SKY_VIEW_LUT_RES = float2(192, 104);

struct AtmosphereParameters {
  float2 padding0;
  // Radius of the planet (center to ground)
  float bottomRadius;
  // Maximum considered atmosphere height (center to atmosphere top)
  float topRadius;

  // Center of the planet
  float3 planetCenter;
  // Rayleigh scattering exponential distribution scale in the atmosphere
  float rayleighDensityExpScale;

  // Rayleigh scattering coefficients
  float3 rayleighScattering;
  // Mie scattering exponential distribution scale in the atmosphere
  float mieDensityExpScale;

  // Mie scattering coefficients
  float3 mieScattering;
  float padding1;

  // Mie extinction coefficients
  float3 mieExtinction;
  float padding2;

  // Mie absorption coefficients
  float3 mieAbsorption;
  // Mie phase function excentricity
  float miePhaseG;

  // Another medium type in the atmosphere
  float absorptionDensity0LayerWidth;
  float absorptionDensity0ConstantTerm;
  float absorptionDensity0LinearTerm;
  float absorptionDensity1ConstantTerm;

  float3 padding3;
  float absorptionDensity1LinearTerm;

  // This other medium only absorb light, e.g. useful to represent ozone in the earth atmosphere
  float3 absorptionExtinction;
  float padding4;

  // The albedo of the ground.
  float3 groundAlbedo;
  float padding5;

  // Varying sample count for sky rendering based on the 'distanceSPPMaxInv' with min-max
  float2 rayMarchMinMaxSPP;
  // Describes how the raymarching samples are distributed no the screen
  float distanceSPPMaxInv;
  // Aerial Perspective exposure override
  float aerialPerspectiveScale;
};

void InitAtmosphereParameters(out AtmosphereParameters parameters) {
  const float earthBottomRadius = 6360.0f;
  const float earthTopRadius = 6460.0f;
  const float earthRayleighScaleHeight = 8.0f;
  const float earthMieScaleHeight = 1.2f;

  // Values shown here are the result of integration over wavelength power spectrum integrated with paricular function.
  // Refer to https://github.com/ebruneton/precomputed_atmospheric_scattering for details.

  // Translation from Bruneton2017 parameterisation.
  parameters.rayleighDensityExpScale = -1.0f / earthRayleighScaleHeight;
  parameters.mieDensityExpScale = -1.0f / earthMieScaleHeight;
  parameters.absorptionDensity0LayerWidth = 25.0f;
  parameters.absorptionDensity0ConstantTerm = -2.0f / 3.0f;
  parameters.absorptionDensity0LinearTerm = 1.0f / 15.0f;
  parameters.absorptionDensity1ConstantTerm = 8.0f / 3.0f;
  parameters.absorptionDensity1LinearTerm = -1.0f / 15.0f;

  parameters.miePhaseG = 0.8f;
  parameters.rayleighScattering = float3(0.005802f, 0.013558f, 0.033100f);
  parameters.mieScattering = float3(0.003996f, 0.003996f, 0.003996f);
  parameters.mieExtinction = float3(0.004440f, 0.004440f, 0.004440f);
  parameters.mieAbsorption.x = parameters.mieExtinction.x - parameters.mieScattering.x;
  parameters.mieAbsorption.y = parameters.mieExtinction.y - parameters.mieScattering.y;
  parameters.mieAbsorption.z = parameters.mieExtinction.z - parameters.mieScattering.z;

  parameters.absorptionExtinction = float3(0.000650f, 0.001881f, 0.000085f);

  parameters.groundAlbedo = float3(0.3f, 0.3f, 0.3f); // 0.3 for earths ground albedo, see https://nssdc.gsfc.nasa.gov/planetary/factsheet/earthfact.html
  parameters.bottomRadius = earthBottomRadius;
  parameters.topRadius = earthTopRadius;
  parameters.planetCenter = float3(0.0f, -earthBottomRadius - 0.1f, 0.0f); // Spawn 100m in the air

  parameters.rayMarchMinMaxSPP = float2(4.0f, 14.0f);
  parameters.distanceSPPMaxInv = 0.01f;
  parameters.aerialPerspectiveScale = 1.0f;
}

float FromUnitToSubUvs(float u, float resolution) {
  return (u + 0.5f / resolution) * (resolution / (resolution + 1.0f));
}

float FromSubUvsToUnit(float u, float resolution) {
  return (u - 0.5f / resolution) * (resolution / (resolution - 1.0f));
}

float3 GetSkyWorldCameraOrigin(AtmosphereParameters atmosphere, float3 cameraPosition) {
  const float planetRadiusOffset = 0.01; // Always force to be 10 meters above the ground/sea level
  const float offset = planetRadiusOffset * SKY_UNIT_TO_M;

  const float3 planetCenterWorld = atmosphere.planetCenter * SKY_UNIT_TO_M;
  const float bottomRadiusWorld = atmosphere.bottomRadius * SKY_UNIT_TO_M;
  const float bottomRadiusWorldOffset = bottomRadiusWorld + offset;

  const float3 planetCenterToCameraWorld = cameraPosition - planetCenterWorld;
  const float distanceToPlanetCenterWorld = length(planetCenterToCameraWorld);
  const float3 planetCenterToCameraWorldNormalized = planetCenterToCameraWorld / distanceToPlanetCenterWorld;

  // If the camera is below the planet surface, we snap it back onto the surface.
  // This is to make sure the sky is always visible even if the camera is inside the virtual planet.
  return distanceToPlanetCenterWorld < bottomRadiusWorldOffset ? planetCenterWorld + bottomRadiusWorldOffset * planetCenterToCameraWorldNormalized : cameraPosition;
}

float3 GetCameraPlanetPos(AtmosphereParameters atmosphere, float3 cameraPosition) {
  const float3 planetCenterWorld = atmosphere.planetCenter * SKY_UNIT_TO_M;
  const float3 skyWorldCameraOrigin = GetSkyWorldCameraOrigin(atmosphere, cameraPosition);

  return (skyWorldCameraOrigin - planetCenterWorld) * M_TO_SKY_UNIT;
}

float2 RaySphereIntersect(float3 rayOrigin, float3 rayDirection, float3 sphereCenter, float sphereRadius) {
  float3 s0_r0 = rayOrigin - sphereCenter;
  float a = dot(rayDirection, rayDirection);
  float b = 2.0 * dot(rayDirection, s0_r0);
  float c = dot(s0_r0, s0_r0) - (sphereRadius * sphereRadius);

  float delta = b * b - 4.0 * a * c;

  float2 sol = -1;

  if (delta >= 0.0) {
    return (-b + float2(-1, 1) * sqrt(delta)) / (2.0 * a);
  }

  return sol;
}

float RaySphereIntersectNearest(float3 rayOrigin, float3 rayDirection, float3 sphereCenter, float sphereRadius) {
  float2 sol = RaySphereIntersect(rayOrigin, rayDirection, sphereCenter, sphereRadius);
  float sol0 = sol.x;
  float sol1 = sol.y;

  if (sol0 < 0.0 && sol1 < 0.0) {
    return -1.0;
  }
  if (sol0 < 0.0) {
    return max(0.0, sol1);
  }
  else if (sol1 < 0.0) {
    return max(0.0, sol0);
  }
  return max(0.0, min(sol0, sol1));
}

bool MoveToTopAtmosphere(inout float3 worldPosition, in float3 worldDirection, in float atmosphereTopRadius) {
  float viewHeight = length(worldPosition);

  bool retval = true;
  if (viewHeight > atmosphereTopRadius) {
    float tTop = RaySphereIntersectNearest(worldPosition, worldDirection, float3(0.0f, 0.0f, 0.0f), atmosphereTopRadius);
    if (tTop >= 0.0f) {
      float3 upVector = worldPosition / viewHeight;
      float3 upOffset = upVector * -PLANET_RADIUS_OFFSET;
      worldPosition = worldPosition + worldDirection * tTop + upOffset;
    }
    else {
      // Ray is not intersecting the atmosphere
      retval = false;
    }
  }
  return retval; // ok to start tracing
}

float UniformPhase() {
  return 1.0f / (4.0f * PI);
}

float CornetteShanksMiePhaseFunction(float g, float cosTheta) {
  float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
  return k * (1.0 + cosTheta * cosTheta) / pow(abs(1.0 + g * g - 2.0 * g * -cosTheta), 1.5);
}

float HgPhase(float g, float cosTheta) {
#ifdef USE_CORNETTE_SHANKS
  return CornetteShanksMiePhaseFunction(g, cosTheta);
#else
  // Reference implementation (i.e. not schlick approximation). 
  // See http://www.pbr-book.org/3ed-2018/Volume_Scattering/Phase_Functions.html
  float numer = 1.0f - g * g;
  float denom = 1.0f + g * g + 2.0f * g * cosTheta;
  return numer / (4.0f * PI * denom * sqrt(denom));
#endif
}

float RayleighPhase(float cosTheta) {
  float factor = 3.0f / (16.0f * PI);
  return factor * (1.0f + cosTheta * cosTheta);
}

void LutTransmittanceParamsToUv(float bottomRadius, float topRadius, in float viewHeight, in float viewZenithCosAngle, out float2 uv) {
  float H = sqrt(max(0.0f, topRadius * topRadius - bottomRadius * bottomRadius));
  float rho = sqrt(max(0.0f, viewHeight * viewHeight - bottomRadius * bottomRadius));

  float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + topRadius * topRadius;
  float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

  float d_min = topRadius - viewHeight;
  float d_max = rho + H;
  float x_mu = (d - d_min) / (d_max - d_min);
  float x_r = rho / H;

  uv = float2(x_mu, x_r);
}

void UvToLutTransmittanceParams(AtmosphereParameters atmosphere, out float viewHeight, out float viewZenithCosAngle, in float2 uv) {
  //uv = float2(FromSubUvsToUnit(uv.x, transmittanceLUTRes.x), FromSubUvsToUnit(uv.y, transmittanceLUTRes.y)); // No real impact so off
  float x_mu = uv.x;
  float x_r = uv.y;

  float H = sqrt(atmosphere.topRadius * atmosphere.topRadius - atmosphere.bottomRadius * atmosphere.bottomRadius);
  float rho = H * x_r;
  viewHeight = sqrt(rho * rho + atmosphere.bottomRadius * atmosphere.bottomRadius);

  float d_min = atmosphere.topRadius - viewHeight;
  float d_max = rho + H;
  float d = d_min + x_mu * (d_max - d_min);
  viewZenithCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
  viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}

#define NONLINEARSKYVIEWLUT 1

void UvToSkyViewLutParams(AtmosphereParameters atmosphere, out float viewZenithCosAngle, out float lightViewCosAngle, in float viewHeight, in float2 uv) {
  // Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
  float Vhorizon = sqrt(viewHeight * viewHeight - atmosphere.bottomRadius * atmosphere.bottomRadius);
  float CosBeta = Vhorizon / viewHeight; // GroundToHorizonCos
  float Beta = acos(CosBeta);
  float ZenithHorizonAngle = PI - Beta;

  if (uv.y < 0.5f) {
    float coord = 2.0 * uv.y;
    coord = 1.0 - coord;
#if NONLINEARSKYVIEWLUT
    coord *= coord;
#endif
    coord = 1.0 - coord;
    viewZenithCosAngle = cos(ZenithHorizonAngle * coord);
  }
  else {
    float coord = uv.y * 2.0 - 1.0;
#if NONLINEARSKYVIEWLUT
    coord *= coord;
#endif
    viewZenithCosAngle = cos(ZenithHorizonAngle + Beta * coord);
  }

  float coord = uv.x;
  coord *= coord;
  lightViewCosAngle = -(coord * 2.0 - 1.0);
}

void SkyViewLutParamsToUv(AtmosphereParameters atmosphere, bool intersectGround, float viewZenithCosAngle, float lightViewCosAngle, float viewHeight, out float2 uv) {
  float Vhorizon = sqrt(viewHeight * viewHeight - atmosphere.bottomRadius * atmosphere.bottomRadius);
  float CosBeta = Vhorizon / viewHeight; // GroundToHorizonCos
  float Beta = acos(CosBeta);
  float ZenithHorizonAngle = PI - Beta;

  if (!intersectGround) {
    float coord = acos(viewZenithCosAngle) / ZenithHorizonAngle;
    coord = 1.0 - coord;
#if NONLINEARSKYVIEWLUT
    coord = sqrt(abs(coord));
#endif
    coord = 1.0 - coord;
    uv.y = coord * 0.5f;
  }
  else {
    float coord = (acos(viewZenithCosAngle) - ZenithHorizonAngle) / Beta;
#if NONLINEARSKYVIEWLUT
    coord = sqrt(abs(coord));
#endif
    uv.y = coord * 0.5f + 0.5f;
  }

  {
    float coord = -lightViewCosAngle * 0.5f + 0.5f;
    coord = sqrt(coord);
    uv.x = coord;
  }

  // Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
  uv = float2(FromUnitToSubUvs(uv.x, SKY_VIEW_LUT_RES.x), FromUnitToSubUvs(uv.y, SKY_VIEW_LUT_RES.y));
}

float3 GetTransmittance(float bottomRadius, float topRadius, float pHeight, float sunZenithCosAngle, Texture2D<float4> transmittanceLutTexture) {
  float2 uv;
  LutTransmittanceParamsToUv(bottomRadius, topRadius, pHeight, sunZenithCosAngle, uv);

  float3 TransmittanceToSun = transmittanceLutTexture.SampleLevel(LINEAR_CLAMPED_SAMPLER, uv, 0).rgb;
  return TransmittanceToSun;
}

float3 GetMultipleScattering(AtmosphereParameters atmosphere, Texture2D<float4> multiScatteringLUTTexture, float3 scattering, float3 extinction, float3 worldPosition, float viewZenithCosAngle) {
  float2 uv = saturate(float2(viewZenithCosAngle * 0.5f + 0.5f, (length(worldPosition) - atmosphere.bottomRadius) / (atmosphere.topRadius - atmosphere.bottomRadius)));
  uv = float2(FromUnitToSubUvs(uv.x, MULTI_SCATTERING_LUT_RES.x), FromUnitToSubUvs(uv.y, MULTI_SCATTERING_LUT_RES.y));

  float3 multiScatteredLuminance = multiScatteringLUTTexture.SampleLevel(LINEAR_CLAMPED_SAMPLER, uv, 0).rgb;
  return multiScatteredLuminance;
}

struct MediumSampleRGB {
  float3 scattering;
  float3 absorption;
  float3 extinction;

  float3 scatteringMie;
  float3 absorptionMie;
  float3 extinctionMie;

  float3 scatteringRay;
  float3 absorptionRay;
  float3 extinctionRay;

  float3 scatteringOzo;
  float3 absorptionOzo;
  float3 extinctionOzo;

  float3 albedo;
};

float3 GetAlbedo(float3 scattering, float3 extinction) {
  return scattering / max(0.001, extinction);
}

MediumSampleRGB SampleMediumRGB(AtmosphereParameters atmosphere, in float3 worldPos) {
  const float viewHeight = length(worldPos) - atmosphere.bottomRadius;

  const float densityMie = exp(atmosphere.mieDensityExpScale * viewHeight);
  const float densityRay = exp(atmosphere.rayleighDensityExpScale * viewHeight);
  const float densityOzo = saturate(viewHeight < atmosphere.absorptionDensity0LayerWidth
                                      ? atmosphere.absorptionDensity0LinearTerm * viewHeight + atmosphere.absorptionDensity0ConstantTerm
                                      : atmosphere.absorptionDensity1LinearTerm * viewHeight + atmosphere.absorptionDensity1ConstantTerm);
  MediumSampleRGB s;

  s.scatteringMie = densityMie * atmosphere.mieScattering;
  s.absorptionMie = densityMie * atmosphere.mieAbsorption;
  s.extinctionMie = densityMie * atmosphere.mieExtinction;

  s.scatteringRay = densityRay * atmosphere.rayleighScattering;
  s.absorptionRay = 0.0f;
  s.extinctionRay = s.scatteringRay + s.absorptionRay;

  s.scatteringOzo = 0.0;
  s.absorptionOzo = densityOzo * atmosphere.absorptionExtinction;
  s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

  s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
  s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
  s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
  s.albedo = GetAlbedo(s.scattering, s.extinction);

  return s;
}

struct SingleScatteringResult {
  float3 L;             // Scattered light (luminance)
  float3 opticalDepth;  // Optical depth (1/m)
  float3 transmittance; // Transmittance in [0,1] (unitless)
  float3 multiScatAs1;

  float3 newMultiScatStep0Out;
  float3 newMultiScatStep1Out;
};

SingleScatteringResult IntegrateScatteredLuminance(
  AtmosphereParameters atmosphereParameters,
  in float2 pixelPosition,
  in float3 worldPosition,
  in float3 worldDirection,
  in float3 sunDirection,
  in float3 sunIlluminance,
  in float tDepth,
  in float sampleCountIni,
  bool variableSampleCount,
  bool perPixelNoise,
  bool opaque,
  bool ground,
  bool mieRayPhase,
  bool multiScatteringApprox,
  bool volumetricCloudShadow,
  bool opaqueShadow,
#ifdef HAS_TRANSMITTANCE_LUT
  Texture2D<float4> transmittanceLutTexture,
#endif
#ifdef HAS_MULTISCATTER_LUT
  Texture2D<float4> multiScatteringLUT,
#endif
  float opticalDepthScale = 1.0f,
  float tMaxMax = 9000000.0f) {
  SingleScatteringResult result = (SingleScatteringResult)0;
  result.L = 0;
  result.opticalDepth = 0;
  result.transmittance = 1.0;
  result.multiScatAs1 = 0;

  // If camera is inside ground, return
  if (length(worldPosition) <= atmosphereParameters.bottomRadius) {
    return result;
  }

  // Compute next intersection with atmosphere or ground 
  float3 earthO = float3(0.0f, 0.0f, 0.0f);
  float tBottom = RaySphereIntersectNearest(worldPosition, worldDirection, earthO, atmosphereParameters.bottomRadius);
  float tTop = RaySphereIntersectNearest(worldPosition, worldDirection, earthO, atmosphereParameters.topRadius);
  float tMax = 0.0f;

  bool proceed = true;
  if (tBottom < 0.0f) {
    if (tTop < 0.0f) {
      tMax = 0.0f; // No intersection with earth nor atmosphere: stop right away  
      proceed = false;
    }
    else {
      tMax = tTop;
    }
  }
  else {
    if (tTop > 0.0f) {
      tMax = min(tTop, tBottom);
    }
  }

  [branch]
  if (proceed) {
    if (opaque) {
      if (tDepth < tMax) {
        tMax = tDepth;
      }
    }
    tMax = min(tMax, tMaxMax);

    // Sample count 
    float sampleCount = sampleCountIni;
    float sampleCountFloor = sampleCountIni;
    float tMaxFloor = tMax;
    if (variableSampleCount) {
      sampleCount = lerp(atmosphereParameters.rayMarchMinMaxSPP.x, atmosphereParameters.rayMarchMinMaxSPP.y, saturate(tMax * atmosphereParameters.distanceSPPMaxInv));
      sampleCountFloor = floor(sampleCount);
      tMaxFloor = tMax * sampleCountFloor / sampleCount; // rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / sampleCount;

    // Unlike volumetric fog lighting, we only care about the outmost cascade. This improves performance where we can't see the inner cascades anyway
#if 0
    ShaderEntity light = (ShaderEntity)0;
    uint furthestCascade = 0;
    bool validLight = false;

    if (opaqueShadow) {
      validLight = furthest_cascade_volumetrics(light, furthestCascade);
    }
#endif

    // Phase functions
    const float uniformPhase = UniformPhase();
    const float3 wi = sunDirection;
    const float3 wo = worldDirection;
    float cosTheta = dot(wi, wo);
    float miePhaseValue = HgPhase(atmosphereParameters.miePhaseG, -cosTheta); // negate cosTheta because due to WorldDir being a "in" direction. 
    float rayleighPhaseValue = RayleighPhase(cosTheta);

    float3 globalL = sunIlluminance;

    // Ray march the atmosphere to integrate optical depth
    float3 L = 0.0f;
    float3 throughput = 1.0;
    float3 opticalDepth = 0.0;
    float t = 0.0f;
    float tPrev = 0.0;
    const float sampleSegmentT = 0.3f;
    for (float s = 0.0f; s < sampleCount; s += 1.0f) {
      if (variableSampleCount) {
        // More expenssive but artefact free
        float t0 = (s) / sampleCountFloor;
        float t1 = (s + 1.0f) / sampleCountFloor;
        // Non linear distribution of sample within the range.
        t0 = t0 * t0;
        t1 = t1 * t1;
        // Make t0 and t1 world space distances.
        t0 = tMaxFloor * t0;
        if (t1 > 1.0) {
          t1 = tMax;
          //	t1 = tMaxFloor;	// this reveal depth slices
        }
        else {
          t1 = tMaxFloor * t1;
        }

        // Reduce sample count with noise and Temporal AA:
        if (perPixelNoise) {
#if 0
          //t = t0 + (t1 - t0) * InterleavedGradientNoise(pixelPosition, GetFrame().frame_count % 16);
          t = t0 + (t1 - t0) * blue_noise(pixelPosition).x;
#endif
        }
        else {
          t = t0 + (t1 - t0) * sampleSegmentT;
        }

        dt = t1 - t0;
      }
      else {
        //t = tMax * (s + SampleSegmentT) / SampleCount;            
        // Exact difference, important for accuracy of multiple scattering
        float newT = tMax * (s + sampleSegmentT) / sampleCount;
        dt = newT - t;
        t = newT;
      }
      float3 P = worldPosition + t * worldDirection;
      float pHeight = length(P);

      MediumSampleRGB medium = SampleMediumRGB(atmosphereParameters, P);
      const float3 sampleOpticalDepth = medium.extinction * dt * opticalDepthScale;
      const float3 sampleTransmittance = exp(-sampleOpticalDepth);
      opticalDepth += sampleOpticalDepth;

      const float3 UpVector = P / pHeight;
      float sunZenithCosAngle = dot(sunDirection, UpVector);
#ifdef HAS_TRANSMITTANCE_LUT
      float3 transmittanceToSun = GetTransmittance(atmosphereParameters.bottomRadius, atmosphereParameters.topRadius, pHeight, sunZenithCosAngle, transmittanceLutTexture);
#else
      float3 transmittanceToSun = float3(0.0f, 0.0f, 0.0f);
#endif

      float3 phaseTimesScattering;
      if (mieRayPhase) {
        phaseTimesScattering = medium.scatteringMie * miePhaseValue + medium.scatteringRay * rayleighPhaseValue;
      }
      else {
        phaseTimesScattering = medium.scattering * uniformPhase;
      }

      // Earth shadow 
      float tEarth = RaySphereIntersectNearest(P, sunDirection, earthO + PLANET_RADIUS_OFFSET * UpVector, atmosphereParameters.bottomRadius);
      float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

      // Volumetric cloud shadow
#if 0
      float3 planetCenter = float3(0.f, 0.f, 0.f); //float3(0.0f, -earthBottomRadius - 0.1f, 0.0f);
      float3 shadowP = P * SKY_UNIT_TO_M + atmosphere.planetCenter * SKY_UNIT_TO_M; 
      if (volumetricCloudShadow && GetFrame().options & OPTION_BIT_VOLUMETRICCLOUDS_CAST_SHADOW) {
        earthShadow *= shadow_2D_volumetricclouds(shadowP);
      }
#endif

      // Opaque shadow from cascade shadow mapping
#if 0
      if (opaqueShadow && validLight) {
        float3 shadow_pos = mul(load_entitymatrix(light.GetMatrixIndex() + furthestCascade), float4(shadowP, 1)).xyz; // ortho matrix, no divide by .w
        float3 shadow_uv = shadow_pos.xyz * float3(0.5f, -0.5f, 0.5f) + 0.5f;

        if (is_saturated(shadow_uv)) {
          earthShadow *= shadow_2D(light, shadow_pos, shadow_uv.xy, furthestCascade).r;
        }
      }
#endif

      // Dual scattering for multi scattering 
      float3 multiScatteredLuminance = 0.0f;
#ifdef HAS_MULTISCATTER_LUT
      if (multiScatteringApprox) {
        multiScatteredLuminance = GetMultipleScattering(atmosphereParameters, multiScatteringLUT, medium.scattering, medium.extinction, P, sunZenithCosAngle);
      }
#endif

      float3 S = globalL * (earthShadow * transmittanceToSun * phaseTimesScattering + multiScatteredLuminance * medium.scattering);

      // When using the power serie to accumulate all sattering order, serie r must be <1 for a serie to converge.
      // Under extreme coefficient, MultiScatAs1 can grow larger and thus result in broken visuals.
      // The way to fix that is to use a proper analytical integration as proposed in slide 28 of http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
      // However, it is possible to disable as it can also work using simple power serie sum unroll up to 5th order. The rest of the orders has a really low contribution.
#define MULTI_SCATTERING_POWER_SERIE 1

#if MULTI_SCATTERING_POWER_SERIE==0
        // 1 is the integration of luminance over the 4pi of a sphere, and assuming an isotropic phase function of 1.0/(4*PI)
            result.multiScatAs1 += throughput * medium.scattering * 1 * dt;
#else
      float3 MS = medium.scattering * 1;
      float3 MSint = (MS - MS * sampleTransmittance) / medium.extinction;
      result.multiScatAs1 += throughput * MSint;
#endif

      // Evaluate input to multi scattering 
      {
        float3 newMS;

        newMS = earthShadow * transmittanceToSun * medium.scattering * uniformPhase * 1;
        result.newMultiScatStep0Out += throughput * (newMS - newMS * sampleTransmittance) / medium.extinction;
        //	result.NewMultiScatStep0Out += SampleTransmittance * throughput * newMS * dt;

        newMS = medium.scattering * uniformPhase * multiScatteredLuminance;
        result.newMultiScatStep1Out += throughput * (newMS - newMS * sampleTransmittance) / medium.extinction;
        //	result.NewMultiScatStep1Out += SampleTransmittance * throughput * newMS * dt;
      }

#if 0
      L += throughput * S * dt;
      throughput *= SampleTransmittance;
#else
      // See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/ 
      float3 Sint = (S - S * sampleTransmittance) / medium.extinction; // integrate along the current step segment 
      L += throughput * Sint;                                          // accumulate and also take into account the transmittance from previous steps
      throughput *= sampleTransmittance;
#endif

      tPrev = t;
    }

#ifdef HAS_TRANSMITTANCE_LUT
    if (ground && tMax == tBottom && tBottom > 0.0) {
      // Account for bounced light off the earth
      float3 P = worldPosition + tBottom * worldDirection;
      float pHeight = length(P);

      const float3 UpVector = P / pHeight;
      float sunZenithCosAngle = dot(sunDirection, UpVector);
      float3 transmittanceToSun = GetTransmittance(atmosphereParameters.bottomRadius, atmosphereParameters.topRadius, pHeight, sunZenithCosAngle, transmittanceLutTexture);

      const float NdotL = saturate(dot(normalize(UpVector), normalize(sunDirection)));
      L += globalL * transmittanceToSun * throughput * NdotL * atmosphereParameters.groundAlbedo / PI;
    }
#endif

    result.L = L;
    result.opticalDepth = opticalDepth;
    result.transmittance = throughput;
  }
  return result;
}

float3 GetAtmosphereTransmittance(float3 worldPosition, float3 worldDirection, AtmosphereParameters atmosphere, Texture2D<float4> transmittanceLutTexture) {
  // If the worldDirection is occluded from this virtual planet, then return.
  // We do this due to the low resolution LUT, where the stored zenith to horizon never reaches black, to prevent linear interpolation artefacts.
  // At the most shadowed point of the LUT, pure black with earth shadow is never reached.
  float2 sol = RaySphereIntersect(worldPosition, worldDirection, float3(0.0f, 0.0f, 0.0f), atmosphere.bottomRadius);
  if (sol.x > 0.0f || sol.y > 0.0f) {
    return 0.0f;
  }

  const float pHeight = length(worldPosition);
  const float3 UpVector = worldPosition / pHeight;
  const float SunZenithCosAngle = dot(worldDirection, UpVector);

  float2 uv;
  LutTransmittanceParamsToUv(atmosphere.bottomRadius, atmosphere.topRadius, pHeight, SunZenithCosAngle, uv);

  float3 TransmittanceToSun = transmittanceLutTexture.SampleLevel(LINEAR_CLAMPED_SAMPLER, uv, 0).rgb;
  return TransmittanceToSun;
}

float3 GetSunLuminance(float3 worldPosition,
                       float3 worldDirection,
                       float3 sunDirection,
                       float3 sunIlluminance,
                       AtmosphereParameters atmosphere,
                       Texture2D<float4> transmittanceLutTexture) {
  //float sunApexAngleDegree = 0.545; // Angular diameter of sun to earth from sea level, see https://en.wikipedia.org/wiki/Solid_angle
  const float sunApexAngleDegree = 2.4; // Modified sun size
  const float sunHalfApexAngleRadian = 0.5 * sunApexAngleDegree * PI / 180.0;
  const float sunCosHalfApexAngle = cos(sunHalfApexAngleRadian);

  float3 retval = 0;

  const float t = RaySphereIntersectNearest(worldPosition, worldDirection, float3(0.0f, 0.0f, 0.0f), atmosphere.bottomRadius);
  // no intersection
  if (t < 0.0f) {
    const float VdotL = dot(worldDirection, normalize(sunDirection)); // weird... the sun disc shrinks near the horizon if we don't normalize sun direction
    if (VdotL > sunCosHalfApexAngle) {
      // Edge fade
      const float halfCosHalfApex = sunCosHalfApexAngle + (1.0f - sunCosHalfApexAngle) * 0.25; // Start fading when at 75% distance from light disk center
      const float weight = 1.0 - saturate((halfCosHalfApex - VdotL) / (halfCosHalfApex - sunCosHalfApexAngle));

      retval = weight * sunIlluminance;
    }

#if STARS // TODO
    if (GetWeather().stars > 0) {
      float3 stars_direction = mul(worldDirection, (float3x3)GetWeather().stars_rotation);
      float stars_visibility = pow(saturate(1 - sunDirection.y), 2);
      float stars_density_at_maximum = lerp(22, 8, GetWeather().stars);
      float stars_threshold = lerp(32, stars_density_at_maximum, stars_visibility); // modifies the number of stars that are visible
      float stars_exposure = lerp(0, 512, stars_visibility);                        // modifies the overall strength of the stars
      float stars = saturate(pow(noise_gradient_3D(stars_direction * 300), stars_threshold)) * stars_exposure;
      stars *= lerp(0.4, 1.4, noise_gradient_3D(stars_direction * 256 + GetTime())); // time based flickering
      retval += stars;
    }
#endif

    const float3 atmosphereTransmittance = GetAtmosphereTransmittance(worldPosition, worldDirection, atmosphere, transmittanceLutTexture);
    retval *= atmosphereTransmittance;
  }

  return retval;
}

#endif
