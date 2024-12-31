#pragma once

#include "Core/Types.hpp"

namespace ox {
class RayCast {
public:
  float t_min = 0;
  float t_max = std::numeric_limits<float>::max();

  RayCast() = default;
  RayCast(const Vec3 ray_origin, const Vec3 ray_direction) : origin(ray_origin), direction(ray_direction) {}

  Vec3 get_point_on_ray(const float fraction) const { return origin + fraction * direction; }

  const Vec3& get_origin() const { return origin; }
  const Vec3& get_direction() const { return direction; }
  Vec3 get_direction_inverse() const { return 1.0f / direction; }

private:
  Vec3 origin = {};
  Vec3 direction = {};
};
} // namespace ox
