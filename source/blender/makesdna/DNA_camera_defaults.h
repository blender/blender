/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
