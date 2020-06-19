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
 *
 * The Original Code is Copyright (C) 2016 Kévin Dietrich & Blender Foundation.
 * All rights reserved.
 */
#pragma once

/** \file
 * \ingroup Alembic
 */

struct Object;

#ifdef _MSC_VER
#  define ABC_INLINE static __forceinline
#else
#  define ABC_INLINE static inline
#endif

namespace blender {
namespace io {
namespace alembic {

/* TODO(kevin): for now keeping these transformations hardcoded to make sure
 * everything works properly, and also because Alembic is almost exclusively
 * used in Y-up software, but eventually they'll be set by the user in the UI
 * like other importers/exporters do, to support other axis. */

/* Copy from Y-up to Z-up. */

ABC_INLINE void copy_zup_from_yup(float zup[3], const float yup[3])
{
  const float old_yup1 = yup[1]; /* in case zup == yup */
  zup[0] = yup[0];
  zup[1] = -yup[2];
  zup[2] = old_yup1;
}

ABC_INLINE void copy_zup_from_yup(short zup[3], const short yup[3])
{
  const short old_yup1 = yup[1]; /* in case zup == yup */
  zup[0] = yup[0];
  zup[1] = -yup[2];
  zup[2] = old_yup1;
}

/* Copy from Z-up to Y-up. */

ABC_INLINE void copy_yup_from_zup(float yup[3], const float zup[3])
{
  const float old_zup1 = zup[1]; /* in case yup == zup */
  yup[0] = zup[0];
  yup[1] = zup[2];
  yup[2] = -old_zup1;
}

ABC_INLINE void copy_yup_from_zup(short yup[3], const short zup[3])
{
  const short old_zup1 = zup[1]; /* in case yup == zup */
  yup[0] = zup[0];
  yup[1] = zup[2];
  yup[2] = -old_zup1;
}

/* Names are given in (dst, src) order, just like
 * the parameters of copy_m44_axis_swap() */
typedef enum {
  ABC_ZUP_FROM_YUP = 1,
  ABC_YUP_FROM_ZUP = 2,
} AbcAxisSwapMode;

/* Create a rotation matrix for each axis from euler angles.
 * Euler angles are swapped to change coordinate system. */
void create_swapped_rotation_matrix(float rot_x_mat[3][3],
                                    float rot_y_mat[3][3],
                                    float rot_z_mat[3][3],
                                    const float euler[3],
                                    AbcAxisSwapMode mode);

void copy_m44_axis_swap(float dst_mat[4][4], float src_mat[4][4], AbcAxisSwapMode mode);

typedef enum {
  ABC_MATRIX_WORLD = 1,
  ABC_MATRIX_LOCAL = 2,
} AbcMatrixMode;

void create_transform_matrix(Object *obj,
                             float r_transform_mat[4][4],
                             AbcMatrixMode mode,
                             Object *proxy_from);

}  // namespace alembic
}  // namespace io
}  // namespace blender