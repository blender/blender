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
