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
/** \name Camera Struct
 * \{ */

#define _DNA_DEFAULT_CameraDOFSettings \
  { \
    .aperture_fstop = 2.8f, \
    .aperture_ratio = 1.0f, \
    .focus_distance = 10.0f, \
  }

#define _DNA_DEFAULT_CameraStereoSettings \
  { \
    .interocular_distance = 0.065f, \
    .convergence_distance = 30.0f * 0.065f, \
    .pole_merge_angle_from = DEG2RADF(60.0f), \
    .pole_merge_angle_to = DEG2RADF(75.0f), \
  }

#define _DNA_DEFAULT_Camera \
  { \
    .lens = 50.0f, \
    .sensor_x = DEFAULT_SENSOR_WIDTH, \
    .sensor_y = DEFAULT_SENSOR_HEIGHT, \
    .clip_start = 0.1f, \
    .clip_end = 1000.0f, \
    .drawsize = 1.0f, \
    .ortho_scale = 6.0, \
    .flag = CAM_SHOWPASSEPARTOUT, \
    .passepartalpha = 0.5f, \
 \
    .dof = _DNA_DEFAULT_CameraDOFSettings, \
 \
    .stereo = _DNA_DEFAULT_CameraStereoSettings, \
  }

/** \} */

/* clang-format on */
