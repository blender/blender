/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief Platform independent time functions.
 */

#pragma once

/**
 * Return an indication of time, expressed as seconds since some fixed point.
 * Successive calls are guaranteed to generate values greater than or equal to the last call.
 */
extern double BLI_time_now_seconds(void);

/** `int` version of #BLI_time_now_seconds. */
extern long int BLI_time_now_seconds_i(void);

/**
 * Platform-independent sleep function.
 * \param ms: Number of milliseconds to sleep
 */
void BLI_time_sleep_ms(int ms);

/**
 * Platform-independent high-resolution sleep function.
 * Using this function can have advantages over \see BLI_time_sleep_ms on Windows due to a default
 * non-precise sleep resolution of 15.25ms.
 * \param us: Number of microseconds to sleep
 */
void BLI_time_sleep_precise_us(int us);
