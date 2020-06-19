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
 * \ingroup Alembic
 */

#include "abc_axis_conversion.h"

extern "C" {
#include "BLI_assert.h"
#include "DNA_object_types.h"

#include "BLI_math_geom.h"
}

namespace blender {
namespace io {
namespace alembic {

void create_swapped_rotation_matrix(float rot_x_mat[3][3],
                                    float rot_y_mat[3][3],
                                    float rot_z_mat[3][3],
                                    const float euler[3],
                                    AbcAxisSwapMode mode)
{
  const float rx = euler[0];
  float ry;
  float rz;

  /* Apply transformation */
  switch (mode) {
    case ABC_ZUP_FROM_YUP:
      ry = -euler[2];
      rz = euler[1];
      break;
    case ABC_YUP_FROM_ZUP:
      ry = euler[2];
      rz = -euler[1];
      break;
    default:
      ry = 0.0f;
      rz = 0.0f;
      BLI_assert(false);
      break;
  }

  unit_m3(rot_x_mat);
  unit_m3(rot_y_mat);
  unit_m3(rot_z_mat);

  rot_x_mat[1][1] = cos(rx);
  rot_x_mat[2][1] = -sin(rx);
  rot_x_mat[1][2] = sin(rx);
  rot_x_mat[2][2] = cos(rx);

  rot_y_mat[2][2] = cos(ry);
  rot_y_mat[0][2] = -sin(ry);
  rot_y_mat[2][0] = sin(ry);
  rot_y_mat[0][0] = cos(ry);

  rot_z_mat[0][0] = cos(rz);
  rot_z_mat[1][0] = -sin(rz);
  rot_z_mat[0][1] = sin(rz);
  rot_z_mat[1][1] = cos(rz);
}  // namespace
   // alembicvoidcreate_swapped_rotation_matrix(floatrot_x_mat[3][3],floatrot_y_mat[3][3],floatrot_z_mat[3][3],constfloateuler[3],AbcAxisSwapModemode)

/* Convert matrix from Z=up to Y=up or vice versa.
 * Use yup_mat = zup_mat for in-place conversion. */
void copy_m44_axis_swap(float dst_mat[4][4], float src_mat[4][4], AbcAxisSwapMode mode)
{
  float dst_rot[3][3], src_rot[3][3], dst_scale_mat[4][4];
  float rot_x_mat[3][3], rot_y_mat[3][3], rot_z_mat[3][3];
  float src_trans[3], dst_scale[3], src_scale[3], euler[3];

  zero_v3(src_trans);
  zero_v3(dst_scale);
  zero_v3(src_scale);
  zero_v3(euler);
  unit_m3(src_rot);
  unit_m3(dst_rot);
  unit_m4(dst_scale_mat);

  /* TODO(Sybren): This code assumes there is no sheer component and no
   * homogeneous scaling component, which is not always true when writing
   * non-hierarchical (e.g. flat) objects (e.g. when parent has non-uniform
   * scale and the child rotates). This is currently not taken into account
   * when axis-swapping. */

  /* Extract translation, rotation, and scale form matrix. */
  mat4_to_loc_rot_size(src_trans, src_rot, src_scale, src_mat);

  /* Get euler angles from rotation matrix. */
  mat3_to_eulO(euler, ROT_MODE_XZY, src_rot);

  /* Create X, Y, Z rotation matrices from euler angles. */
  create_swapped_rotation_matrix(rot_x_mat, rot_y_mat, rot_z_mat, euler, mode);

  /* Concatenate rotation matrices. */
  mul_m3_m3m3(dst_rot, dst_rot, rot_z_mat);
  mul_m3_m3m3(dst_rot, dst_rot, rot_y_mat);
  mul_m3_m3m3(dst_rot, dst_rot, rot_x_mat);

  mat3_to_eulO(euler, ROT_MODE_XZY, dst_rot);

  /* Start construction of dst_mat from rotation matrix */
  unit_m4(dst_mat);
  copy_m4_m3(dst_mat, dst_rot);

  /* Apply translation */
  switch (mode) {
    case ABC_ZUP_FROM_YUP:
      copy_zup_from_yup(dst_mat[3], src_trans);
      break;
    case ABC_YUP_FROM_ZUP:
      copy_yup_from_zup(dst_mat[3], src_trans);
      break;
    default:
      BLI_assert(false);
  }

  /* Apply scale matrix. Swaps y and z, but does not
   * negate like translation does. */
  dst_scale[0] = src_scale[0];
  dst_scale[1] = src_scale[2];
  dst_scale[2] = src_scale[1];

  size_to_mat4(dst_scale_mat, dst_scale);
  mul_m4_m4m4(dst_mat, dst_mat, dst_scale_mat);
}

/* Recompute transform matrix of object in new coordinate system
 * (from Z-Up to Y-Up). */
void create_transform_matrix(Object *obj,
                             float r_yup_mat[4][4],
                             AbcMatrixMode mode,
                             Object *proxy_from)
{
  float zup_mat[4][4];

  /* get local or world matrix. */
  if (mode == ABC_MATRIX_LOCAL && obj->parent) {
    /* Note that this produces another matrix than the local matrix, due to
     * constraints and modifiers as well as the obj->parentinv matrix. */
    invert_m4_m4(obj->parent->imat, obj->parent->obmat);
    mul_m4_m4m4(zup_mat, obj->parent->imat, obj->obmat);
  }
  else {
    copy_m4_m4(zup_mat, obj->obmat);
  }

  if (proxy_from) {
    mul_m4_m4m4(zup_mat, proxy_from->obmat, zup_mat);
  }

  copy_m44_axis_swap(r_yup_mat, zup_mat, ABC_YUP_FROM_ZUP);
}

}  // namespace alembic
}  // namespace io
}  // namespace blender