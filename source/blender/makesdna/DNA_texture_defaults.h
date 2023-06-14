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
/** \name Texture Struct
 * \{ */

#define _DNA_DEFAULT_MTex \
  { \
    .texco = TEXCO_UV, \
    .mapto = MAP_COL, \
    .object = NULL, \
    .projx = PROJ_X, \
    .projy = PROJ_Y, \
    .projz = PROJ_Z, \
    .mapping = MTEX_FLAT, \
    .ofs[0] = 0.0, \
    .ofs[1] = 0.0, \
    .ofs[2] = 0.0, \
    .size[0] = 1.0, \
    .size[1] = 1.0, \
    .size[2] = 1.0, \
    .tex = NULL, \
    .r = 1.0, \
    .g = 0.0, \
    .b = 1.0, \
    .k = 1.0, \
    .def_var = 1.0, \
    .blendtype = MTEX_BLEND, \
    .colfac = 1.0, \
    .alphafac = 1.0f, \
    .timefac = 1.0f, \
    .lengthfac = 1.0f, \
    .clumpfac = 1.0f, \
    .kinkfac = 1.0f, \
    .kinkampfac = 1.0f, \
    .roughfac = 1.0f, \
    .twistfac = 1.0f, \
    .padensfac = 1.0f, \
    .lifefac = 1.0f, \
    .sizefac = 1.0f, \
    .ivelfac = 1.0f, \
    .dampfac = 1.0f, \
    .gravityfac = 1.0f, \
    .fieldfac = 1.0f, \
    .brush_map_mode = MTEX_MAP_MODE_TILED, \
    .random_angle = 2.0f * (float)M_PI, \
    .brush_angle_mode = 0, \
  } \

#define _DNA_DEFAULT_Tex \
  { \
    .type = TEX_IMAGE, \
    .ima = NULL, \
    .stype = 0, \
    .flag = TEX_CHECKER_ODD | TEX_NO_CLAMP, \
    .imaflag = TEX_INTERPOL | TEX_MIPMAP | TEX_USEALPHA, \
    .extend = TEX_REPEAT, \
    .cropxmin = 0.0, \
    .cropymin = 0.0, \
    .cropxmax = 1.0, \
    .cropymax = 1.0, \
    .texfilter = TXF_EWA, \
    .afmax = 8, \
    .xrepeat = 1, \
    .yrepeat = 1, \
    .sfra = 1, \
    .frames = 0, \
    .offset = 0, \
    .noisesize = 0.25, \
    .noisedepth = 2, \
    .turbul = 5.0, \
    .nabla = 0.025,  /* also in do_versions. */ \
    .bright = 1.0, \
    .contrast = 1.0, \
    .saturation = 1.0, \
    .filtersize = 1.0, \
    .rfac = 1.0, \
    .gfac = 1.0, \
    .bfac = 1.0, \
    /* newnoise: init. */ \
    .noisebasis = 0, \
    .noisebasis2 = 0, \
    /* musgrave */ \
    .mg_H = 1.0, \
    .mg_lacunarity = 2.0, \
    .mg_octaves = 2.0, \
    .mg_offset = 1.0, \
    .mg_gain = 1.0, \
    .ns_outscale = 1.0, \
    /* distnoise */ \
    .dist_amount = 1.0, \
    /* voronoi */ \
    .vn_w1 = 1.0, \
    .vn_w2 = 0.0, \
    .vn_w3 = 0.0, \
    .vn_w4 = 0.0, \
    .vn_mexp = 2.5, \
    .vn_distm = 0, \
    .vn_coltype = 0, \
    .preview = NULL, \
  }

/** \} */

/* clang-format on */
