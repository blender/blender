/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

/* clang-format off */

/* -------------------------------------------------------------------- */
/** \name World Struct
 * \{ */

#define _DNA_DEFAULT_World \
  { \
    .flag = WO_USE_SUN_SHADOW, \
  \
    .horr = 0.05f, \
    .horg = 0.05f, \
    .horb = 0.05f, \
  \
    .aodist = 10.0f, \
    .aoenergy = 1.0f, \
  \
    .preview = NULL, \
    .miststa = 5.0f, \
    .mistdist = 25.0f, \
  \
    .probe_resolution = LIGHT_PROBE_RESOLUTION_1024, \
    .sun_threshold = 10.0f, \
    .sun_angle = DEG2RADF(0.526f), \
    .sun_shadow_maximum_resolution = 0.001f, \
  }

/** \} */

/* clang-format on */
