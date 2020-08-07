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
 * The Original Code is Copyright (C) 2018 Blender Foundation.
 * All rights reserved.
 */

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
