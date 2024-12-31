#pragma once
#include <glm/gtx/norm.hpp>

#include "Core/Types.hpp"

namespace ox {
class RayCast;
}
namespace ox {
struct Plane;
struct Frustum;

enum Intersection { Outside = 0, Intersects = 1, Inside = 2 };

struct AABB {
  float3 min = {};
  float3 max = {};

  AABB() = default;
  ~AABB() = default;
  AABB(const AABB& other) = default;
  AABB(const float3 min, const float3 max) : min(min), max(max) {}

  float3 get_center() const { return (max + min) * 0.5f; }
  float3 get_extents() const { return max - min; }
  float3 get_size() const { return get_extents(); }

  void translate(const float3& translation);
  void scale(const float3& scale);
  void rotate(const float3x3& rotation);
  void transform(const float4x4& transform);
  AABB get_transformed(const float4x4& transform) const;

  void merge(const AABB& other);

  bool is_on_or_forward_plane(const Plane& plane) const;
  bool is_on_frustum(const Frustum& frustum) const;
  bool intersects(const float3& point) const;
  Intersection intersects(const AABB& box) const;
  bool intersects_fast(const AABB& box) const;
  bool intersects(const RayCast& ray) const;
};

struct Sphere {
  float3 center = {};
  float radius = {};

  Sphere() = default;
  ~Sphere() = default;
  Sphere(const Sphere& other) = default;
  Sphere(const float3 center, const float radius) : center(center), radius(radius) {}

  bool intersects(const AABB& b) const;
  bool intersects(const Sphere& b) const;
  bool intersects(const Sphere& b, float& dist) const;
  bool intersects(const Sphere& b, float& dist, float3& direction) const;
  bool intersects(const RayCast& ray) const;
  bool intersects(const RayCast& ray, float& dist) const;
  bool intersects(const RayCast& ray, float& dist, float3& direction) const;
};
} // namespace ox
