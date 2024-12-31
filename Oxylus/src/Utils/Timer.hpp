#pragma once
#include <chrono>
typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimeStamp;

namespace ox {
class Timer {
public:
  Timer()
    : m_start(now()), m_frequency() {
    m_last_time = m_start;
  }

  ~Timer() = default;

  float get_timed_ms();

  static TimeStamp now();

  static double duration(TimeStamp start, TimeStamp end, double timeResolution = 1.0);
  static float duration(TimeStamp start, TimeStamp end, float timeResolution);

  float get_elapsed_ms() const { return get_elapsed(1000.0f); }
  float get_elapsed_s() const { return get_elapsed(1.0f); }
  double get_elapsed_msd() const { return get_elapsed(1000.0); }
  double get_elapsed_sd() const { return get_elapsed(1.0); }

protected:
  float get_elapsed(const float timeResolution) const { return duration(m_start, now(), timeResolution); }

  double get_elapsed(const double timeResolution = 1.0) const { return duration(m_start, now(), timeResolution); }

  TimeStamp m_start;     // Start of timer
  TimeStamp m_frequency; // Ticks Per Second
  TimeStamp m_last_time; // Last time GetTimedMS was called
};
}
