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
 */

/** \file
 * \ingroup DNA
 */

#pragma once

/* Struct members on own line. */
/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name LightProbe Struct
 * \{ */

#define _DNA_DEFAULT_LightProbe \
  { \
    .grid_resolution_x = 4, \
    .grid_resolution_y = 4, \
    .grid_resolution_z = 4, \
    .distinf = 2.5f, \
    .distpar = 2.5f, \
    .falloff = 0.2f, \
    .clipsta = 0.8f, \
    .clipend = 40.0f, \
    .vis_bias = 1.0f, \
    .vis_blur = 0.2f, \
    .intensity = 1.0f, \
    .flag = LIGHTPROBE_FLAG_SHOW_INFLUENCE, \
  }

/** \} */

/* clang-format on */
