/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include "BLI_sys_types.h"

namespace blender::timeit {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Nanoseconds = std::chrono::nanoseconds;

void print_duration(Nanoseconds duration);

class ScopedTimer {
 private:
  std::string name_;
  TimePoint start_;

 public:
  ScopedTimer(std::string name) : name_(std::move(name))
  {
    start_ = Clock::now();
  }

  ~ScopedTimer()
  {
    const TimePoint end = Clock::now();
    const Nanoseconds duration = end - start_;

    std::cout << "Timer '" << name_ << "' took ";
    print_duration(duration);
    std::cout << '\n';
  }
};

class ScopedTimerAveraged {
 private:
  std::string name_;
  TimePoint start_;

  int64_t &total_count_;
  Nanoseconds &total_time_;
  Nanoseconds &min_time_;

 public:
  ScopedTimerAveraged(std::string name,
                      int64_t &total_count,
                      Nanoseconds &total_time,
                      Nanoseconds &min_time)
      : name_(std::move(name)),
        total_count_(total_count),
        total_time_(total_time),
        min_time_(min_time)
  {
    start_ = Clock::now();
  }

  ~ScopedTimerAveraged();
};

}  // namespace blender::timeit

#define SCOPED_TIMER(name) blender::timeit::ScopedTimer scoped_timer(name)

/**
 * Print the average and minimum runtime of the timer's scope.
 * \warning This uses static variables, so it is not thread-safe.
 */
#define SCOPED_TIMER_AVERAGED(name) \
  static int64_t total_count_; \
  static blender::timeit::Nanoseconds total_time_; \
  static blender::timeit::Nanoseconds min_time_ = blender::timeit::Nanoseconds::max(); \
  blender::timeit::ScopedTimerAveraged scoped_timer(name, total_count_, total_time_, min_time_)
