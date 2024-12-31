struct VSOutput {
  float4 position : SV_Position;
  float2 uv : TEXCOORD;
};

VSOutput main(uint vertexID : SV_VertexID) {
  VSOutput output;

  output.uv = float2((vertexID << 1) & 2, vertexID & 2);
  output.position = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);

  return output;
}
