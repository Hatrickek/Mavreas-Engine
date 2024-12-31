#include "LuaAudioBindings.hpp"

#include <sol/state.hpp>

#include "LuaHelpers.hpp"

#include "Assets/AssetManager.hpp"

#include "Audio/AudioSource.hpp"

#include "Scene/Components.hpp"
#include "Scene/Entity.hpp"

namespace ox::LuaBindings {
void bind_audio(const Shared<sol::state>& state) {
  auto audio_source = state->new_usertype<AudioSource>("AudioSource");
  SET_TYPE_FUNCTION(audio_source, AudioSource, get_path);
  SET_TYPE_FUNCTION(audio_source, AudioSource, play);
  SET_TYPE_FUNCTION(audio_source, AudioSource, pause);
  SET_TYPE_FUNCTION(audio_source, AudioSource, un_pause);
  SET_TYPE_FUNCTION(audio_source, AudioSource, stop);
  SET_TYPE_FUNCTION(audio_source, AudioSource, is_playing);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_config);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_volume);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_pitch);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_looping);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_spatialization);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_attenuation_model);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_roll_off);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_min_gain);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_max_gain);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_min_distance);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_max_distance);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_cone);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_doppler_factor);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_position);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_direction);
  SET_TYPE_FUNCTION(audio_source, AudioSource, set_velocity);

#define ASC AudioSourceComponent
  REGISTER_COMPONENT(state, ASC, FIELD(ASC, config), FIELD(ASC, source));

  const std::initializer_list<std::pair<sol::string_view, AttenuationModelType>> attenuation_model_type = {
    ENUM_FIELD(AttenuationModelType, Inverse),
    ENUM_FIELD(AttenuationModelType, Linear),
    ENUM_FIELD(AttenuationModelType, Exponential),
  };
  state->new_enum<AttenuationModelType, true>("AttenuationModelType", attenuation_model_type);

  auto audio_source_config_type = state->new_usertype<AudioSourceConfig>("AudioSourceConfig");
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, volume_multiplier);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, pitch_multiplier);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, play_on_awake);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, looping);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, spatialization);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, attenuation_model);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, roll_off);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, min_gain);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, max_gain);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, min_distance);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, max_distance);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, cone_inner_angle);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, cone_outer_angle);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, cone_outer_gain);
  SET_TYPE_FIELD(audio_source_config_type, AudioSourceConfig, doppler_factor);

#define ALC AudioListenerComponent
  REGISTER_COMPONENT(state, ALC, FIELD(ALC, active), FIELD(ALC, config), FIELD(ALC, listener));

  sol::usertype<AudioListenerConfig> audio_listener_config_type = state->new_usertype<AudioListenerConfig>("AudioListenerConfig");
  SET_TYPE_FIELD(audio_listener_config_type, AudioListenerConfig, cone_inner_angle);
  SET_TYPE_FIELD(audio_listener_config_type, AudioListenerConfig, cone_outer_angle);
  SET_TYPE_FIELD(audio_listener_config_type, AudioListenerConfig, cone_outer_gain);
}
}
