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
    .seam_margin = 8, \
  }

/** \} */

/* clang-format on */
