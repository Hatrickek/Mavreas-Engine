#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define UINT32_MAX 4294967295
#define INVALID_ID UINT32_MAX
#define PI 3.1415926535897932384626433832795
#define TwoPI 2 * PI
#define EPSILON 0.00001

#define MEDIUMP_FLT_MAX 65504.0
#define saturateMediump(x) min(x, MEDIUMP_FLT_MAX)
#define sqr(a) ((a) * (a))
#define pow5(x) pow(x, 5)

typedef uint64_t uint64;
typedef uint32_t uint32;
// typedef uint16_t uint16; need to enable -enable-16bit-types to use it...
// typedef uint8_t uint8; - not even supported in dxc

struct PackedFloat2 {
  float x, y;

  float2 unpack() { return float2(x, y); }
  void pack(float2 v) {
    x = v.x;
    y = v.y;
  }
};

struct PackedUint2 {
  uint32 x, y;

  uint2 unpack() { return uint2(x, y); }
  void pack(uint2 v) {
    x = v.x;
    y = v.y;
  }
};

struct PackedFloat3 {
  float x, y, z;

  float3 unpack() { return float3(x, y, z); }
  void pack(float3 v) {
    x = v.x;
    y = v.y;
    z = v.z;
  }
};

struct PackedFloat4 {
  float x, y, z, w;

  float4 unpack() { return float4(x, y, z, w); }
  void pack(float4 v) {
    x = v.x;
    y = v.y;
    z = v.z;
    w = v.w;
  }
};

struct PackedFloat4x4 { 
  float4 r1, r2, r3, r4;

  float4x4 unpack() { return float4x4(r1, r2, r3, r4); }
  void pack(float4x4 m) { 
    r1 = m[0];
    r2 = m[1];
    r3 = m[2];
    r4 = m[3];
  }
};

struct Vertex {
  PackedFloat3 position : POSITION;
  uint32 normal : NORMAL; // Octahedral encoding: decode with unpack_snorm2_x16 and oct_to_vec3
  PackedFloat2 uv : UV;
};

struct VertexOutput {
#ifdef USE_POSITION
  float4 position : SV_POSITION;
#endif
#ifdef USE_WORLD_POS
  float3 world_pos : WORLD_POSITION;
#endif
#ifdef USE_NORMAL
  float3 normal : NORMAL;
#endif
#ifdef USE_UV
  float2 uv : TEXCOORD;
#endif
#ifdef USE_VIEWPOS
  float3 view_pos : VIEW_POS;
#endif
#ifdef USE_TANGENT
  float4 tangent : TANGENT;
#endif
#ifdef USE_VIEWPORT
  uint vp_index : SV_ViewportArrayIndex;
#endif
#ifdef USE_PREV_POS
  float4 prev_position : PREV_POSITION;
#endif
  uint draw_index : DrawIndex;
};

struct PushConst {
  uint64 vertex_buffer_ptr;
  uint instance_offset;
  uint material_index;
};

struct MeshInstancePointer {
  uint data;

  void init() { data = 0; }
  void create(uint instance_index, uint camera_index = 0, float dither = 0) {
    data = 0;
    data |= instance_index & 0xFFFFFF;
    data |= (camera_index & 0xF) << 24u;
    data |= (uint(dither * 15.0f) & 0xF) << 28u;
  }
  uint get_instance_index() { return data & 0xFFFFFF; }
  uint get_camera_index() { return (data >> 24u) & 0xF; }
  float get_dither() { return float((data >> 28u) & 0xF) / 15.0f; }
};

struct ShaderEntity {
  float4x4 transform;
};

struct CameraData {
  float4 position;

  float4x4 projection;
  float4x4 inv_projection;
  float4x4 view;
  float4x4 inv_view;
  float4x4 projection_view;
  float4x4 inv_projection_view;

  float4x4 previous_projection;
  float4x4 previous_inv_projection;
  float4x4 previous_view;
  float4x4 previous_inv_view;
  float4x4 previous_projection_view;
  float4x4 previous_inv_projection_view;

  float2 temporalaa_jitter;
  float2 temporalaa_jitter_prev;

  float4 frustum_planes[6]; // xyz normal, w distance

  float3 up;
  float near_clip;
  float3 forward;
  float far_clip;
  float3 right;
  float fov;
  float3 _pad;
  uint output_index;
};

