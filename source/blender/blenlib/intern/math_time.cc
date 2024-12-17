/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_math_base.h"
#include "BLI_math_time.h"

void BLI_math_time_seconds_decompose(double seconds,
                                     double *r_days,
                                     double *r_hours,
                                     double *r_minutes,
                                     double *r_seconds,
                                     double *r_milliseconds)
{
  BLI_assert(r_days != nullptr || r_hours != nullptr || r_minutes != nullptr ||
             r_seconds != nullptr || r_milliseconds != nullptr);

  if (r_days != nullptr) {
    seconds = modf(seconds / SECONDS_IN_DAY, r_days) * SECONDS_IN_DAY;
  }
  if (r_hours != nullptr) {
    seconds = modf(seconds / SECONDS_IN_HOUR, r_hours) * SECONDS_IN_HOUR;
  }
  if (r_minutes != nullptr) {
    seconds = modf(seconds / SECONDS_IN_MINUTE, r_minutes) * SECONDS_IN_MINUTE;
  }
  if (r_seconds != nullptr) {
    seconds = modf(seconds, r_seconds);
  }
  if (r_milliseconds != nullptr) {
    *r_milliseconds = seconds / SECONDS_IN_MILLISECONDS;
  }
  else if (r_seconds != nullptr) {
    *r_seconds += seconds;
  }
  else if (r_minutes != nullptr) {
    *r_minutes += seconds / SECONDS_IN_MINUTE;
  }
  else if (r_hours != nullptr) {
    *r_hours += seconds / SECONDS_IN_HOUR;
  }
  else if (r_days != nullptr) {
    *r_days = seconds / SECONDS_IN_DAY;
  }
}
