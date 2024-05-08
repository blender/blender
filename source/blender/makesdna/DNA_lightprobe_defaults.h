/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name LightProbe Struct
 * \{ */

#define _DNA_DEFAULT_LightProbe \
  { \
    .grid_resolution_x = 4, \
    .grid_resolution_y = 4, \
    .grid_resolution_z = 4, \
    .grid_bake_samples = 2048, \
    .grid_surface_bias = 0.05, \
    .grid_escape_bias = 0.1, \
    .grid_normal_bias = 0.3f, \
    .grid_view_bias = 0.0f, \
    .grid_facing_bias = 0.5f, \
    .grid_validity_threshold = 0.40f, \
    .grid_dilation_threshold = 0.5f, \
    .grid_dilation_radius = 1.0f, \
    .grid_clamp_direct = 0.0f, \
    .grid_clamp_indirect = 10.0f, \
    .grid_surfel_density = 20, \
    .distinf = 2.5f, \
    .distpar = 2.5f, \
    .falloff = 0.2f, \
    .clipsta = 0.8f, \
    .clipend = 40.0f, \
    .vis_bias = 1.0f, \
    .vis_blur = 0.2f, \
    .intensity = 1.0f, \
    .flag = LIGHTPROBE_FLAG_SHOW_INFLUENCE, \
    .grid_flag = LIGHTPROBE_GRID_CAPTURE_INDIRECT | LIGHTPROBE_GRID_CAPTURE_EMISSION, \
    .data_display_size = 0.1f, \
  }

/** \} */

/* clang-format on */