struct CameraCB {
  CameraData camera_data[16];
};

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light {
  PackedFloat3 position;

  PackedFloat3 rotation;
  uint32 type8_flags8_range16;

  PackedUint2 direction16_cone_angle_cos16; // coneAngleCos is used for cascade count in directional light
  PackedUint2 color;                        // half4 packed

  PackedFloat4 shadow_atlas_mul_add;

  uint32 radius16_length16;
  uint32 matrix_index;
  uint32 remap;

  float3 get_position() { return position.unpack(); }
  uint32 get_type() { return type8_flags8_range16 & 0xFF; }
  uint32 get_flags() { return (type8_flags8_range16 >> 8u) & 0xFF; }
  float get_range() { return f16tof32(type8_flags8_range16 >> 16u); }
  float get_radius() { return f16tof32(radius16_length16); }
  float get_length() { return f16tof32(radius16_length16 >> 16u); }
  float3 get_direction() {
    return normalize(float3(f16tof32(direction16_cone_angle_cos16.x),
                            f16tof32(direction16_cone_angle_cos16.x >> 16u),
                            f16tof32(direction16_cone_angle_cos16.y)));
  }
  float get_cone_angle_cos() { return f16tof32(direction16_cone_angle_cos16.y >> 16u); }
  uint32 get_shadow_cascade_count() { return direction16_cone_angle_cos16.y >> 16u; }
  float get_angle_scale() { return f16tof32(remap); }
  float get_angle_offset() { return f16tof32(remap >> 16u); }
  float get_cubemap_depth_remap_near() { return f16tof32(remap); }
  float get_cubemap_depth_remap_far() { return f16tof32(remap >> 16u); }
  float4 get_color() {
    float4 retVal;
    retVal.x = f16tof32(color.x);
    retVal.y = f16tof32(color.x >> 16u);
    retVal.z = f16tof32(color.y);
    retVal.w = f16tof32(color.y >> 16u);
    return retVal;
  }
  uint32 get_matrix_index() { return matrix_index; }
  float get_gravity() { return get_cone_angle_cos(); }
  float3 get_collider_tip() { return shadow_atlas_mul_add.unpack().xyz; }
};

struct SceneData {
  uint32 num_lights;
  float grid_max_distance;
  PackedUint2 screen_size;
  int draw_meshlet_aabbs;

  PackedFloat2 screen_size_rcp;
  PackedUint2 shadow_atlas_res;

  PackedFloat3 sun_direction;
  uint32 meshlet_count;

  PackedFloat4 sun_color; // pre-multipled with intensity

  struct Indices {
    int albedo_image_index;
    int normal_image_index;
    int normal_vertex_image_index;
    int depth_image_index;
    int bloom_image_index;
    int mesh_instance_buffer_index;
    int entites_buffer_index;
    int materials_buffer_index;
    int lights_buffer_index;
    int sky_env_map_index;
    int sky_transmittance_lut_index;
    int sky_multiscatter_lut_index;
    int velocity_image_index;
    int shadow_array_index;
    int gtao_buffer_image_index;
    int hiz_image_index;
    int vis_image_index;
    int emission_image_index;
    int metallic_roughness_ao_image_index;
    int transforms_buffer_index;
    int sprite_materials_buffer_index;
  } indices_;

  // TODO: use flags
  struct PostProcessingData {
    int tonemapper;
    float exposure;
    float gamma;

    int enable_bloom;
    int enable_ssr;
    int enable_gtao;

    PackedFloat4 vignette_color;       // rgb: color, a: intensity
    PackedFloat4 vignette_offset;      // xy: offset, z: useMask, w: enable effect
    PackedFloat2 film_grain;           // x: enable, y: amount
    PackedFloat2 chromatic_aberration; // x: enable, y: amount
    PackedFloat2 sharpen;              // x: enable, y: amount
  } post_processing_data;
};

struct Meshlet {
  uint32 vertex_offset;
  uint32 index_offset;
  uint32 primitive_offset;
  uint32 index_count;
  uint32 primitive_count;
  PackedFloat3 aabb_min;
  PackedFloat3 aabb_max;
};

struct MeshletInstance {
  uint32 meshlet_id;
  uint32 instance_id;
  uint32 material_id;
};

struct DrawIndirectCommand {
  uint32 vertex_count;
  uint32 instance_count;
  uint32 first_vertex;
  uint32 first_instance;
};

#define MAX_AABB_COUNT 100000

struct DebugAabb {
  PackedFloat3 center;
  PackedFloat3 extent;
  PackedFloat4 color;
};

bool is_nan(float3 vec) {
  return (asuint(vec.x) & 0x7fffffff) > 0x7f800000 || (asuint(vec.y) & 0x7fffffff) > 0x7f800000 || (asuint(vec.z) & 0x7fffffff) > 0x7f800000;
}

