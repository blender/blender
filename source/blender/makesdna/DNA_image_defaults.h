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
/** \name Image Struct
 * \{ */

#define _DNA_DEFAULT_Image \
  { \
    .aspx = 1.0, \
    .aspy = 1.0, \
    .gen_x = 1024, \
    .gen_y = 1024, \
    .gen_type = IMA_GENTYPE_GRID, \
 \
    .gpuframenr = INT_MAX, \
    .gpu_pass = SHRT_MAX, \
    .gpu_layer = SHRT_MAX, \
  }

/** \} */

/* clang-format on */
