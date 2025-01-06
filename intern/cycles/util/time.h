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
