#include "Globals.hlsli"
#include "VisBufferCommon.hlsli"

void debug_draw_meshlet_aabb(const uint meshletInstanceId) {
  const MeshletInstance meshlet_instance = get_meshlet_instance(meshletInstanceId);
  const uint32 meshlet_id = meshlet_instance.meshlet_id;
  const Meshlet meshlet = get_meshlet(meshlet_id);
  const float4x4 transform = get_transform(meshlet_instance.instance_id);

  const float3 aabb_min = meshlet.aabb_min.unpack();
  const float3 aabb_max = meshlet.aabb_max.unpack();
  const float3 aabb_size = aabb_max - aabb_min;
  const float3 aabb_corners[] = {aabb_min,
                                 aabb_min + float3(aabb_size.x, 0.0, 0.0),
                                 aabb_min + float3(0.0, aabb_size.y, 0.0),
                                 aabb_min + float3(0.0, 0.0, aabb_size.z),
                                 aabb_min + float3(aabb_size.xy, 0.0),
                                 aabb_min + float3(0.0, aabb_size.yz),
                                 aabb_min + float3(aabb_size.x, 0.0, aabb_size.z),
                                 aabb_min + aabb_size};

  float3 world_aabb_min = float3(1e20, 1e20, 1e20);
  float3 world_aabb_max = float3(-1e20, -1e20, -1e20);
  for (uint i = 0; i < 8; ++i) {
    float3 world = float3(mul(transform, float4(aabb_corners[i], 1.0)).xyz);
    world_aabb_min = min(world_aabb_min, world);
    world_aabb_max = max(world_aabb_max, world);
  }

  const float3 aabb_center = (world_aabb_min + world_aabb_max) / 2.0;
  PackedFloat3 center_packed = (PackedFloat3)0;
  center_packed.pack(aabb_center);

  const float3 extent = (world_aabb_max - world_aabb_min);
  PackedFloat3 extent_packed = (PackedFloat3)0;
  extent_packed.pack(extent);

  const float GOLDEN_CONJ = 0.6180339887498948482045868343656;
  const float4 color = float4(2.0 * hsv_to_rgb(float3(float(meshlet_id) * GOLDEN_CONJ, 0.875, 0.85)), 1.0);
  PackedFloat4 color_packed = (PackedFloat4)0;
  color_packed.pack(color);

  DebugAabb aabb;
  aabb.extent = extent_packed;
  aabb.center = center_packed;
  aabb.color = color_packed;

  try_push_debug_aabb(aabb);
}

bool is_aabb_inside_plane(in float3 center, in float3 extent, in float4 plane) {
  const float3 normal = plane.xyz;
  const float radius = dot(extent, abs(normal));
  return (dot(normal, center) - plane.w) >= -radius;
}

struct GetMeshletUvBoundsParams {
  uint meshlet_instance_id;
  float4x4 view_proj;
  bool clamp_ndc;
};

void get_meshlet_uv_bounds(GetMeshletUvBoundsParams params, out float2 minXY, out float2 maxXY, out float nearestZ, out int intersects_near_plane) {
  const MeshletInstance meshlet_instance = get_meshlet_instance(params.meshlet_instance_id);
  const Meshlet meshlet = get_meshlet(meshlet_instance.meshlet_id);
  const float4x4 transform = get_transform(meshlet_instance.instance_id);

  const float3 aabb_min = meshlet.aabb_min.unpack();
  const float3 aabb_max = meshlet.aabb_max.unpack();
  const float3 aabb_size = aabb_max - aabb_min;
  const float3 aabb_corners[] = {aabb_min,
                                 aabb_min + float3(aabb_size.x, 0.0, 0.0),
                                 aabb_min + float3(0.0, aabb_size.y, 0.0),
                                 aabb_min + float3(0.0, 0.0, aabb_size.z),
                                 aabb_min + float3(aabb_size.xy, 0.0),
                                 aabb_min + float3(0.0, aabb_size.yz),
                                 aabb_min + float3(aabb_size.x, 0.0, aabb_size.z),
                                 aabb_min + aabb_size};

  // The nearest projected depth of the object's AABB
  nearestZ = 0;

  // Min and max projected coordinates of the object's AABB in UV space
  minXY = float2(1e20, 1e20);
  maxXY = float2(-1e20, -1e20);
  const float4x4 mvp = mul(params.view_proj, transform);
  for (uint i = 0; i < 8; ++i) {
    float4 clip = mul(mvp, float4(aabb_corners[i], 1.0));

    // AABBs that go behind the camera at all are considered visible
    if (clip.w <= 0) {
      intersects_near_plane = true;
      return;
    }

    clip.z = max(clip.z, 0.0);
    clip /= clip.w;
    if (params.clamp_ndc) {
      clip.xy = clamp(clip.xy, -1.0, 1.0);
    }
    clip.xy = clip.xy * 0.5 + 0.5;
    minXY = min(minXY, clip.xy);
    maxXY = max(maxXY, clip.xy);
    nearestZ = saturate(max(nearestZ, clip.z));
  }

  intersects_near_plane = false;
}

