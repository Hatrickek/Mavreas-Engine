#ifndef PBRCOMMON_HLSLI
#define PBRCOMMON_HLSLI

#include "Globals.hlsli"
#define MIN_PERCEPTUAL_ROUGHNESS 0.045
#define MIN_ROUGHNESS 0.002025
#define MIN_N_DOT_V 1e-4

float3 F_Schlick(const float3 f0, float VoH) {
  // Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"
  float f90 = saturate(50.0 * dot(f0, 0.33)); // reflectance at grazing angle
  return f0 + (f90 - f0) * pow5(1.0 - VoH);
}

float3x3 GetNormalTangent(float3 worldPos, float3 normal, float2 uv) {
  float3 T = normalize(ddx(worldPos) * ddy(uv).y - ddy(worldPos) * ddx(uv).y);
  float3 B = -normalize(cross(normal, T));
  float3x3 TBN = float3x3(T, B, normal);

  return TBN;
}

int GetCascadeIndex(float4 cascadeSplits, float3 viewPosition, int cascadeCount) {
  int cascadeIndex = 0;
  for (int i = 0; i < cascadeCount - 1; ++i) {
    if (viewPosition.z < cascadeSplits[i]) {
      cascadeIndex = i + 1;
    }
  }
  return cascadeIndex;
}

float HardenedKernel(float a) {
  // this is basically a stronger smoothstep()
  float x = 2.0f * a - 1.0f;
  const float s = sign(x);
  x = 1.0f - s * x;
  x = x * x * x;
  x = s - x * s;
  return 0.5f * x + 0.5f;
}

// Poisson disk generated with 'poisson-disk-generator' tool from
// https://github.com/corporateshark/poisson-disk-generator by Sergey Kosarevsky
static const float2 PoissonDisk[64] =
  {float2(0.511749, 0.547686),     float2(0.58929, 0.257224),    float2(0.165018, 0.57663),     float2(0.407692, 0.742285),
   float2(0.707012, 0.646523),     float2(0.31463, 0.466825),    float2(0.801257, 0.485186),    float2(0.418136, 0.146517),
   float2(0.579889, 0.0368284),    float2(0.79801, 0.140114),    float2(-0.0413185, 0.371455),  float2(-0.0529108, 0.627352),
   float2(0.0821375, 0.882071),    float2(0.17308, 0.301207),    float2(-0.120452, 0.867216),   float2(0.371096, 0.916454),
   float2(-0.178381, 0.146101),    float2(-0.276489, 0.550525),  float2(0.12542, 0.126643),     float2(-0.296654, 0.286879),
   float2(0.261744, -0.00604975),  float2(-0.213417, 0.715776),  float2(0.425684, -0.153211),   float2(-0.480054, 0.321357),
   float2(-0.0717878, -0.0250567), float2(-0.328775, -0.169666), float2(-0.394923, 0.130802),   float2(-0.553681, -0.176777),
   float2(-0.722615, 0.120616),    float2(-0.693065, 0.309017),  float2(0.603193, 0.791471),    float2(-0.0754941, -0.297988),
   float2(0.109303, -0.156472),    float2(0.260605, -0.280111),  float2(0.129731, -0.487954),   float2(-0.537315, 0.520494),
   float2(-0.42758, 0.800607),     float2(0.77309, -0.0728102),  float2(0.908777, 0.328356),    float2(0.985341, 0.0759158),
   float2(0.947536, -0.11837),     float2(-0.103315, -0.610747), float2(0.337171, -0.584),      float2(0.210919, -0.720055),
   float2(0.41894, -0.36769),      float2(-0.254228, -0.49368),  float2(-0.428562, -0.404037),  float2(-0.831732, -0.189615),
   float2(-0.922642, 0.0888026),   float2(-0.865914, 0.427795),  float2(0.706117, -0.311662),   float2(0.545465, -0.520942),
   float2(-0.695738, 0.664492),    float2(0.389421, -0.899007),  float2(0.48842, -0.708054),    float2(0.760298, -0.62735),
   float2(-0.390788, -0.707388),   float2(-0.591046, -0.686721), float2(-0.769903, -0.413775),  float2(-0.604457, -0.502571),
   float2(-0.557234, 0.00451362),  float2(0.147572, -0.924353),  float2(-0.0662488, -0.892081), float2(0.863832, -0.407206)};

