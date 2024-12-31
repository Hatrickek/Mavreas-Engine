#ifndef OBJECTVS_HLSL
#define OBJECTVS_HLSL

#ifdef DEPTH_PASS
  #define USE_POSITION
  #define USE_WORLD_POS
  #define USE_NORMAL
  #define USE_UV
  #define OUTPUT_NORMAL_VS
  #define USE_PREV_POS
#endif

#ifdef SHADOW_PASS
  #define USE_POSITION
  #define USE_VIEWPORT
#endif

#ifdef FORWARD_PASS
  #define USE_POSITION
  #define USE_WORLD_POS
  #define USE_NORMAL
  #define USE_UV
  #define USE_VIEWPOS
  #define USE_TANGENT
#endif

#include "Globals.hlsli"

[[vk::push_constant]] PushConst push_const;

VertexOutput VSmain(VertexInput input) {
  const float3 vertex_position = input.get_vertex_position(push_const.vertex_buffer_ptr);
  const float3 vertex_normal = input.get_vertex_normal(push_const.vertex_buffer_ptr);
  const float2 vertex_uv = input.get_vertex_uv(push_const.vertex_buffer_ptr);
  const float4 tangent = input.get_vertex_tangent(push_const.vertex_buffer_ptr);

  const MeshInstance mesh_instance = input.get_instance(push_const.instance_offset);

  const float4x4 model_matrix = mesh_instance.transform;

  const float4 loc_pos = mul(model_matrix, float4(vertex_position.xyz, 1.0));

  const CameraData cam = get_camera(input.get_instance_pointer(push_const.instance_offset).get_camera_index());

  VertexOutput output;

#ifdef USE_NORMAL
  output.normal = normalize(mul(model_matrix, vertex_normal)).xyz;
  #ifdef OUTPUT_NORMAL_VS
  output.normal = mul(cam.view, output.normal); // normals in viewspace
  #endif
#endif

#ifdef USE_UV
  output.uv = vertex_uv.xy;
#endif
#ifdef USE_WORLD_POS
  output.world_pos = loc_pos.xyz;
#endif
#ifdef USE_POSITION
  output.position = mul(cam.projection_view, float4(loc_pos.xyz, 1.0));
#endif
#ifdef USE_VIEWPOS
  output.view_pos = mul(cam.view, float4(loc_pos.xyz, 1.0f)).xyz;
#endif
#ifdef USE_TANGENT
  output.tangent = mul(model_matrix, tangent);
#endif
#ifdef USE_VIEWPORT
  output.vp_index = cam.output_index;
#endif
#ifdef USE_PREV_POS
  output.prev_position = mul(cam.previous_projection_view, float4(vertex_position.xyz, 1.0f));
#endif

  output.draw_index = input.draw_index;

  return output;
}

#endif
