#include "Globals.hlsli"

#define RENDER_FLAGS_2D_SORT_Y 1u << 0u
#define RENDER_FLAGS_2D_FLIP_X 1u << 1u

struct VOutput {
  float4 position : SV_Position;
  float3 normal : NORMAL;
  float4 uv_alpha : UV;
  uint32 material_index : MAT_INDEX;
  uint32 flags : FLAGS;
};

struct VertexInput {
  PackedFloat4x4 transform : TRANSFORM;
  uint32 material_id16_ypos16 : MAT_INDEX;
  uint32 flags16_distance16 : FLAGS;
};

static float3 positions[6] =
  {float3(-0.5, -0.5, 0), float3(0.5, -0.5, 0), float3(0.5, 0.5, 0), float3(0.5, 0.5, 0), float3(-0.5, 0.5, 0), float3(-0.5, -0.5, 0)};
static float2 uvs[6] = {float2(0.0, 1.0), float2(1.0, 1.0), float2(1.0, 0.0), float2(1.0, 0.0), float2(0.0, 0.0), float2(0.0, 1.0)};

VOutput VSmain(VertexInput input, uint vertex_id : SV_VertexID) {
  VOutput output = (VOutput)0;

  const uint32 flags = unpack_u32_low(input.flags16_distance16);
  output.flags = flags;

  const uint32 material_index = unpack_u32_low(input.material_id16_ypos16);
  SpriteMaterial material = get_sprite_material(material_index);

  float4x4 unpacked_transform = transpose(input.transform.unpack());
  float4 uv_size_offset = float4(material.get_uv_size(), material.get_uv_offset());

  const uint vertex_index = vertex_id % 6;

  output.uv_alpha.xy = uvs[vertex_index];
  output.uv_alpha.xy = (output.uv_alpha.xy * uv_size_offset.xy) + (uv_size_offset.zw);

  const int flip = flags & RENDER_FLAGS_2D_FLIP_X;
  float4 world_position = float4(float2(positions[vertex_index].xy * float2((float)flip ? -1 : 1, 1)), 0, 1);
  world_position = mul(unpacked_transform, world_position);

  output.position = mul(get_camera(0).projection_view, float4(world_position.xyz, 1.0f));
  output.material_index = material_index;

  return output;
}

float4 PSmain(VOutput input) : SV_Target0 {
  SpriteMaterial material = get_sprite_material(input.material_index);
  const SamplerState material_sampler = NEAREST_REPEATED_SAMPLER;

  float4 color = material.color.unpack();

  if (material.albedo_map_id != INVALID_ID) {
    float2 uv = input.uv_alpha.xy;
    color *= get_sprite_material_albedo_texture(material).Sample(material_sampler, uv);
  }

  return color;
}