bool cull_quad_hiz(float2 minXY, float2 maxXY, float nearestZ) {
  const float4 boxUvs = float4(minXY, maxXY);
  Texture2D<float> hiz_texture = get_hiz_texture();
  float hizwidth, hizheight;
  hiz_texture.GetDimensions(hizwidth, hizheight);

  const float width = (boxUvs.z - boxUvs.x) * hizwidth;
  const float height = (boxUvs.w - boxUvs.y) * hizheight;

  // Select next level so the box is always in [0.5, 1.0) of a texel of the current level.
  // If the box is larger than a single texel of the current level, then it could touch nine
  // texels rather than four! So we need to round up to the next level.
  const float level = ceil(log2(max(width, height)));
  const float depth[4] = {hiz_texture.SampleLevel(HIZ_SAMPLER, boxUvs.xy, level).x,
                          hiz_texture.SampleLevel(HIZ_SAMPLER, boxUvs.zy, level).x,
                          hiz_texture.SampleLevel(HIZ_SAMPLER, boxUvs.xw, level).x,
                          hiz_texture.SampleLevel(HIZ_SAMPLER, boxUvs.zw, level).x};
  const float farHZB = min(min(min(depth[0], depth[1]), depth[2]), depth[3]);

  // Object is occluded if its nearest depth is farther away from the camera than the farthest sampled depth
  if (nearestZ < farHZB) {
    return false;
  }

  return true;
}

bool cull_meshlet_frustum(const uint meshlet_instance_id) {
  const MeshletInstance meshlet_instance = get_meshlet_instance(meshlet_instance_id);
  const Meshlet meshlet = get_meshlet(meshlet_instance.meshlet_id);
  const float4x4 transform = get_transform(meshlet_instance.instance_id);

  const float3 aabb_min = meshlet.aabb_min.unpack();
  const float3 aabb_max = meshlet.aabb_max.unpack();
  const float3 aabb_center = (aabb_min + aabb_max) / 2.0f;
  const float3 aabb_extent = aabb_max - aabb_center;
  const float3 world_aabb_center = float3(mul(transform, float4(aabb_center, 1.0)).xyz);

  const float4x4 transformT = transpose(transform);
  const float3 right = transformT[0].xyz * aabb_extent.x;
  const float3 up = transformT[1].xyz * aabb_extent.y;
  const float3 forward = -transformT[2].xyz * aabb_extent.z;

  const float3 worldExtent = float3(abs(dot(float3(1.0, 0.0, 0.0), right)) + abs(dot(float3(1.0, 0.0, 0.0), up)) +
                                      abs(dot(float3(1.0, 0.0, 0.0), forward)),

                                    abs(dot(float3(0.0, 1.0, 0.0), right)) + abs(dot(float3(0.0, 1.0, 0.0), up)) +
                                      abs(dot(float3(0.0, 1.0, 0.0), forward)),

                                    abs(dot(float3(0.0, 0.0, 1.0), right)) + abs(dot(float3(0.0, 0.0, 1.0), up)) +
                                      abs(dot(float3(0.0, 0.0, 1.0), forward)));

  const CameraData cam = get_camera(0);
  for (uint i = 0; i < 6; ++i) {
    if (!is_aabb_inside_plane(world_aabb_center, worldExtent, cam.frustum_planes[i])) {
      return false;
    }
  }

  return true;
}

[numthreads(128, 1, 1)] void main(uint3 threadID
                                  : SV_DispatchThreadID) {
  const uint meshlet_instance_id = threadID.x;
  const MeshletInstance meshlet_instance = get_meshlet_instance(meshlet_instance_id);
  const uint meshlet_id = meshlet_instance.meshlet_id;

  if (meshlet_id >= get_scene().meshlet_count) {
    return;
  }

  if (cull_meshlet_frustum(meshlet_instance_id)) {
    GetMeshletUvBoundsParams params;
    params.meshlet_instance_id = meshlet_instance_id;
    params.view_proj = get_camera(0).projection_view;
    params.clamp_ndc = true;

    float2 minXY;
    float2 maxXY;
    float nearestZ;
    bool intersects_near_plane;
    get_meshlet_uv_bounds(params, minXY, maxXY, nearestZ, intersects_near_plane);
    bool is_visible = intersects_near_plane;

    const bool CULL_MESHLET_HIZ = true;
    if (!is_visible) {
      if (!CULL_MESHLET_HIZ) {
        is_visible = true;
      } else {
        // Hack to get around apparent precision issue for tiny meshlets
        is_visible = cull_quad_hiz(minXY, maxXY, nearestZ + 0.0001);
      }
    }

    if (is_visible) {
      uint idx = 0;
      buffers_rw[CULL_TRIANGLES_DISPATCH_PARAMS_BUFFERS_INDEX].InterlockedAdd(0, 1, idx);
      buffers_rw[VISIBLE_MESHLETS_BUFFER_INDEX].Store<uint32>(idx * sizeof(uint32), meshlet_id);

      if (get_scene().draw_meshlet_aabbs) {
        debug_draw_meshlet_aabb(meshlet_id);
        // DebugRect rect;
        // rect.minOffset = Vec2ToPacked(minXY);
        // rect.maxOffset = Vec2ToPacked(maxXY);
        // const float GOLDEN_CONJ = 0.6180339887498948482045868343656;
        // float4 color = float4(2.0 * hsv_to_rgb(float3(float(meshletId) * GOLDEN_CONJ, 0.875, 0.85)), 1.0);
        // rect.color = Vec4ToPacked(color);
        // rect.depth = nearestZ;
        // TryPushDebugRect(rect);
      }
    }
  }
}
