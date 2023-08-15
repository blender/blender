/* SPDX-FileCopyrightText: 2021 Blender Foundation
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
  BLI_assert(r_days != NULL || r_hours != NULL || r_minutes != NULL || r_seconds != NULL ||
             r_milliseconds != NULL);

  if (r_days != NULL) {
    seconds = modf(seconds / SECONDS_IN_DAY, r_days) * SECONDS_IN_DAY;
  }
  if (r_hours != NULL) {
    seconds = modf(seconds / SECONDS_IN_HOUR, r_hours) * SECONDS_IN_HOUR;
  }
  if (r_minutes != NULL) {
    seconds = modf(seconds / SECONDS_IN_MINUTE, r_minutes) * SECONDS_IN_MINUTE;
  }
  if (r_seconds != NULL) {
    seconds = modf(seconds, r_seconds);
  }
  if (r_milliseconds != NULL) {
    *r_milliseconds = seconds / SECONDS_IN_MILLISECONDS;
  }
  else if (r_seconds != NULL) {
    *r_seconds += seconds;
  }
  else if (r_minutes != NULL) {
    *r_minutes += seconds / SECONDS_IN_MINUTE;
  }
  else if (r_hours != NULL) {
    *r_hours += seconds / SECONDS_IN_HOUR;
  }
  else if (r_days != NULL) {
    *r_days = seconds / SECONDS_IN_DAY;
  }
}
