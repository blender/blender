/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <functional>

#include "util/string.h"

CCL_NAMESPACE_BEGIN

/* Give current time in seconds in double precision, with good accuracy. */

double time_dt();

/* Sleep for the specified number of seconds. */

void time_sleep(const double t);

/* Fast timer for applications where overhead is critical and some inaccuracy is acceptable.
 *
 * On x86, this uses RDTSCP, which also can check which CPU the code runs on, which in turn
 * allows us to skip measurements where we moved CPU in-between (which might be invalid due
 * to different clock states between cores and/or misleading due to OS scheduling). Therefore,
 * we provide last_cpu to time_fast_tick, and it may set it if supported. */

uint64_t time_fast_tick(uint32_t *last_cpu);
uint64_t time_fast_frequency();

/* Scoped timer. */

class scoped_timer {
 public:
  explicit scoped_timer(double *value = nullptr) : value_(value)
  {
    time_start_ = time_dt();
  }

  ~scoped_timer()
  {
    if (value_ != nullptr) {
      *value_ = get_time();
    }
  }

  double get_start() const
  {
    return time_start_;
  }

  double get_time() const
  {
    return time_dt() - time_start_;
  }

 protected:
  double *value_;
  double time_start_;
};

class fast_timer {
 public:
  fast_timer()
  {
    last_cpu = 0;
    last_value = time_fast_tick(&last_cpu);
  }

  bool lap(uint64_t &delta)
  {
    uint32_t new_cpu = 0;
    uint64_t new_value = time_fast_tick(&new_cpu);

    const bool cpu_consistent = new_cpu == last_cpu;
    delta = new_value - last_value;

    last_cpu = new_cpu;
    last_value = new_value;

    return cpu_consistent;
  }

 protected:
  uint32_t last_cpu;
  uint64_t last_value;
};

class scoped_callback_timer {
 public:
  using callback_type = std::function<void(double)>;

  explicit scoped_callback_timer(callback_type cb) : cb(cb) {}

  ~scoped_callback_timer()
  {
    if (cb) {
      cb(timer.get_time());
    }
  }

 protected:
  scoped_timer timer;
  callback_type cb;
};

/* Make human readable string from time, compatible with Blender metadata. */

string time_human_readable_from_seconds(const double seconds);
double time_human_readable_to_seconds(const string &time_string);

CCL_NAMESPACE_END
