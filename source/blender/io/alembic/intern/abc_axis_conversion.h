/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Kévin Dietrich & Blender Foundation */
#pragma once

/** \file
 * \ingroup Alembic
 */

#include "BLI_compiler_compat.h"

struct Object;

namespace blender::io::alembic {

/* TODO(kevin): for now keeping these transformations hardcoded to make sure
 * everything works properly, and also because Alembic is almost exclusively
 * used in Y-up software, but eventually they'll be set by the user in the UI
 * like other importers/exporters do, to support other axis. */

/* Copy from Y-up to Z-up. */

BLI_INLINE void copy_zup_from_yup(float zup[3], const float yup[3])
{
  const float old_yup1 = yup[1]; /* in case zup == yup */
  zup[0] = yup[0];
  zup[1] = -yup[2];
  zup[2] = old_yup1;
}

BLI_INLINE void copy_zup_from_yup(short zup[3], const short yup[3])
{
  const short old_yup1 = yup[1]; /* in case zup == yup */
  zup[0] = yup[0];
  zup[1] = -yup[2];
  zup[2] = old_yup1;
}

/* Copy from Z-up to Y-up. */

BLI_INLINE void copy_yup_from_zup(float yup[3], const float zup[3])
{
  const float old_zup1 = zup[1]; /* in case yup == zup */
  yup[0] = zup[0];
  yup[1] = zup[2];
  yup[2] = -old_zup1;
}

BLI_INLINE void copy_yup_from_zup(short yup[3], const short zup[3])
{
  const short old_zup1 = zup[1]; /* in case yup == zup */
  yup[0] = zup[0];
  yup[1] = zup[2];
  yup[2] = -old_zup1;
}

/* Names are given in (dst, src) order, just like
 * the parameters of copy_m44_axis_swap(). */

typedef enum {
  ABC_ZUP_FROM_YUP = 1,
  ABC_YUP_FROM_ZUP = 2,
} AbcAxisSwapMode;

/**
 * Create a rotation matrix for each axis from euler angles.
 * Euler angles are swapped to change coordinate system.
 */
void create_swapped_rotation_matrix(float rot_x_mat[3][3],
                                    float rot_y_mat[3][3],
                                    float rot_z_mat[3][3],
                                    const float euler[3],
                                    AbcAxisSwapMode mode);

/**
 * Convert matrix from Z=up to Y=up or vice versa.
 * Use yup_mat = zup_mat for in-place conversion.
 */
void copy_m44_axis_swap(float dst_mat[4][4], float src_mat[4][4], AbcAxisSwapMode mode);

typedef enum {
  ABC_MATRIX_WORLD = 1,
  ABC_MATRIX_LOCAL = 2,
} AbcMatrixMode;

/**
 * Recompute transform matrix of object in new coordinate system
 * (from Z-Up to Y-Up).
 */
void create_transform_matrix(Object *obj,
                             float r_yup_mat[4][4],
                             AbcMatrixMode mode,
                             Object *proxy_from);

}  // namespace blender::io::alembic
