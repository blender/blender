/* SPDX-License-Identifier: GPL-2.0-or-later */

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

}  // namespace blender::timeit

#define SCOPED_TIMER(name) blender::timeit::ScopedTimer scoped_timer(name)
