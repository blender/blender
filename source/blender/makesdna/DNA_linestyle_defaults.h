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
