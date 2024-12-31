#include "Atmosphere/SkyCommonShading.hlsli"
#include "PBRCommon.hlsli"

float D_GGX(float roughness, float NoH, const float3 h) {
  // Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces"
  float oneMinusNoHSquared = 1.0 - NoH * NoH;

  float a = NoH * roughness;
  float k = roughness / (oneMinusNoHSquared + a * a);
  float d = k * k * (1.0 / PI);
  return saturateMediump(d);
}

float V_SmithGGXCorrelated(float roughness, float NoV, float NoL) {
  // Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"
  float a2 = roughness * roughness;
  // TODO: lambdaV can be pre-computed for all the lights, it should be moved out of this function
  float lambdaV = NoL * sqrt((NoV - a2 * NoV) * NoV + a2);
  float lambdaL = NoV * sqrt((NoL - a2 * NoL) * NoL + a2);
  float v = 0.5 / (lambdaV + lambdaL);
  // a2=0 => v = 1 / 4*NoL*NoV   => min=1/4, max=+inf
  // a2=1 => v = 1 / 2*(NoL+NoV) => min=1/4, max=+inf
  // clamp to the maximum value representable in mediump
  return saturateMediump(v);
}

// https://www.unrealengine.com/en-US/blog/physically-based-shading-on-mobile
half3 EnvBRDFApprox(half3 SpecularColor, half Roughness, half NoV) {
  const half4 c0 = {-1, -0.0275, -0.572, 0.022};
  const half4 c1 = {1, 0.0425, 1.04, -0.04};
  half4 r = Roughness * c0 + c1;
  half a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
  half2 AB = half2(-1.04, 1.04) * a004 + r.zw;
  return SpecularColor * AB.x + AB.y;
}

struct AnisotropicSurface {
  float2 direction;
  float strength;
  float3 T;

  // computed values:
  float at;
  float ab;
  float3 B;
  float TdotV;
  float BdotV;
};

struct Surface {
  float4 BaseColor;
  float3 Albedo;
  float Metallic;
  float Roughness;
  float Occlusion; // occlusion [0 -> 1]
  float4 Refraction;
  float3 EmissiveColor;
  float3 P;        // world space position
  float3 VP;       // view space position
  float3 N;        // world space normal
  float3 V;        // world space view vector
  float4 T;        // tangent
  float3 B;        // bitangent
  float H;
  float3 F0;       // fresnel value
  float2 PixelPosition;
  float Opacity;   // opacity for blending operation [0 -> 1]
  float3 ViewPos;
  float3 BumpColor;

#ifdef SHEEN
  SheenSurface sheen;
#endif // SHEEN

#ifdef CLEARCOAT
  ClearcoatSurface clearcoat;
#endif // CLEARCOAT

#ifdef ANISOTROPIC
  AnisotropicSurface aniso;
#endif         // ANISOTROPIC

  float NdotV; // cos(angle between normal and view vector)
  float3 R;    // reflection vector
  float3 F;    // fresnel term computed from NdotV

  void init() {
    P = 0;
    V = 0;
    N = 0;
    BaseColor = 1;
    Albedo = 1;
    F0 = 0.0;
    Roughness = 1;
    Occlusion = 1;
    Opacity = 1;
    EmissiveColor = 0;
    Refraction = 0.0;
    T = 0;
    B = 0;
    BumpColor = 0;
  }

  void create(float4 baseColor, float3 surfaceMap) {
    BaseColor = baseColor;
    Opacity = baseColor.a; // TODO: UNUSED

    float4 specularMap = 1;
    F0 = specularMap.rgb * specularMap.a;

    // metallic roughness workflow
    Metallic = surfaceMap.r;
    Roughness = surfaceMap.g;
    Occlusion = surfaceMap.b;

    const float reflectance = 0.0f; // TODO:?
    Albedo = baseColor.rgb * (1.0f - max(reflectance, Metallic));
    F0 *= lerp(reflectance.xxx, baseColor.rgb, Metallic);
  }

