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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

#include "BCSampleData.h"
#include "collada_utils.h"

BCSample::~BCSample()
{
  BCBoneMatrixMap::iterator it;
  for (it = bonemats.begin(); it != bonemats.end(); ++it) {
    delete it->second;
  }
}

void BCSample::add_bone_matrix(Bone *bone, Matrix &mat)
{
  BCMatrix *matrix;
  BCBoneMatrixMap::const_iterator it = bonemats.find(bone);
  if (it != bonemats.end()) {
    throw std::invalid_argument("bone " + std::string(bone->name) + " already defined before");
  }
  matrix = new BCMatrix(mat);
  bonemats[bone] = matrix;
}

BCMatrix::BCMatrix(const BCMatrix &mat)
{
  set_transform(mat.matrix);
}

BCMatrix::BCMatrix(Matrix &mat)
{
  set_transform(mat);
}

BCMatrix::BCMatrix(Object *ob)
{
  set_transform(ob);
}

BCMatrix::BCMatrix()
{
  unit();
}

BCMatrix::BCMatrix(BC_global_forward_axis global_forward_axis, BC_global_up_axis global_up_axis)
{
  float mrot[3][3];
  float mat[4][4];
  mat3_from_axis_conversion(
      BC_DEFAULT_FORWARD, BC_DEFAULT_UP, global_forward_axis, global_up_axis, mrot);

  transpose_m3(mrot); // TODO: Verify that mat3_from_axis_conversion() returns a transposed matrix
  copy_m4_m3(mat, mrot);
  set_transform(mat);
}

void BCMatrix::add_transform(const Matrix &mat, bool inverse)
{
  add_transform(this->matrix, mat, this->matrix, inverse);
}

void BCMatrix::add_transform(const BCMatrix &mat, bool inverse)
{
  add_transform(this->matrix, mat.matrix, this->matrix, inverse);
}

void BCMatrix::apply_transform(const BCMatrix &mat, bool inverse)
{
  apply_transform(this->matrix, mat.matrix, this->matrix, inverse);
}

void BCMatrix::add_transform(Matrix &to, const Matrix &transform, const Matrix &from, bool inverse)
{
  if (inverse) {
    Matrix globinv;
    invert_m4_m4(globinv, transform);
    add_transform(to, globinv, from, /*inverse=*/false);
  }
  else {
    mul_m4_m4m4(to, transform, from);
  }
}

void BCMatrix::apply_transform(Matrix &to, const Matrix &transform, const Matrix &from, bool inverse)
{
  Matrix globinv;
  invert_m4_m4(globinv, transform);
  if (inverse) {
    add_transform(to, globinv, from, /*inverse=*/false);
  }
  else {
    mul_m4_m4m4(to, transform, from);
    mul_m4_m4m4(to, to, globinv);
  }
}

void BCMatrix::add_inverted_transform(Matrix &to, const Matrix &transform, const Matrix &from)
{
  Matrix workmat;
  invert_m4_m4(workmat, transform);
  mul_m4_m4m4(to, workmat, from);
}

void BCMatrix::set_transform(Object *ob)
{
  Matrix lmat;

  BKE_object_matrix_local_get(ob, lmat);
  copy_m4_m4(matrix, lmat);

  mat4_decompose(this->loc, this->q, this->size, lmat);
  quat_to_compatible_eul(this->rot, ob->rot, this->q);
}

void BCMatrix::set_transform(Matrix &mat)
{
  copy_m4_m4(matrix, mat);
  mat4_decompose(this->loc, this->q, this->size, mat);
  quat_to_eul(this->rot, this->q);
}

const BCMatrix *BCSample::get_matrix(Bone *bone) const
{
  BCBoneMatrixMap::const_iterator it = bonemats.find(bone);
  if (it == bonemats.end()) {
    return NULL;
  }
  return it->second;
}

const BCMatrix &BCSample::get_matrix() const
{
  return obmat;
}

/* Get channel value */
const bool BCSample::get_value(std::string channel_target, const int array_index, float *val) const
{
  if (channel_target == "location") {
    *val = obmat.location()[array_index];
  }
  else if (channel_target == "scale") {
    *val = obmat.scale()[array_index];
  }
  else if (channel_target == "rotation" || channel_target == "rotation_euler") {
    *val = obmat.rotation()[array_index];
  }
  else if (channel_target == "rotation_quat") {
    *val = obmat.quat()[array_index];
  }
  else {
    *val = 0;
    return false;
  }

  return true;
}

void BCMatrix::copy(Matrix &out, Matrix &in)
{
  /* destination comes first: */
  memcpy(out, in, sizeof(Matrix));
}

void BCMatrix::transpose(Matrix &mat)
{
  transpose_m4(mat);
}

void BCMatrix::sanitize(Matrix &mat, int precision)
{
  bc_sanitize_mat(mat, precision);
}

void BCMatrix::unit()
{
  unit_m4(this->matrix);
  mat4_decompose(this->loc, this->q, this->size, this->matrix);
  quat_to_eul(this->rot, this->q);
}

/* We need double here because the OpenCollada API needs it.
 * precision = -1 indicates to not limit the precision. */
void BCMatrix::get_matrix(DMatrix &mat, const bool transposed, const int precision) const
{
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) {
      float val = (transposed) ? matrix[j][i] : matrix[i][j];
      if (precision >= 0)
        val = floor((val * pow(10, precision) + 0.5)) / pow(10, precision);
      mat[i][j] = val;
    }
}

void BCMatrix::get_matrix(Matrix &mat,
                          const bool transposed,
                          const int precision,
                          const bool inverted) const
{
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < 4; j++) {
      float val = (transposed) ? matrix[j][i] : matrix[i][j];
      if (precision >= 0)
        val = floor((val * pow(10, precision) + 0.5)) / pow(10, precision);
      mat[i][j] = val;
    }

  if (inverted) {
    invert_m4(mat);
  }
}

const bool BCMatrix::in_range(const BCMatrix &other, float distance) const
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (fabs(other.matrix[i][j] - matrix[i][j]) > distance) {
        return false;
      }
    }
  }
  return true;
}

float (&BCMatrix::location() const)[3]
{
  return loc;
}

float (&BCMatrix::rotation() const)[3]
{
  return rot;
}

float (&BCMatrix::scale() const)[3]
{
  return size;
}

float (&BCMatrix::quat() const)[4]
{
  return q;
}
