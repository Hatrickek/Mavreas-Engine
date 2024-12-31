#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

struct VSOutput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD;
};

float PSmain(VSOutput input) : SV_DEPTH {
  const int2 position = int2(input.position.xy);
  Texture2D<uint> vis_texture = get_visibility_texture();
  const uint payload = vis_texture.Load(int3(position, 0));
  if (payload == ~0u) {
    discard;
  }
  const uint meshletId = (payload >> MESHLET_PRIMITIVE_BITS) & MESHLET_ID_MASK;
  const uint materialId = get_meshlet_instance(meshletId).material_id;

  return asfloat(0x3f7fffffu - (materialId & MESHLET_MATERIAL_ID_MASK));
}