  void update() {
    Roughness = saturate(Roughness);
    NdotV = saturate(dot(N, V) + 1e-5);
    F = EnvBRDFApprox(F0, Roughness, NdotV);
    R = reflect(-V, N);
    B = normalize(cross(T.xyz, N) * T.w); // Compute bitangent again after normal mapping
#ifdef SHEEN
    sheen.roughness = saturate(sheen.roughness);
    // Sheen energy compensation:
    // https://dassaultsystemes-technology.github.io/EnterprisePBRShadingModel/spec-2021x.md.html#figure_energy-compensation-sheen-e
    sheen.DFG = GetSheenLUT().SampleLevel(LINEAR_CLAMPED_SAMPLER, float2(NdotV, sheen.roughness), 0).r;
    sheen.albedoScaling = 1.0 - max3(sheen.color) * sheen.DFG;
#endif
#ifdef CLEARCOAT
    clearcoat.roughness = saturate(clearcoat.roughness);
    clearcoat.F = EnvBRDFApprox(f0, clearcoat.roughness, clearcoatNdotV);
    clearcoat.F *= clearcoat.factor;
    clearcoat.R = -reflect(V, clearcoat.N);
#endif
#ifdef ANISOTROPIC
    aniso.B = normalize(cross(N, aniso.T));
    aniso.TdotV = dot(aniso.T.xyz, V);
    aniso.BdotV = dot(aniso.B, V);
    float roughnessBRDF = sqr(clamp(roughness, 0.045, 1));
    aniso.at = max(0, roughnessBRDF * (1 + aniso.strength));
    aniso.ab = max(0, roughnessBRDF * (1 - aniso.strength));
#endif // ANISOTROPIC
  }
};

struct SurfaceToLight {
  float3 L;    // surface to light vector (normalized)
  float3 H;    // half-vector between view vector and light vector
  float NdotL; // cos angle between normal and light direction
  float NdotH; // cos angle between normal and half vector
  float LdotH; // cos angle between light direction and half vector
  float VdotH; // cos angle between view direction and half vector
  float3 F;    // fresnel term computed from VdotH

#ifdef ANISOTROPIC
  float TdotL;
  float BdotL;
  float TdotH;
  float BdotH;
#endif // ANISOTROPIC

  void create(Surface surface, float3 Lnormalized) {
    L = Lnormalized;
    H = normalize(L + surface.V);

    NdotL = dot(L, surface.N);

#ifdef BRDF_NDOTL_BIAS
    NdotL += BRDF_NDOTL_BIAS;
#endif // BRDF_NDOTL_BIAS

    NdotH = saturate(dot(surface.N, H));
    LdotH = saturate(dot(L, H));
    VdotH = saturate(dot(surface.V, H));

    F = F_Schlick(surface.F0, VdotH);

#ifdef ANISOTROPIC
    TdotL = dot(surface.aniso.T.xyz, L);
    BdotL = dot(surface.aniso.B, L);
    TdotH = dot(surface.aniso.T.xyz, H);
    BdotH = dot(surface.aniso.B, H);
#endif

    NdotL = saturate(NdotL);
  }
};

float3 BRDF_GetSpecular(Surface surface, SurfaceToLight surface_to_light) {
#ifdef ANISOTROPIC
  float D = D_GGX_Anisotropic(surface.aniso.at, surface.aniso.ab, surface_to_light.TdotH, surface_to_light.BdotH, surface_to_light.NdotH);
  float Vis = V_SmithGGXCorrelated_Anisotropic(surface.aniso.at,
                                               surface.aniso.ab,
                                               surface.aniso.TdotV,
                                               surface.aniso.BdotV,
                                               surface_to_light.TdotL,
                                               surface_to_light.BdotL,
                                               surface.NdotV,
                                               surface_to_light.NdotL);
#else
  float roughnessBRDF = sqr(clamp(surface.Roughness, 0.045f, 1.0f));
  float D = D_GGX(roughnessBRDF, surface_to_light.NdotH, surface_to_light.H);
  float Vis = V_SmithGGXCorrelated(roughnessBRDF, surface.NdotV, surface_to_light.NdotL);
#endif

  float3 specular = D * Vis * surface_to_light.F;

#ifdef SHEEN
  specular *= surface.sheen.albedoScaling;
  float sheen_roughnessBRDF = sqr(clamp(surface.sheen.roughness, 0.045, 1));
  D = D_Charlie(sheen_roughnessBRDF, surface_to_light.NdotH);
  Vis = V_Neubelt(surface.NdotV, surface_to_light.NdotL);
  specular += D * Vis * surface.sheen.color;
#endif

#ifdef CLEARCOAT
  specular *= 1 - surface.clearcoat.F;
  float NdotH = saturate(dot(surface.clearcoat.N, surface_to_light.H));
  float clearcoat_roughnessBRDF = sqr(clamp(surface.clearcoat.roughness, 0.045, 1));
  D = D_GGX(clearcoat_roughnessBRDF, NdotH, surface_to_light.H);
  Vis = V_Kelemen(surface_to_light.LdotH);
  specular += D * Vis * surface.clearcoat.F;
#endif

  return specular * surface_to_light.NdotL;
}

