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
/** \name FreestyleLineStyle Struct
 * \{ */

#define _DNA_DEFAULT_FreestyleLineStyle \
  { \
    .panel = LS_PANEL_STROKES, \
    .r = 0, \
    .g = 0, \
    .b = 0, \
    .alpha = 1.0f, \
    .thickness = 3.0f, \
    .thickness_position = LS_THICKNESS_CENTER, \
    .thickness_ratio = 0.5f, \
    .flag = LS_SAME_OBJECT | LS_NO_SORTING | LS_TEXTURE, \
    .chaining = LS_CHAINING_PLAIN, \
    .rounds = 3, \
    .min_angle = DEG2RADF(0.0f), \
    .max_angle = DEG2RADF(0.0f), \
    .min_length = 0.0f, \
    .max_length = 10000.0f, \
    .split_length = 100, \
    .chain_count = 10, \
    .sort_key = LS_SORT_KEY_DISTANCE_FROM_CAMERA, \
    .integration_type = LS_INTEGRATION_MEAN, \
    .texstep = 1.0f, \
    .pr_texture = TEX_PR_TEXTURE, \
    .caps = LS_CAPS_BUTT, \
  }

/** \} */

/* clang-format on */
