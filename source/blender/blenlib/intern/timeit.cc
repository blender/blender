/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_timeit.hh"

#include <algorithm>
#include <iomanip>

namespace blender::timeit {

void print_duration(Nanoseconds duration)
{
  using namespace std::chrono;
  if (duration < microseconds(100)) {
    std::cout << duration.count() << " ns";
  }
  else if (duration < seconds(5)) {
    std::cout << std::fixed << std::setprecision(1) << duration.count() / 1.0e6 << " ms";
  }
  else if (duration > seconds(90)) {
    /* Long durations: print seconds, and also H:m:s */
    const auto dur_hours = duration_cast<hours>(duration);
    const auto dur_mins = duration_cast<minutes>(duration - dur_hours);
    const auto dur_sec = duration_cast<seconds>(duration - dur_hours - dur_mins);
    std::cout << std::fixed << std::setprecision(1) << duration.count() / 1.0e9 << " s ("
              << dur_hours.count() << "H:" << dur_mins.count() << "m:" << dur_sec.count() << "s)";
  }
  else {
    std::cout << std::fixed << std::setprecision(1) << duration.count() / 1.0e9 << " s";
  }
}

ScopedTimerAveraged::~ScopedTimerAveraged()
{
  const TimePoint end = Clock::now();
  const Nanoseconds duration = end - start_;

  total_count_++;
  total_time_ += duration;
  min_time_ = std::min(duration, min_time_);

  std::cout << "Timer '" << name_ << "': (Average: ";
  print_duration(total_time_ / total_count_);
  std::cout << ", Min: ";
  print_duration(min_time_);
  std::cout << ", Last: ";
  print_duration(duration);
  std::cout << ")\n";
}

}  // namespace blender::timeit