float3 BRDF_GetDiffuse(Surface surface, SurfaceToLight surface_to_light) {
  float3 diffuse = (1 - lerp(surface_to_light.F, 0, saturate(0.0f)));

#ifdef SHEEN
  diffuse *= surface.sheen.albedoScaling;
#endif // SHEEN

#ifdef CLEARCOAT
  diffuse *= 1 - surface.clearcoat.F;
#endif // CLEARCOAT

  return diffuse * surface_to_light.NdotL;
}

struct LightingPart {
  float3 diffuse;
  float3 specular;
};

struct Lighting {
  LightingPart direct;
  LightingPart indirect;

  void create(in float3 diffuseDirect, in float3 specularDirect, in float3 diffuseIndirect, in float3 specularIndirect) {
    direct.diffuse = diffuseDirect;
    direct.specular = specularDirect;
    indirect.diffuse = diffuseIndirect;
    indirect.specular = specularIndirect;
  }
};

inline void apply_lighting(Surface surface, Lighting lighting, inout float4 color) {
  const float3 diffuse = lighting.direct.diffuse / PI + lighting.indirect.diffuse * (1 - surface.F) * surface.Occlusion;
  // reminder: cannot apply surface.F for whole indirect specular, because multiple layers have separate fresnels (sheen, clearcoat)
  const float3 specular = lighting.direct.specular + lighting.indirect.specular * surface.Occlusion;
  color.rgb = lerp(surface.Albedo * diffuse, surface.Refraction.rgb, surface.Refraction.a);
  color.rgb += specular;
  color.rgb += surface.EmissiveColor;
}

void light_directional(Light light, Surface surface, inout Lighting lighting) {
  float3 L = normalize(light.get_direction());

  SurfaceToLight surfaceToLight = (SurfaceToLight)0;
  surfaceToLight.create(surface, L);

  float3 shadow = 1;

  for (uint cascade = 0; cascade < light.get_shadow_cascade_count(); ++cascade) {
    // Project into shadow map space (no need to divide by .w because ortho projection!):
    float3 shadow_pos = mul(get_entity(light.matrix_index + cascade).transform, float4(surface.P, 1)).xyz;
    float3 shadow_uv = clipspace_to_uv(shadow_pos);

    // Determine if pixel is inside current cascade bounds and compute shadow if it is:
    if (is_saturated(shadow_uv)) {
      const float3 shadow_main = shadow_2D(light, shadow_pos, shadow_uv.xy, cascade);
      const float3 cascade_edgefactor = saturate(saturate(abs(shadow_pos)) - 0.8) * 5.0; // fade will be on edge and inwards 20%
      const float cascade_fade = max(cascade_edgefactor.x, max(cascade_edgefactor.y, cascade_edgefactor.z));

      // If we are on cascade edge threshold and not the last cascade, then fallback to a larger cascade:
      if (cascade_fade > 0 && cascade < light.get_shadow_cascade_count() - 1) {
        // Project into next shadow cascade (no need to divide by .w because ortho projection!):
        cascade += 1;
        shadow_pos = mul(get_entity(light.matrix_index + cascade).transform, float4(surface.P, 1)).xyz;
        shadow_uv = clipspace_to_uv(shadow_pos);
        const float3 shadow_fallback = shadow_2D(light, shadow_pos, shadow_uv.xy, cascade);

        shadow *= lerp(shadow_main, shadow_fallback, cascade_fade);
      } else {
        shadow *= shadow_main;
      }
      break;
    }
  }

  if (any(shadow)) {
    float3 light_color = light.get_color().rgb * shadow;

    AtmosphereParameters atmosphere;
    InitAtmosphereParameters(atmosphere);
    light_color *= GetAtmosphericLightTransmittance(atmosphere, surface.P.xyz, L, get_sky_transmittance_lut_texture());

    lighting.direct.diffuse = mad(light_color, BRDF_GetDiffuse(surface, surfaceToLight), lighting.direct.diffuse);
    lighting.direct.specular = mad(light_color, BRDF_GetSpecular(surface, surfaceToLight), lighting.direct.specular);
  }
}

