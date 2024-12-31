#include "AudioSource.hpp"

#include <miniaudio.h>

#include "AudioEngine.hpp"

#include "Core/App.hpp"

#include "Utils/Log.hpp"

namespace ox {
AudioSource::AudioSource(const std::string& filepath) {
  m_sound = create_unique<ma_sound>();

  auto* engine = App::get_system<AudioEngine>()->get_engine();
  const ma_result result = ma_sound_init_from_file(engine,
                                                   filepath.c_str(),
                                                   MA_SOUND_FLAG_NO_SPATIALIZATION,
                                                   nullptr,
                                                   nullptr,
                                                   m_sound.get());
  if (result != MA_SUCCESS)
    OX_LOG_ERROR("Failed to load sound: {}", filepath);
}

AudioSource::~AudioSource() {
  ma_sound_uninit(m_sound.get());
  m_sound = nullptr;
}

void AudioSource::play() const {
  ma_sound_seek_to_pcm_frame(m_sound.get(), 0);
  ma_sound_start(m_sound.get());
}

void AudioSource::pause() const {
  ma_sound_stop(m_sound.get());
}

void AudioSource::un_pause() const {
  ma_sound_start(m_sound.get());
}

void AudioSource::stop() const {
  ma_sound_stop(m_sound.get());
  ma_sound_seek_to_pcm_frame(m_sound.get(), 0);
}

bool AudioSource::is_playing() const {
  return ma_sound_is_playing(m_sound.get());
}

static ma_attenuation_model GetAttenuationModel(const AttenuationModelType model) {
  switch (model) {
    case AttenuationModelType::None: return ma_attenuation_model_none;
    case AttenuationModelType::Inverse: return ma_attenuation_model_inverse;
    case AttenuationModelType::Linear: return ma_attenuation_model_linear;
    case AttenuationModelType::Exponential: return ma_attenuation_model_exponential;
  }

  return ma_attenuation_model_none;
}

void AudioSource::set_config(const AudioSourceConfig& config) {
  ma_sound* sound = m_sound.get();
  ma_sound_set_volume(sound, config.volume_multiplier);
  ma_sound_set_pitch(sound, config.pitch_multiplier);
  ma_sound_set_looping(sound, config.looping);

  if (m_spatialization != config.spatialization) {
    m_spatialization = config.spatialization;
    ma_sound_set_spatialization_enabled(sound, config.spatialization);
  }

  if (config.spatialization) {
    ma_sound_set_attenuation_model(sound, GetAttenuationModel(config.attenuation_model));
    ma_sound_set_rolloff(sound, config.roll_off);
    ma_sound_set_min_gain(sound, config.min_gain);
    ma_sound_set_max_gain(sound, config.max_gain);
    ma_sound_set_min_distance(sound, config.min_distance);
    ma_sound_set_max_distance(sound, config.max_distance);

    ma_sound_set_cone(sound, config.cone_inner_angle, config.cone_outer_angle, config.cone_outer_gain);
    ma_sound_set_doppler_factor(sound, glm::max(config.doppler_factor, 0.0f));
  }
  else {
    ma_sound_set_attenuation_model(sound, ma_attenuation_model_none);
  }
}

void AudioSource::set_volume(float volume) const {
  ma_sound_set_volume(m_sound.get(), volume);
}

void AudioSource::set_pitch(float pitch) const {
  ma_sound_set_pitch(m_sound.get(), pitch);
}

void AudioSource::set_looping(const bool state) const {
  ma_sound_set_looping(m_sound.get(), state);
}

void AudioSource::set_spatialization(const bool state) {
  m_spatialization = state;
  ma_sound_set_spatialization_enabled(m_sound.get(), state);
}

void AudioSource::set_attenuation_model(const AttenuationModelType type) const {
  if (m_spatialization)
    ma_sound_set_attenuation_model(m_sound.get(), GetAttenuationModel(type));
  else
    ma_sound_set_attenuation_model(m_sound.get(), GetAttenuationModel(AttenuationModelType::None));
}

void AudioSource::set_roll_off(const float rollOff) const {
  ma_sound_set_rolloff(m_sound.get(), rollOff);
}

void AudioSource::set_min_gain(const float minGain) const {
  ma_sound_set_min_gain(m_sound.get(), minGain);
}

void AudioSource::set_max_gain(const float maxGain) const {
  ma_sound_set_max_gain(m_sound.get(), maxGain);
}

void AudioSource::set_min_distance(const float minDistance) const {
  ma_sound_set_min_distance(m_sound.get(), minDistance);
}

void AudioSource::set_max_distance(const float maxDistance) const {
  ma_sound_set_max_distance(m_sound.get(), maxDistance);
}

void AudioSource::set_cone(const float innerAngle, const float outerAngle, const float outerGain) const {
  ma_sound_set_cone(m_sound.get(), innerAngle, outerAngle, outerGain);
}

void AudioSource::set_doppler_factor(const float factor) const {
  ma_sound_set_doppler_factor(m_sound.get(), glm::max(factor, 0.0f));
}

void AudioSource::set_position(const glm::vec3& position) const {
  ma_sound_set_position(m_sound.get(), position.x, position.y, position.z);
}

void AudioSource::set_direction(const glm::vec3& forward) const {
  ma_sound_set_direction(m_sound.get(), forward.x, forward.y, forward.z);
}

void AudioSource::set_velocity(const glm::vec3& velocity) const {
  ma_sound_set_velocity(m_sound.get(), velocity.x, velocity.y, velocity.z);
}
}