bool is_saturated(float a) { return a == saturate(a); }
bool is_saturated(float2 a) { return all(a == saturate(a)); }
bool is_saturated(float3 a) { return all(a == saturate(a)); }
bool is_saturated(float4 a) { return all(a == saturate(a)); }

float3 clipspace_to_uv(in float3 clipspace) { return clipspace * float3(0.5, 0.5, 0.5) + 0.5; }
float2 clipspace_to_uv(in float2 clipspace) { return clipspace * float2(0.5, -0.5) + 0.5; }
float2 uv_to_clipspace(in float2 uv) { return uv * 2.0 - 1.0; }

float3 cubemap_to_uv(in float3 r) {
  float faceIndex = 0;
  float3 absr = abs(r);
  float3 uvw = 0;
  if (absr.x > absr.y && absr.x > absr.z) {
    // x major
    float negx = step(r.x, 0.0);
    uvw = float3(r.zy, absr.x) * float3(lerp(-1.0, 1.0, negx), -1, 1);
    faceIndex = negx;
  } else if (absr.y > absr.z) {
    // y major
    float negy = step(r.y, 0.0);
    uvw = float3(r.xz, absr.y) * float3(1.0, lerp(1.0, -1.0, negy), 1.0);
    faceIndex = 2.0 + negy;
  } else {
    // z major
    const float negz = step(r.z, 0.0);
    uvw = float3(r.xy, absr.z) * float3(lerp(1.0, -1.0, negz), -1, 1);
    faceIndex = 4.0 + negz;
  }
  return float3((uvw.xy / uvw.z + 1) * 0.5, faceIndex);
}

// Return the closest point on the segment (with limit)
float3 closest_point_on_segment(float3 a, float3 b, float3 c) {
  float3 ab = b - a;
  float t = dot(c - a, ab) / dot(ab, ab);
  return a + saturate(t) * ab;
}

inline float sphere_volume(const float radius) { return 4.0 / 3.0 * PI * radius * radius * radius; }

float2x2 inverse(float2x2 mat) {
  float2x2 m;
  m[0][0] = mat[1][1];
  m[0][1] = -mat[0][1];
  m[1][0] = -mat[1][0];
  m[1][1] = mat[0][0];

  return mul((1.0f / mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0]), m);
}

// zero-to-one depth
float3 unproject_uv_zo(float depth, float2 uv, float4x4 invXProj) {
  const float4 ndc = float4(uv * 2.0 - 1.0, depth, 1.0);
  float4 world = mul(invXProj, ndc);
  return world.xyz / world.w;
}

float3 hsv_to_rgb(const float3 hsv) {
  const float3 rgb = clamp(abs(fmod(hsv.x * 6.0 + float3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
  return hsv.z * lerp(float3(1.0, 1.0, 1.0), rgb, hsv.y);
}

float3 create_cube(in uint vertexID) {
  const uint b = 1u << vertexID;
  return float3((0x287au & b) != 0u, (0x02afu & b) != 0u, (0x31e3u & b) != 0u);
}

float3 oct_to_vec3(float2 e) {
  float3 v = float3(e.xy, 1.0 - abs(e.x) - abs(e.y));
  const float2 sign_not_zero = float2((v.x >= 0.0) ? +1.0 : -1.0, (v.y >= 0.0) ? +1.0 : -1.0);
  if (v.z < 0.0)
    v.xy = (1.0 - abs(v.yx)) * sign_not_zero;
  return normalize(v);
}

float2 vec3_to_oct(float3 v) {
  float2 p = float2(v.x, v.y) * (1.0 / (abs(v.x) + abs(v.y) + abs(v.z)));
  const float2 sign_not_zero = float2((p.x >= 0.0) ? 1.0 : -1.0, (p.y >= 0.0) ? 1.0 : -1.0);
  return (v.z <= 0.0) ? ((1.0 - abs(float2(p.y, p.x))) * sign_not_zero) : p;
}

float u16n_to_f32(const uint val) { return saturate(val / 65535.0f); }
float2 unpack_unorm2_x16(const uint packed) { return float2(u16n_to_f32(packed & 0xFFFF), u16n_to_f32(packed >> 16)); }
float2 unpack_snorm2_x16(const uint packed) {
  int2 signextended;
  signextended.x = (int)(packed << 16) >> 16;
  signextended.y = (int)(packed & 0xFFFF0000) >> 16;
  return max(float2(signextended) / 32767.0f, -1.0f);
}

uint unpack_u32_low(uint packed) { return packed & 0xFFFF; }
uint unpack_u32_high(uint packed) { return (packed >> 16) & 0xFFFF; }

#endif