float attenuation_point_light(in float dist, in float dist2, in float range, in float range2) {
  // GLTF recommendation: https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#range-property
  // return saturate(1 - pow(dist / range, 4)) / dist2;

  // Removed pow(x, 4), and avoid zero divisions:
  float dist_per_range = dist2 / max(0.0001, range2); // pow2
  dist_per_range *= dist_per_range;                   // pow4
  return saturate(1 - dist_per_range) / max(0.0001, dist2);
}

void light_point(Light light, Surface surface, inout Lighting lighting) {
  float3 L = light.get_position() - surface.P;

  if (light.get_length() > 0) {
    // Diffuse representative point on line:
    const float3 line_point = closest_point_on_segment(light.get_position() - light.get_direction() * light.get_length() * 0.5,
                                                       light.get_position() + light.get_direction() * light.get_length() * 0.5,
                                                       surface.P);
    L = line_point - surface.P;
  }

  const float dist2 = dot(L, L);
  const float range = light.get_range();
  const float range2 = range * range;

  if (dist2 < range2) {
    const float dist = sqrt(dist2);
    L /= dist;

    SurfaceToLight surface_to_light = (SurfaceToLight)0;
    surface_to_light.create(surface, L);

    if (any(surface_to_light.NdotL)) {
      float3 shadow = 1;

      shadow *= shadow_cube(light, light.get_position() - surface.P);

      if (any(shadow)) {
        float3 light_color = light.get_color() * shadow;
        light_color *= attenuation_point_light(dist, dist2, range, range2);

        lighting.direct.diffuse = mad(light_color, BRDF_GetDiffuse(surface, surface_to_light), lighting.direct.diffuse);

        if (light.get_length() > 0) {
          // Specular representative point on line:
          float3 P0 = light.get_position() - light.get_direction() * light.get_length() * 0.5;
          float3 P1 = light.get_position() + light.get_direction() * light.get_length() * 0.5;
          float3 L0 = P0 - surface.P;
          float3 L1 = P1 - surface.P;
          float3 Ld = L1 - L0;
          float RdotLd = dot(surface.R, Ld);
          float t = dot(surface.R, L0) * RdotLd - dot(L0, Ld);
          t /= dot(Ld, Ld) - RdotLd * RdotLd;
          L = (L0 + saturate(t) * Ld);
        } else {
          L = light.get_position() - surface.P;
        }
        if (light.get_radius() > 0) {
          // Specular representative point on sphere:
          const float3 center_to_ray = mad(dot(L, surface.R), surface.R, -L);
          L = mad(center_to_ray, saturate(light.get_radius() / length(center_to_ray)), L);
          // Energy conservation for radius:
          light_color /= max(1, sphere_volume(light.get_radius()));
        }
        if (light.get_length() > 0 || light.get_radius() > 0) {
          L = normalize(L);
          surface_to_light.create(surface, L); // recompute all surface-light vectors
        }

        lighting.direct.specular = mad(light_color, BRDF_GetSpecular(surface, surface_to_light), lighting.direct.specular);
      }
    }
  }
}

inline float attenuation_spot_light(in float dist,
                                    in float dist2,
                                    in float range,
                                    in float range2,
                                    in float spot_factor,
                                    in float angle_scale,
                                    in float angle_offset) {
  float attenuation = attenuation_point_light(dist, dist2, range, range2);
  float angularAttenuation = saturate(mad(spot_factor, angle_scale, angle_offset));
  angularAttenuation *= angularAttenuation;
  attenuation *= angularAttenuation;
  return attenuation;
}

