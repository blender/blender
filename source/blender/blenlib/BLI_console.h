/* SPDX-FileCopyrightText: 2018 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** \file
 * \ingroup bli
 * \brief Set of utility functions and constants to work with consoles.
 */

/* Format string where one could BLI_snprintf() R, G and B values
 * and get proper marker to start colored output in the console.
 */
#define TRUECOLOR_ANSI_COLOR_FORMAT "\x1b[38;2;%d;%d;%dm"

/* Marker which indicates that colored output is finished. */
#define TRUECOLOR_ANSI_COLOR_FINISH "\x1b[0m"

#ifdef __cplusplus
}
#endif
