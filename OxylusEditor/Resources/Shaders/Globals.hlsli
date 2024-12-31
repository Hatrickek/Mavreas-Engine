#ifndef GLOBALS_HLSLI
#define GLOBALS_HLSLI

#include "Common.hlsli"
#include "Materials.hlsli"

// set 1
[[vk::binding(0, 1)]] ConstantBuffer<CameraCB> Camera;

// set 0
[[vk::binding(0, 0)]] StructuredBuffer<SceneData> Scene;

[[vk::binding(1, 0)]] ByteAddressBuffer Buffers[];
[[vk::binding(2, 0)]] RWByteAddressBuffer BuffersRW[];

[[vk::binding(3, 0)]] Texture2D<float4> SceneTextures[];
[[vk::binding(4, 0)]] Texture2D<float> SceneTexturesFloat[];
[[vk::binding(5, 0)]] Texture2D<uint> SceneTexturesUint[];
[[vk::binding(6, 0)]] TextureCube<float4> SceneCubeTextures[];
[[vk::binding(7, 0)]] Texture2DArray<float4> SceneArrayTextures[];
[[vk::binding(8, 0)]] RWTexture2D<float4> SceneRWTextures[];
[[vk::binding(9, 0)]] RWTexture2D<float> SceneRWTexturesFloat[];
[[vk::binding(10, 0)]] Texture2D<float4> MaterialTextureMaps[];

// 0 - linear_sampler_clamped
// 1 - linear_sampler_repeated
// 2 - linear_sampler_repeated_anisotropy
// 3 - nearest_sampler_clamped
// 4 - nearest_sampler_repeated
// 5 - hiz_sampler
[[vk::binding(11, 0)]] SamplerState Samplers[];
[[vk::binding(12, 0)]] SamplerComparisonState ComparisonSamplers[];

#define LINEAR_CLAMPED_SAMPLER Samplers[0]
#define LINEAR_REPEATED_SAMPLER Samplers[1]
#define LINEAR_REPEATED_SAMPLER_ANISOTROPY Samplers[2]
#define NEAREST_CLAMPED_SAMPLER Samplers[3]
#define NEAREST_REPEATED_SAMPLER Samplers[4]
#define HIZ_SAMPLER Samplers[5]

#define CMP_DEPTH_SAMPLER ComparisonSamplers[0]

SceneData get_scene() { return Scene.Load(0); }

CameraData get_camera(uint camera_index = 0) { return Camera.camera_data[camera_index]; }

// scene textures
Texture2D<float4> get_albedo_texture() { return SceneTextures[get_scene().indices_.albedo_image_index]; }
Texture2D<float4> get_normal_texture() { return SceneTextures[get_scene().indices_.normal_image_index]; }
Texture2D<float4> get_normal_vertex_texture() { return SceneTextures[get_scene().indices_.normal_vertex_image_index]; }
Texture2D<float4> get_depth_texture() { return SceneTextures[get_scene().indices_.depth_image_index]; }
Texture2D<float4> get_shadow_atlas() { return SceneTextures[get_scene().indices_.shadow_array_index]; }
Texture2D<float4> get_sky_transmittance_lut_texture() { return SceneTextures[get_scene().indices_.sky_transmittance_lut_index]; }
Texture2D<float4> get_sky_multi_scatter_lut_texture() { return SceneTextures[get_scene().indices_.sky_multiscatter_lut_index]; }
Texture2D<float4> get_bloom_texture() { return SceneTextures[get_scene().indices_.bloom_image_index]; }
Texture2D<float4> get_emission_texture() { return SceneTextures[get_scene().indices_.emission_image_index]; }
Texture2D<float4> get_metallic_roughness_ao_texture() { return SceneTextures[get_scene().indices_.metallic_roughness_ao_image_index]; }
Texture2D<float> get_hiz_texture() { return SceneTexturesFloat[get_scene().indices_.hiz_image_index]; }

// scene r/w textures
RWTexture2D<float4> get_sky_transmittance_lut_texturerw() { return SceneRWTextures[get_scene().indices_.sky_transmittance_lut_index]; }
RWTexture2D<float4> get_sky_multi_scatter_lut_texturerw() { return SceneRWTextures[get_scene().indices_.sky_multiscatter_lut_index]; }
RWTexture2D<float> get_hiz_texturerw() { return SceneRWTexturesFloat[get_scene().indices_.hiz_image_index]; }

// scene cube textures
TextureCube<float4> get_sky_env_map_texture() { return SceneCubeTextures[get_scene().indices_.sky_env_map_index]; }

// scene uint textures
Texture2D<uint> get_visibility_texture() { return SceneTexturesUint[get_scene().indices_.vis_image_index]; }
Texture2D<uint> get_gtao_texture() { return SceneTexturesUint[get_scene().indices_.gtao_buffer_image_index]; }

// material textures
SamplerState get_material_sampler(Material material) { return Samplers[material.sampler]; }

Texture2D<float4> get_material_albedo_texture(Material material) { return MaterialTextureMaps[material.albedo_map_id]; }
Texture2D<float4> get_material_normal_texture(Material material) { return MaterialTextureMaps[material.normal_map_id]; }
Texture2D<float4> get_material_physical_texture(Material material) { return MaterialTextureMaps[material.physical_map_id]; }
Texture2D<float4> get_material_ao_texture(Material material) { return MaterialTextureMaps[material.ao_map_id]; }
Texture2D<float4> get_material_emissive_texture(Material material) { return MaterialTextureMaps[material.emissive_map_id]; }

Texture2D<float4> get_sprite_material_albedo_texture(SpriteMaterial material) { return MaterialTextureMaps[material.albedo_map_id]; }

ShaderEntity get_entity(uint32 index) { return Buffers[get_scene().indices_.entites_buffer_index].Load<ShaderEntity>(index * sizeof(ShaderEntity)); }
float4x4 get_transform(uint32 index) { return Buffers[get_scene().indices_.transforms_buffer_index].Load<float4x4>(index * sizeof(float4x4)); }

Material get_material(uint32 material_index) {
  return Buffers[get_scene().indices_.materials_buffer_index].Load<Material>(material_index * sizeof(Material));
}

SpriteMaterial get_sprite_material(uint32 material_index) {
  return Buffers[get_scene().indices_.sprite_materials_buffer_index].Load<SpriteMaterial>(material_index * sizeof(SpriteMaterial));
}

Light get_light(uint32 lightIndex) { return Buffers[get_scene().indices_.lights_buffer_index].Load<Light>(lightIndex * sizeof(Light)); }

DebugAabb get_debug_aabb(uint32 index) { return BuffersRW[0].Load<DebugAabb>(sizeof(DrawIndirectCommand) + sizeof(DebugAabb) * index); }

bool try_push_debug_aabb(DebugAabb aabb) {
  uint index = 0;
  BuffersRW[0].InterlockedAdd(sizeof(uint32), 1, index);

#if TODO
  // Check if buffer is full
  if (index >= debugAabbBuffer.aabbs.length()) {
    atomicAdd(debugAabbBuffer.drawCommand.instanceCount, -1);
    return false;
  }
#endif

  BuffersRW[0].Store(sizeof(DrawIndirectCommand) + sizeof(DebugAabb) * index, aabb);
  return true;
}

#endif // GLOBALS_HLSLI
