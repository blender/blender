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
/** \name Curve Struct
 * \{ */

#define _DNA_DEFAULT_Curve \
  { \
    .size = {1, 1, 1}, \
    .flag = CU_DEFORM_BOUNDS_OFF | CU_PATH_RADIUS, \
    .pathlen = 100, \
    .resolu = 12, \
    .resolv = 12, \
    .width = 1.0, \
    .wordspace = 1.0, \
    .spacing = 1.0f, \
    .linedist = 1.0, \
    .fsize = 1.0, \
    .ulheight = 0.05, \
    .texflag = CU_AUTOSPACE, \
    .smallcaps_scale = 0.75f, \
    /* This one seems to be the best one in most cases, at least for curve deform. */ \
    .twist_mode = CU_TWIST_MINIMUM, \
    .bevfac1 = 0.0f, \
    .bevfac2 = 1.0f, \
    .bevfac1_mapping = CU_BEVFAC_MAP_RESOLU, \
    .bevfac2_mapping = CU_BEVFAC_MAP_RESOLU, \
    .bevresol = 4, \
    .bevel_mode = CU_BEV_MODE_ROUND, \
    .taper_radius_mode = CU_TAPER_RADIUS_OVERRIDE, \
  }

/** \} */

/* clang-format on */
