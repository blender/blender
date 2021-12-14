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

#pragma once

/** \file
 * \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Time Constants Definitions
 * \{ */

#define SECONDS_IN_MILLISECONDS 0.001
#define SECONDS_IN_MINUTE 60.0
#define MINUTES_IN_HOUR 60.0
#define HOURS_IN_DAY 24.0

#define MINUTES_IN_DAY (MINUTES_IN_HOUR * HOURS_IN_DAY)
#define SECONDS_IN_DAY (MINUTES_IN_DAY * SECONDS_IN_MINUTE)
#define SECONDS_IN_HOUR (MINUTES_IN_HOUR * SECONDS_IN_MINUTE)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Time API
 * \{ */

/**
 * Explode given time value expressed in seconds, into a set of days, hours, minutes, seconds
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
                                     double *r_milliseconds);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline Definitions
 * \{ */

/* None. */

/** \} */

#ifdef __cplusplus
}
#endif
