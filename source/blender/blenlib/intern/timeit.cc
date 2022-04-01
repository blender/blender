/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_timeit.hh"

#include <algorithm>

namespace blender::timeit {

void print_duration(Nanoseconds duration)
{
  if (duration < std::chrono::microseconds(100)) {
    std::cout << duration.count() << " ns";
  }
  else if (duration < std::chrono::seconds(5)) {
    std::cout << duration.count() / 1.0e6 << " ms";
  }
  else {
    std::cout << duration.count() / 1.0e9 << " s";
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
  std::cout << ")\n";
}

}  // namespace blender::timeit
