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
/** \name Mesh Struct
 * \{ */

#define _DNA_DEFAULT_Mesh \
  { \
    .size = {1.0f, 1.0f, 1.0f}, \
    .smoothresh = DEG2RADF(30), \
    .texflag = ME_AUTOSPACE, \
    .remesh_voxel_size = 0.1f, \
    .remesh_voxel_adaptivity = 0.0f, \
    .face_sets_color_seed = 0, \
    .face_sets_color_default = 1, \
    .flag = ME_REMESH_FIX_POLES | ME_REMESH_REPROJECT_VOLUME, \
    .editflag = ME_EDIT_MIRROR_VERTEX_GROUPS \
  }

/** \} */

/* clang-format on */
