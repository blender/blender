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
/** \name Material Struct
 * \{ */

#define _DNA_DEFAULT_Material \
  { \
    .r = 0.8, \
    .g = 0.8, \
    .b = 0.8, \
    .specr = 1.0, \
    .specg = 1.0, \
    .specb = 1.0, \
    .a = 1.0f, \
    .spec = 0.5, \
 \
    .roughness = 0.4f, \
 \
    .pr_type = MA_SPHERE, \
 \
    .alpha_threshold = 0.5f, \
 \
    .blend_shadow = MA_BS_SOLID, \
    \
    .lineart.mat_occlusion = 1, \
  }

/** \} */

/* clang-format on */
