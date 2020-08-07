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
    .colormodel = 0, \
    .r = 1.0, \
    .g = 0.0, \
    .b = 1.0, \
    .k = 1.0, \
    .def_var = 1.0, \
    .blendtype = MTEX_BLEND, \
    .colfac = 1.0, \
    .norfac = 1.0, \
    .varfac = 1.0, \
    .dispfac = 0.2, \
    .colspecfac = 1.0f, \
    .mirrfac = 1.0f, \
    .alphafac = 1.0f, \
    .difffac = 1.0f, \
    .specfac = 1.0f, \
    .emitfac = 1.0f, \
    .hardfac = 1.0f, \
    .raymirrfac = 1.0f, \
    .translfac = 1.0f, \
    .ambfac = 1.0f, \
    .colemitfac = 1.0f, \
    .colreflfac = 1.0f, \
    .coltransfac = 1.0f, \
    .densfac = 1.0f, \
    .scatterfac = 1.0f, \
    .reflfac = 1.0f, \
    .shadowfac = 1.0f, \
    .zenupfac = 1.0f, \
    .zendownfac = 1.0f, \
    .blendfac = 1.0f, \
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
    .normapspace = MTEX_NSPACE_TANGENT, \
    .brush_map_mode = MTEX_MAP_MODE_TILED, \
    .random_angle = 2.0f * (float)M_PI, \
    .brush_angle_mode = 0, \
  } \

#define _DNA_DEFAULT_Tex \
  { \
    .type = TEX_IMAGE, \
    .ima = NULL, \
    .stype = 0, \
    .flag = TEX_CHECKER_ODD, \
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
