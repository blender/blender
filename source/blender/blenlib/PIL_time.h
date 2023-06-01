/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief Platform independent time functions.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern
    /** Return an indication of time, expressed as
     * seconds since some fixed point. Successive calls
     * are guaranteed to generate values greater than or
     * equal to the last call. */
    double
    PIL_check_seconds_timer(void);

extern
    /** `int` version of #PIL_check_seconds_timer. */
    long int
    PIL_check_seconds_timer_i(void);

/**
 * Platform-independent sleep function.
 * \param ms: Number of milliseconds to sleep
 */
void PIL_sleep_ms(int ms);

#ifdef __cplusplus
}
#endif
