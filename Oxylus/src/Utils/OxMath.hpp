#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Core/Types.hpp"

#include "Profiler.hpp"
#include "Render/BoundingVolume.hpp"

namespace JPH {
class AABox;
class Vec3;
class Vec4;
} // namespace JPH

namespace ox::math {
inline uint32 flooru32(float value) {
  int64 value_container = (int64)floorf(value);
  const uint32 v = (uint32)value_container;
  return v;
}

inline uint32 pack_u16(uint16 low, uint16 high) {
  uint32 result = 0;
  result |= low;
  result |= high << 16u;
  return result;
}

inline uint16 unpack_u32_low(uint32 packed) { return packed & 0xFFFF; }
inline uint16 unpack_u32_high(uint32 packed) { return (packed >> 16) & 0xFFFF; }

inline float2 sign_not_zero(float2 v) { return {(v.x >= 0.0f) ? +1.0f : -1.0f, (v.y >= 0.0f) ? +1.0f : -1.0f}; }

inline float2 float32x3_to_oct(float3 v) {
  const float2 p = float2{v.x, v.y} * (1.0f / (abs(v.x) + abs(v.y) + abs(v.z)));
  return (v.z <= 0.0f) ? ((1.0f - glm::abs(float2{p.y, p.x})) * sign_not_zero(p)) : p;
}

constexpr uint32_t previous_power2(uint32_t x) {
  uint32_t v = 1;
  while ((v << 1) < x) {
    v <<= 1;
  }
  return v;
}

inline float3 unproject_uv_zo(float depth, float2 uv, const float4x4& invXProj) {
  float4 ndc = float4(uv * 2.0f - 1.0f, depth, 1.0f);
  float4 world = invXProj * ndc;
  return float3(world) / world.w;
}

bool decompose_transform(const float4x4& transform, float3& translation, float3& rotation, float3& scale);

template <typename T>
static T smooth_damp(const T& current, const T& target, T& current_velocity, float smooth_time, const float max_speed, float delta_time) {
  OX_SCOPED_ZONE;
  // Based on Game Programming Gems 4 Chapter 1.10
  smooth_time = glm::max(0.0001F, smooth_time);
  const float omega = 2.0f / smooth_time;

  const float x = omega * delta_time;
  const float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);

  T change = current - target;
  const T original_to = target;

  // Clamp maximum speed
  const float max_change = max_speed * smooth_time;

  const float max_change_sq = max_change * max_change;
  const float sq_dist = glm::length2(change);
  if (sq_dist > max_change_sq) {
    const float mag = glm::sqrt(sq_dist);
    change = change / mag * max_change;
  }

  const T new_target = current - change;
  const T temp = (current_velocity + omega * change) * delta_time;

  current_velocity = (current_velocity - omega * temp) * exp;

  T output = new_target + (change + temp) * exp;

  // Prevent overshooting
  const T orig_minus_current = original_to - current;
  const T out_minus_orig = output - original_to;

  if (glm::compAdd(orig_minus_current * out_minus_orig) > 0.0f) {
    output = original_to;
    current_velocity = (output - original_to) / delta_time;
  }
  return output;
}

float lerp(float a, float b, float t);
float inverse_lerp(float a, float b, float value);
float inverse_lerp_clamped(float a, float b, float value);
float2 world_to_screen(const float3& world_pos, const float4x4& mvp, float width, float height, float win_pos_x, float win_pos_y);

float4 transform(const float4& vec, const float4x4& view);
float4 transform_normal(const float4& vec, const float4x4& mat);
float4 transform_coord(const float4& vec, const float4x4& view);

float3 from_jolt(const JPH::Vec3& vec);
JPH::Vec3 to_jolt(const float3& vec);
float4 from_jolt(const JPH::Vec4& vec);
JPH::Vec4 to_jolt(const float4& vec);
AABB from_jolt(const JPH::AABox& aabb);
} // namespace ox::math
