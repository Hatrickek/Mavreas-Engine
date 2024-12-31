#include "AudioListener.hpp"

#include <miniaudio.h>
#include "AudioEngine.hpp"

#include "Core/App.hpp"

namespace ox {
void AudioListener::set_config(const AudioListenerConfig& config) const {
  auto* engine = App::get_system<AudioEngine>()->get_engine();
  ma_engine_listener_set_cone(engine,
    m_ListenerIndex,
    config.cone_inner_angle,
    config.cone_outer_angle,
    config.cone_outer_gain);
}

void AudioListener::set_position(const glm::vec3& position) const {
  auto* engine = App::get_system<AudioEngine>()->get_engine();
  ma_engine_listener_set_position(engine, m_ListenerIndex, position.x, position.y, position.z);

  static bool setupWorldUp = false;
  if (!setupWorldUp) {
    ma_engine_listener_set_world_up(engine, m_ListenerIndex, 0, 1, 0);
    setupWorldUp = true;
  }
}

void AudioListener::set_direction(const glm::vec3& forward) const {
  auto* engine = App::get_system<AudioEngine>()->get_engine();
  ma_engine_listener_set_direction(engine, m_ListenerIndex, forward.x, forward.y, forward.z);
}

void AudioListener::set_velocity(const glm::vec3& velocity) const {
  auto* engine = App::get_system<AudioEngine>()->get_engine();
  ma_engine_listener_set_velocity(engine, m_ListenerIndex, velocity.x, velocity.y, velocity.z);
}
}