inline void light_spot(Light light, Surface surface, inout Lighting lighting) {
  float3 L = light.get_position() - surface.P;
  const float dist2 = dot(L, L);
  const float range = light.get_range();
  const float range2 = range * range;

  if (dist2 < range2) {
    const float dist = sqrt(dist2);
    L /= dist;

    SurfaceToLight surface_to_light = (SurfaceToLight)0;
    surface_to_light.create(surface, L);

    if (any(surface_to_light.NdotL)) {
      const float spot_factor = dot(L, light.get_direction());
      const float spot_cutoff = light.get_cone_angle_cos();

      if (spot_factor > spot_cutoff) {
        float3 shadow = 1;

        float4 shadow_pos = mul(get_entity(light.matrix_index).transform, float4(surface.P, 1));
        shadow_pos.xyz /= shadow_pos.w;
        float2 shadow_uv = clipspace_to_uv(shadow_pos.xy);
        if (is_saturated(shadow_uv)) {
          shadow *= shadow_2D(light, shadow_pos.xyz, shadow_uv.xy, 0);
        }

        if (any(shadow)) {
          float3 light_color = light.get_color().rgb * shadow;
          light_color *= attenuation_spot_light(dist, dist2, range, range2, spot_factor, light.get_angle_scale(), light.get_angle_offset());

          lighting.direct.diffuse = mad(light_color, BRDF_GetDiffuse(surface, surface_to_light), lighting.direct.diffuse);

          if (light.get_radius() > 0) {
            // Specular representative point on sphere:
            L = light.get_position() - surface.P;
            float3 centerToRay = mad(dot(L, surface.R), surface.R, -L);
            L = mad(centerToRay, saturate(light.get_radius() / length(centerToRay)), L);
            L = normalize(L);
            surface_to_light.create(surface, L); // recompute all surface-light vectors
                                                 // Energy conservation for radius:
            light_color /= max(1, sphere_volume(light.get_radius()));
          }

          lighting.direct.specular = mad(light_color, BRDF_GetSpecular(surface, surface_to_light), lighting.direct.specular);
        }
      }
    }
  }
}

inline void forward_lighting(inout Surface surface, inout Lighting lighting) {
  for (int i = 0; i < get_scene().num_lights; i++) {
    Light light = get_light(i);

    switch (light.get_type()) {
      case DIRECTIONAL_LIGHT: {
        light_directional(light, surface, lighting);
      } break;
      case POINT_LIGHT: {
        light_point(light, surface, lighting);
      } break;
      case SPOT_LIGHT: {
        light_spot(light, surface, lighting);
      } break;
    }
  }
}

float geometric_occlusion(Surface pixelData, float NoL) {
  float NdotL = NoL;
  float NdotV = pixelData.NdotV;
  float r = pixelData.Roughness * pixelData.Roughness;

  float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r * r + (1.0 - r * r) * (NdotL * NdotL)));
  float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r * r + (1.0 - r * r) * (NdotV * NdotV)));
  return attenuationL * attenuationV;
}

float3 get_ambient(float3 worldNormal) {
  // Set realistic_sky_stationary to true so we capture ambient at float3(0.0, 0.0, 0.0), similar to the standard sky to avoid flickering and weird
  // behavior
  float3 ambient = lerp(GetDynamicSkyColor(float2(0.0f, 0.0f), float3(0, -1, 0), false, false, true),
                        GetDynamicSkyColor(float2(0.0f, 0.0f), float3(0, 1, 0), false, false, true),
                        saturate(worldNormal.y * 0.5 + 0.5));

  ambient += float3(0.19608, 0.19608, 0.19608); // GetScene().AmbientColor;

  return ambient;
}

struct VOut {
  float4 position : SV_Position;
  float2 uv : TEXCOORD;
};