float GetPenumbraRatio(const bool directional, const int index, float z_receiver, float z_blocker) {
  // z_receiver/z_blocker are not linear depths (i.e. they're not distances)
  // Penumbra ratio for PCSS is given by:  pr = (d_receiver - d_blocker) / d_blocker
  float penumbraRatio;
  if (directional) {
    // TODO: take lispsm into account
    // For directional lights, the depths are linear but depend on the position (because of LiSPSM).
    // With:        z_linear = f + z * (n - f)
    // We get:      (r-b)/b ==> (f/(n-f) + r_linear) / (f/(n-f) + b_linear) - 1
    // Assuming f>>n and ignoring LISPSM, we get:
    penumbraRatio = (z_blocker - z_receiver) / (1.0 - z_blocker);
  } else {
    // For spotlights, the depths are congruent to 1/z, specifically:
    //      z_linear = (n * f) / (n + z * (f - n))
    // replacing in (r - b) / b gives:
    float nearOverFarMinusNear = 0.01f / (1000.f - 0.01f); // shadowUniforms.shadows[index].nearOverFarMinusNear; TODO: parameterize
    penumbraRatio = (nearOverFarMinusNear + z_blocker) / (nearOverFarMinusNear + z_receiver) - 1.0;
  }
  return penumbraRatio;
}

void BlockerSearchAndFilter(out float occludedCount,
                            out float z_occSum,
                            const Texture2DArray map,
                            const float4 scissorNormalized,
                            const float2 uv,
                            const float z_rec,
                            const uint layer,
                            const float2 filterRadii,
                            const float2x2 R,
                            const float2 dz_duv,
                            const uint tapCount) {
  occludedCount = 0.0f;
  z_occSum = 0.0f;

  for (uint i = 0u; i < tapCount; i++) {
    float2 duv = mul(R, (PoissonDisk[i] * filterRadii));
    float2 tc = clamp(uv + duv, scissorNormalized.xy, scissorNormalized.zw);

    float z_occ = map.Sample(NEAREST_CLAMPED_SAMPLER, float3(uv + duv, layer)).r;

    // note: z_occ and z_rec are not necessarily linear here, comparing them is always okay for
    // the regular PCF, but the "distance" is meaningless unless they are actually linear
    // (e.g.: for the directional light).
    // Either way, if we assume that all the samples are close to each other we can take their
    // average regardless, and the average depth value of the occluders
    // becomes: z_occSum / occludedCount.

    // receiver plane depth bias
    float z_bias = dot(dz_duv, duv);
    float dz = z_occ - z_rec; // dz>0 when blocker is between receiver and light
    float occluded = step(z_bias, dz);
    occludedCount += occluded;
    z_occSum += z_occ * occluded;
  }
}

float interleavedGradientNoise(float2 w) {
  const float3 m = float3(0.06711056f, 0.00583715f, 52.9829189f);
  return frac(m.z * frac(dot(w, m.xy)));
}

float2x2 GetRandomRotationMatrix(float2 fragCoord) {
  // rotate the poisson disk randomly
  float randomAngle = interleavedGradientNoise(fragCoord) * (2.0f * PI);
  float2 randomBase = float2(cos(randomAngle), sin(randomAngle));
  float2x2 R = float2x2(randomBase.x, randomBase.y, -randomBase.y, randomBase.x);
  return R;
}

float GetPenumbraLs(const bool DIRECTIONAL, const int index, const float zLight) {
  float penumbra;
  // This conditional is resolved at compile time
  if (DIRECTIONAL) {
    penumbra = 3.0f;
    // penumbra = shadowUniforms.shadows[index].bulbRadiusLs; TODO:
  } else {
    penumbra = 3.0f;
    // the penumbra radius depends on the light-space z for spotlights
    // penumbra = shadowUniforms.shadows[index].bulbRadiusLs / zLight; TODO:
  }
  return penumbra;
}

#define DPCF_SHADOW_TAP_COUNT 12

float2 ComputeReceiverPlaneDepthBias(const float3 position) {
  // see: GDC '06: Shadow Mapping: GPU-based Tips and Techniques
  // Chain rule to compute dz/du and dz/dv
  // |dz/du|   |du/dx du/dy|^-T   |dz/dx|
  // |dz/dv| = |dv/dx dv/dy|    * |dz/dy|
  float3 duvz_dx = ddx(position);
  float3 duvz_dy = ddy(position);
  float2 dz_duv = mul(inverse(transpose(float2x2(duvz_dx.xy, duvz_dy.xy))), float2(duvz_dx.z, duvz_dy.z));
  return dz_duv;
}

