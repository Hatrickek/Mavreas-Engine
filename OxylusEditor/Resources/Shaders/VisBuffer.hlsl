#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

struct VOut {
  float4 position : SV_POSITION;
  uint meshlet_id : MESHLET_ID;
  uint primitive_id : PRIMITIVE_ID;
  float2 uv : UV0;
  float3 object_space_pos : OBJECT_SPACE_POS;
  uint material_id : MATERIAL;
};

VOut VSmain(uint vertex_index : SV_VertexID) {
  VOut vout;

  const uint meshlet_instance_id = (vertex_index >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
  const uint primitiveId = vertex_index & MESHLET_PRIMITIVE_MASK;

  const MeshletInstance meshlet_instance = get_meshlet_instance(meshlet_instance_id);
  const uint instance_id = meshlet_instance.instance_id;
  const uint meshlet_id = meshlet_instance.meshlet_id;

  const Meshlet meshlet = get_meshlet(meshlet_id);
  const uint vertexOffset = meshlet.vertex_offset;
  const uint indexOffset = meshlet.index_offset;
  const uint primitiveOffset = meshlet.primitive_offset;

  const uint primitive = uint(get_primitive(primitiveOffset + primitiveId));
  const uint index = get_index(indexOffset + primitive);
  Vertex vertex = get_vertex(vertexOffset + index);
  const float3 position = vertex.position.unpack();
  const float2 uv = vertex.uv.unpack();
  const float4x4 transform = get_transform(instance_id);

  vout.meshlet_id = meshlet_id;
  vout.primitive_id = primitiveId / 3;
  vout.uv = uv;
  vout.object_space_pos = position;
  vout.material_id = meshlet_instance.material_id;

  vout.position = mul(mul(get_camera(0).projection_view, transform), float4(position, 1.0));

  return vout;
}

uint PSmain(VOut input) : SV_TARGET {
  const Material material = get_material(input.material_id);

  float2 scaled_uv = input.uv;
  scaled_uv *= material.uv_scale;

  float4 base_color;
  const SamplerState material_sampler = Samplers[material.sampler];
  if (material.albedo_map_id != INVALID_ID) {
    base_color = get_material_albedo_texture(material).Sample(material_sampler, scaled_uv) * material.color;
  } else {
    base_color = material.color;
  }

  if (material.alpha_mode == ALPHA_MODE_MASK) {
    clip(base_color.a - material.alpha_cutoff);
  }

  return (input.meshlet_id << MESHLET_PRIMITIVE_BITS) | (input.primitive_id & MESHLET_PRIMITIVE_MASK);
}
