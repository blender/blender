/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

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
    .gpuframenr = IMAGE_GPU_FRAME_NONE, \
    .gpu_pass = IMAGE_GPU_PASS_NONE, \
    .gpu_layer = IMAGE_GPU_LAYER_NONE, \
    .gpu_view = IMAGE_GPU_VIEW_NONE, \
    .seam_margin = 8, \
  }

/** \} */

/* clang-format on */
