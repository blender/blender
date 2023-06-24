/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
    .grid_bake_samples = 2048, \
    .surfel_density = 1.0f, \
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
