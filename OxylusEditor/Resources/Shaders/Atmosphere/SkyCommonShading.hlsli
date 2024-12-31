#ifndef SKYCOMMONSHADING_HLSLI
#define SKYCOMMONSHADING_HLSLI

#define HAS_TRANSMITTANCE_LUT
#define HAS_MULTISCATTER_LUT
#include "SkyCommon.hlsli"

float3 AccurateAtmosphericScattering(float2 pixelPosition,
                                     float3 rayOrigin,
                                     float3 rayDirection,
                                     float3 sunDirection,
                                     float3 sunColor,
                                     bool enableSun,
                                     bool darkMode,
                                     bool stationary,
                                     bool highQuality,
                                     bool perPixelNoise,
                                     bool receiveShadow) {
  AtmosphereParameters atmosphere;
  InitAtmosphereParameters(atmosphere);

  const float3 skyRelativePosition = stationary ? float3(0.00001f, 0.00001f, 0.00001f) : rayOrigin;

  float3 worldPosition = GetCameraPlanetPos(atmosphere, skyRelativePosition);
  const float3 worldDirection = rayDirection;

  float3 luminance = 0;
  // Move to top atmosphere as the starting point for ray marching.
  // This is critical to be after the above to not disrupt above atmosphere tests and voxel selection.
  if (MoveToTopAtmosphere(worldPosition, worldDirection, atmosphere.topRadius)) {
    // Apply the start offset after moving to the top of atmosphere to avoid black pixels
    worldPosition += worldDirection * AP_START_OFFSET_KM;

    const float3 sunIlluminance = sunColor;

    const float tDepth = 0.0;
    const float sampleCountIni = 0.0;
    const bool variableSampleCount = true;
    const bool opaque = false;
    const bool ground = false; // TODO: parameterize
    const bool mieRayPhase = true;
    const bool multiScatteringApprox = true;
    const bool volumetricCloudShadow = receiveShadow;
    const bool opaqueShadow = receiveShadow;
    const SingleScatteringResult ss = IntegrateScatteredLuminance(
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
      opaqueShadow,
      get_sky_transmittance_lut_texture(),
      get_sky_multi_scatter_lut_texture()
    );

    luminance = ss.L;
  }

  float3 totalColor;

  if (enableSun) {
    const float3 sunIlluminance = sunColor;
    totalColor = luminance + GetSunLuminance(worldPosition, worldDirection, sunDirection, sunIlluminance, atmosphere, get_sky_transmittance_lut_texture());
  }
  else {
    totalColor = luminance; // We cant really seperate mie from luminance due to precomputation, todo?
  }

  if (darkMode) {
    totalColor = max(pow(saturate(dot(sunDirection, rayDirection)), 64) * sunColor, 0) * luminance * 1.0;
  }

#if HEIGHT_FOG // TODO
  if (GetFrame().options & OPTION_BIT_HEIGHT_FOG) {
    const float3 planet_center = atmosphere.planetCenter * SKY_UNIT_TO_M;
    const float bottom_radius = atmosphere.bottomRadius * SKY_UNIT_TO_M;
    const float top_radius = bottom_radius + GetWeather().fog.height_end;
    float dist = RaySphereIntersectNearest(rayOrigin, rayDirection, planet_center, top_radius);

    if (dist >= 0) {
      // Offset origin with fog start value.
      // We can't do this with normal distance due to infinite distance.
      const float3 offsetO = rayOrigin + rayDirection * GetWeather().fog.start;
      float4 fog = GetFog(dist, offsetO, rayDirection);
      if (fog.a > 0) // this check avoids switching to fully fogged above fog level camera at zero density
      {
        if (length(rayOrigin - planet_center) > top_radius) // check if we are above fog height sphere
        {
          // hack: flip fog when camera is above
          fog.a = 1 - fog.a; // this only supports non-premultiplied fog
        }
        totalColor = lerp(totalColor, fog.rgb, fog.a); // non-premultiplied fog
      }
    }
  }
#endif

  return totalColor;
}

float3 GetDynamicSkyColor(in float2 pixel, in float3 V, bool sunEnabled = true, bool darkEnabled = false, bool stationary = false, bool highQuality = false, bool perPixelNoise = false, bool receiveShadow = false) {
  float3 sky = AccurateAtmosphericScattering(
    pixel,
    get_camera().position, // Ray origin
    V, // Ray direction
    get_scene().sun_direction.unpack(), // Position of the sun
    get_scene().sun_color.unpack(), // Sun Color
    sunEnabled, // Use sun and total
    darkEnabled, // Enable dark mode for light shafts etc.
    stationary, // Fixed position for ambient and environment capture.
    highQuality, // Skip color lookup from lut
    perPixelNoise, // Vary sampling position with TAA
    receiveShadow // Atmosphere to use pre-rendered shadow data
  );

  sky *= 1.0f; //GetScene().sky_exposure;

  return sky;
}

float3 GetAtmosphericLightTransmittance(AtmosphereParameters atmosphere,
                                        float3 worldPosition,
                                        float3 worldDirection,
                                        Texture2D<float4> transmittanceLutTexture) {
  const float3 planetCenterWorld = atmosphere.planetCenter * SKY_UNIT_TO_M;
  const float3 planetCenterToWorldPos = (worldPosition - planetCenterWorld) * M_TO_SKY_UNIT;

  float3 atmosphereTransmittance = GetAtmosphereTransmittance(planetCenterToWorldPos, worldDirection, atmosphere, transmittanceLutTexture);
  return atmosphereTransmittance;
}

#endif