float ShadowSample_DPCF(const float2 pixelPosition,
                        const bool directional,
                        const Texture2DArray<> map,
                        const float4 scissorNormalized,
                        const uint layer,
                        const int index,
                        const float4 shadowPosition) {
  int width, height, element, numlevels;
  map.GetDimensions(0, width, height, element, numlevels);
  float2 texelSize = float2(1.0f, 1.0f) / float2(width, height);
  float3 position = shadowPosition.xyz * (1.0f / shadowPosition.w);

  float penumbra = GetPenumbraLs(directional, index, 0.0f);

  float2x2 R = GetRandomRotationMatrix(pixelPosition.xy);

  float2 dz_duv = ComputeReceiverPlaneDepthBias(position);

  float occludedCount = 0.0f;
  float z_occSum = 0.0f;

  BlockerSearchAndFilter(occludedCount,
                         z_occSum,
                         map,
                         scissorNormalized,
                         position.xy,
                         position.z,
                         layer,
                         texelSize * penumbra,
                         R,
                         dz_duv,
                         DPCF_SHADOW_TAP_COUNT);

  // early exit if there is no occluders at all, also avoids a divide-by-zero below.
  if (z_occSum == 0.0f) {
    return 1.0f;
  }

  float penumbraRatio = GetPenumbraRatio(directional, index, position.z, z_occSum / occludedCount);
  penumbraRatio = saturate(penumbraRatio);

  float percentageOccluded = occludedCount * (1.0 / float(DPCF_SHADOW_TAP_COUNT));

  percentageOccluded = lerp(HardenedKernel(percentageOccluded), percentageOccluded, penumbraRatio);
  return 1.0f - percentageOccluded;
}

float Shadow(float2 pixelPosition,
             const bool directional,
             const Texture2DArray shadowMap,
             const int index,
             float4 shadowPosition,
             float4 scissorNormalized) {
  uint layer = index;
  return ShadowSample_DPCF(pixelPosition, directional, shadowMap, scissorNormalized, layer, index, shadowPosition);
}

inline float3 sample_shadow(float2 uv, float cmp) {
  Texture2D texture_shadowatlas = get_shadow_atlas();
  float3 shadow = texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp).r;

#ifndef DISABLE_SOFT_SHADOWMAP
  // sample along a rectangle pattern around center:
  shadow.x += texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp, int2(-1, -1)).r;
  shadow.x += texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp, int2(-1, 0)).r;
  shadow.x += texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp, int2(-1, 1)).r;
  shadow.x += texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp, int2(0, -1)).r;
  shadow.x += texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp, int2(0, 1)).r;
  shadow.x += texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp, int2(1, -1)).r;
  shadow.x += texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp, int2(1, 0)).r;
  shadow.x += texture_shadowatlas.SampleCmpLevelZero(CMP_DEPTH_SAMPLER, uv, cmp, int2(1, 1)).r;
  shadow = shadow.xxx / 9.0;
#endif // DISABLE_SOFT_SHADOWMAP

#if 0  // DISABLE_TRANSPARENT_SHADOWMAP
  if (transparent_shadows) {
    Texture2D texture_shadowatlas_transparent = GetShadowTransparentAtlas();
    float4 transparent_shadow = texture_shadowatlas_transparent.SampleLevel(LINEAR_CLAMPED_SAMPLER, uv, 0);
  #ifdef TRANSPARENT_SHADOWMAP_SECONDARY_DEPTH_CHECK
		if (transparent_shadow.a > cmp)
  #endif
    {
      shadow *= transparent_shadow.rgb;
    }
  }
#endif // DISABLE_TRANSPARENT_SHADOWMAP

  return shadow;
}

// This is used to clamp the uvs to last texel center to avoid sampling on the border and overfiltering into a different shadow
inline void shadow_border_shrink(in Light light, inout float2 shadow_uv) {
  const float2 shadow_resolution = light.shadow_atlas_mul_add.unpack().xy * get_scene().shadow_atlas_res.unpack();
#ifdef DISABLE_SOFT_SHADOWMAP
  const float border_size = 0.5;
#else
  const float border_size = 1.5;
#endif // DISABLE_SOFT_SHADOWMAP
  shadow_uv = clamp(shadow_uv * shadow_resolution, border_size, shadow_resolution - border_size) / shadow_resolution;
}

inline float3 shadow_2D(in Light light, in float3 shadow_pos, in float2 shadow_uv, in uint cascade) {
  shadow_border_shrink(light, shadow_uv);
  shadow_uv.x += cascade;
  shadow_uv = mad(shadow_uv, light.shadow_atlas_mul_add.unpack().xy, light.shadow_atlas_mul_add.unpack().zw);
  return sample_shadow(shadow_uv, shadow_pos.z);
}

inline float3 shadow_cube(in Light light, in float3 Lunnormalized) {
  const float remapped_distance = light.get_cubemap_depth_remap_near() +
                                  light.get_cubemap_depth_remap_far() / (max(max(abs(Lunnormalized.x), abs(Lunnormalized.y)), abs(Lunnormalized.z)) *
                                                                         0.989); // little bias to avoid artifact
  const float3 uv_slice = cubemap_to_uv(-Lunnormalized);
  float2 shadow_uv = uv_slice.xy;
  shadow_border_shrink(light, shadow_uv);
  shadow_uv.x += uv_slice.z;
  shadow_uv = mad(shadow_uv, light.shadow_atlas_mul_add.unpack().xy, light.shadow_atlas_mul_add.unpack().zw);
  return sample_shadow(shadow_uv, remapped_distance);
}

#endif
