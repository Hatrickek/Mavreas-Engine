#include "Globals.hlsli"

[numthreads(16, 16, 1)] void main(uint3 threadID
                                  : SV_DispatchThreadID) {
  RWTexture2D<float> hiz_texture = get_hiz_texturerw();
  const float2 position = float2(threadID.xy);
  float width, height;
  hiz_texture.GetDimensions(width, height);
  const float2 uv = (position + 0.5) / float2(width, height);
  const float depth[] = {get_depth_texture().SampleLevel(HIZ_SAMPLER, uv, 0.0, int2(0.0, 0.0)).r,
                         get_depth_texture().SampleLevel(HIZ_SAMPLER, uv, 0.0, int2(1.0, 0.0)).r,
                         get_depth_texture().SampleLevel(HIZ_SAMPLER, uv, 0.0, int2(0.0, 1.0)).r,
                         get_depth_texture().SampleLevel(HIZ_SAMPLER, uv, 0.0, int2(1.0, 1.0)).r};
  const float depth_sample = min(min(min(depth[0], depth[1]), depth[2]), depth[3]);

  hiz_texture[uint2(position.xy)] = depth_sample;
}
