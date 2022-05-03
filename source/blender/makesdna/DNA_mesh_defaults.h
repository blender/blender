/* SPDX-License-Identifier: GPL-2.0-or-later */

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
    .flag = ME_REMESH_REPROJECT_VOLUME | ME_REMESH_REPROJECT_PAINT_MASK | ME_REMESH_REPROJECT_SCULPT_FACE_SETS | ME_REMESH_REPROJECT_VERTEX_COLORS, \
    .editflag = ME_EDIT_MIRROR_VERTEX_GROUPS \
  }

/** \} */

/* clang-format on */
