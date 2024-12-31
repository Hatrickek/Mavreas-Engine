#include "Timer.hpp"

namespace ox {
float Timer::get_timed_ms() {
  const float time = duration(m_last_time, now(), 1000.0f);
  m_last_time = now();
  return time;
}

TimeStamp Timer::now() {
  return std::chrono::high_resolution_clock::now();
}

double Timer::duration(const TimeStamp start, const TimeStamp end, const double timeResolution) {
  return std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count() * timeResolution;
}

float Timer::duration(const TimeStamp start, const TimeStamp end, const float timeResolution) {
  return (float)std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count() * timeResolution;
}
}
