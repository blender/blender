/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2021 by Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bli
 */

#include "BLI_math.h"

/** Explode given time value expressed in seconds, into a set of days, hours, minutes, seconds
 * and/or milliseconds (depending on which return parameters are not NULL).
 *
 * \note The smallest given return parameter will get the potential fractional remaining time
 * value. E.g. if you give `seconds=90.0` and do not pass `r_seconds` and `r_milliseconds`,
 * `r_minutes` will be set to `1.5`.
 */
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
