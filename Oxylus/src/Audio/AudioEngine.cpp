#include "AudioEngine.hpp"

#define MINIAUDIO_IMPLEMENTATION

#include <miniaudio.h>

#include "Utils/Log.hpp"
#include "Utils/Profiler.hpp"

namespace ox {
void AudioEngine::init() {
  OX_SCOPED_ZONE;
  ma_engine_config config = ma_engine_config_init();
  config.listenerCount = 1;

  engine = new ma_engine();
  const ma_result result = ma_engine_init(&config, engine);
  OX_CHECK_EQ(result, MA_SUCCESS, "Failed to initialize audio engine!");

  OX_LOG_INFO("Initalized audio engine.");
}

void AudioEngine::deinit() {
  ma_engine_uninit(engine);
  delete engine;
}

ma_engine* AudioEngine::get_engine() const {
  return engine;
}
}