float4 PSmain(VOut input, float4 pixelPosition : SV_Position) : SV_Target {
  const float3 albedo = get_albedo_texture().Load(float3(pixelPosition.xy, 0)).rgb;
  const float4 normal_oct_and_flat_normal_oct = get_normal_texture().Load(float3(pixelPosition.xy, 0)).rgba;
  float3 mapped_normal = oct_to_vec3(normal_oct_and_flat_normal_oct.xy);
  // UNUSED... const float3 flat_normal = oct_to_vec3(normal_oct_and_flat_normal_oct.zw);
  const float3 smooth_normal = oct_to_vec3(get_normal_vertex_texture().Load(float3(pixelPosition.xy, 0)).rg);
  const float depth = get_depth_texture().Load(float3(pixelPosition.xy, 0)).r;
  const float3 emissive = get_emission_texture().Load(float3(pixelPosition.xy, 0)).rgb;
  const float3 metallic_roughness_ao = get_metallic_roughness_ao_texture().Load(float3(pixelPosition.xy, 0)).rgb;

  if (depth == 0.0f) {
    discard;
  }

  const float3 normal = normalize(lerp(smooth_normal, mapped_normal, length(mapped_normal)));

  Surface surface = (Surface)0;
  surface.init();
  surface.P = unproject_uv_zo(depth, input.uv, get_camera(0).inv_projection_view); // world pos
  surface.N = normal;
  surface.V = get_camera().position.xyz - surface.P;
  surface.V /= length(surface.V);
  surface.PixelPosition = pixelPosition.xy;
  surface.EmissiveColor = emissive;
  surface.create(float4(albedo, 1.0f), metallic_roughness_ao);

  float2 screenCoords = pixelPosition.xy / float2(get_scene().screen_size.x, get_scene().screen_size.y);

#ifndef TRANSPARENT
  // Apply GTAO
  if (get_scene().post_processing_data.enable_gtao == 1) {
    uint value = get_gtao_texture().Sample(NEAREST_REPEATED_SAMPLER, screenCoords).r;
    float aoVisibility = value / 255.0;
    //surface.Occlusion *= aoVisibility;
  }
#endif

#ifdef ANISOTROPIC
  surface.aniso.strength = material.anisotropy_strength;
  surface.aniso.direction = float2(material.anisotropy_rotation_cos, material.anisotropy_rotation_sin);

  if (material.AnistropyMapID != INVALID_ID) {
    float2 anisotropyTexture = GetMaterialAnistropyTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rg * 2 - 1;
    surface.aniso.strength *= length(anisotropyTexture);
    surface.aniso
      .direction = mul(float2x2(surface.aniso.direction.x, surface.aniso.direction.y, -surface.aniso.direction.y, surface.aniso.direction.x),
                       normalize(anisotropyTexture));
  }

  surface.aniso.T = normalize(mul(TBN, float3(surface.aniso.direction, 0)));

#endif // ANISOTROPIC

#ifdef SHEEN
  surface.sheen.color = material.SheenColor;
  surface.sheen.roughness = material.SheenRoughness;

  if (material.SheenColorMapID != INVALID_ID) {
    surface.sheen.color = GetMaterialSheenColorTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rgb;
  }
  if (material.SheenRoughnessMapID != INVALID_ID) {
    surface.sheen.roughness = GetMaterialSheenRoughnessTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).a;
  }
#endif // SHEEN

#ifdef CLEARCOAT
  surface.clearcoat.factor = material.Clearcoat;
  surface.clearcoat.roughness = material.ClearcoatRoughness;
  surface.clearcoat.N = input.nor;

  if (material.ClearCoatMapID != INVALID_ID) {
    surface.clearcoat.factor = GetMaterialClearCoatTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).r;
  }
  if (material.ClearCoatRoughnessMapID != INVALID_ID) {
    surface.clearcoat.roughness = GetMaterialClearCoatRoughnessTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).g;
  }
  if (material.ClearCoatNormalMapID != INVALID_ID) {
    float3 clearcoatNormalMap = float3(GetMaterialClearCoatNormalTexture(material).Sample(LINEAR_REPEATED_SAMPLER, scaledUV).rg, 1);
    clearcoatNormalMap = clearcoatNormalMap * 2 - 1;
    surface.clearcoat.N = mul(clearcoatNormalMap, TBN);
  }

  surface.clearcoat.N = normalize(surface.clearcoat.N);

#endif // CLEARCOAT

  surface.update();

  float3 ambient = get_ambient(surface.N);

  TextureCube cubemap = get_sky_env_map_texture();
  uint2 dim;
  uint mips;
  cubemap.GetDimensions(0, dim.x, dim.y, mips);

  float mip = surface.Roughness * mips;
  float3 envColor = cubemap.SampleLevel(LINEAR_CLAMPED_SAMPLER, surface.R, mip).rgb * surface.F;

  Lighting lighting = (Lighting)0;
  lighting.create(0, 0, ambient, max(0, envColor));

  float4 color = surface.BaseColor;

  forward_lighting(surface, lighting);

#if 0
  if (GetScene().PostProcessingData.EnableSSR == 1) {
    float4 ssr = GetSSRTexture().SampleLevel(LINEAR_CLAMPED_SAMPLER, screenCoords, 0);
    lighting.indirect.specular = lerp(lighting.indirect.specular, ssr.rgb * surface.F, ssr.a);
  }
#endif

  apply_lighting(surface, lighting, color);

  color = clamp(color, 0, 65000);

  return color;

#if 0
  // Specular anti-aliasing
  {
    const float strength = 1.0f;
    const float maxRoughnessGain = 0.02f;
    float roughness2 = roughness * roughness;
    float3 dndu = ddx(pixelData.N);
    float3 dndv = ddy(pixelData.N);
    float variance = (dot(dndu, dndu) + dot(dndv, dndv));
    float kernelRoughness2 = min(variance * strength, maxRoughnessGain);
    float filteredRoughness2 = saturate(roughness2 + kernelRoughness2);
    pixelData.Roughness = sqrt(filteredRoughness2);
  }
#endif
}
