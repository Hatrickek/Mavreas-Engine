#define HAS_MULTISCATTER_LUT
#define HAS_TRANSMITTANCE_LUT
#include "SkyCommonShading.hlsli"

struct VSInput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD;
};

inline void vertexID_create_fullscreen_triangle(in uint vertexID, out float4 pos) {
  pos.x = (float)(vertexID / 2) * 4.0 - 1.0;
  pos.y = (float)(vertexID % 2) * 4.0 - 1.0;
  pos.z = 0;
  pos.w = 1;
}

VSInput VSmain(uint vertexID : SV_VertexID) {
  VSInput input;
  vertexID_create_fullscreen_triangle(vertexID, input.position);

  input.uv = input.position.xy;

  return input;
}

float4 PSmain(VSInput input) : SV_TARGET {
  float4 unprojected = mul(get_camera().inv_projection_view, float4(input.uv, 0.0f, 1.0f));
  unprojected.xyz /= unprojected.w;

  const float3 V = normalize(unprojected.xyz - get_camera().position);

  float4 color = float4(GetDynamicSkyColor(input.uv, V), 1);

  color = clamp(color, 0, 65000);
  return color;
}
