#pragma once
#include <cstdint>
#include <random>

#include "Core/ESystem.hpp"
#include "Core/Types.hpp"

namespace ox {
class Random : public ESystem {
public:
  Random() = default;

  void init() override;
  void deinit() override;

  static uint32_t get_uint();
  static uint32_t get_uint(uint32_t min, uint32_t max);
  static float get_float();
  static Vec3 get_vec3();
  static Vec3 get_vec3(float min, float max);
  static Vec3 in_unit_sphere();

private:
  static std::mt19937 random_engine;
  static std::uniform_int_distribution<std::mt19937::result_type> distribution;
};
}
