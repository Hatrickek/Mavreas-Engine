#pragma once
#include "Core/Types.hpp"

namespace ox {
struct AudioListenerConfig {
  float cone_inner_angle = glm::radians(360.0f);
  float cone_outer_angle = glm::radians(360.0f);
  float cone_outer_gain = 0.0f;
};

class AudioListener {
public:
  AudioListener() = default;

  void set_config(const AudioListenerConfig& config) const;
  void set_position(const Vec3& position) const;
  void set_direction(const Vec3& forward) const;
  void set_velocity(const Vec3& velocity) const;

private:
  uint32_t m_ListenerIndex = 0;
};
}
