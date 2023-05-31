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
/** \name Speaker Struct
 * \{ */

#define _DNA_DEFAULT_Speaker \
  { \
    .attenuation = 1.0f, \
    .cone_angle_inner = 360.0f, \
    .cone_angle_outer = 360.0f, \
    .cone_volume_outer = 1.0f, \
    .distance_max = FLT_MAX, \
    .distance_reference = 1.0f, \
    .flag = 0, \
    .pitch = 1.0f, \
    .sound = NULL, \
    .volume = 1.0f, \
    .volume_max = 1.0f, \
    .volume_min = 0.0f, \
  }

/** \} */

/* clang-format on */
