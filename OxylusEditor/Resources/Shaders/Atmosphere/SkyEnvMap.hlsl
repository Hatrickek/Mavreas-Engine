#include "SkyCommonShading.hlsli"

struct VOutput {
  float4 pos : SV_POSITION;
  float3 nor : NORMAL;
  uint RTIndex : SV_RenderTargetArrayIndex;
};

VOutput VSmain(Vertex inVertex, uint vid : SV_VERTEXID, uint instanceID : SV_INSTANCEID) {
  VOutput output;
  output.RTIndex = instanceID;
  output.pos = mul(get_camera(output.RTIndex).projection_view, float4(inVertex.position.unpack(), 1.0));
  output.nor = inVertex.position.unpack();
  return output;
}

float4 PSmain(VOutput input) : SV_TARGET {
  const float3 normal = normalize(input.nor);

  // No direct sun should be visible inside the probe capture:
  float4 color = float4(GetDynamicSkyColor(input.pos.xy, normal, false, false, false, false, false, false), 1);

  color = clamp(color, 0, 65000);
  return float4(color.rgb, 1);
}
