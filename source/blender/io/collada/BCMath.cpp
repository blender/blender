/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_utildefines.h"

#include "BCMath.h"
#include "BlenderContext.h"

void BCQuat::rotate_to(Matrix &mat_to)
{
  Quat qd;
  Matrix matd;
  Matrix mati;
  Matrix mat_from;

  quat_to_mat4(mat_from, q);

  /* Calculate the difference matrix matd between mat_from and mat_to */
  invert_m4_m4(mati, mat_from);
  mul_m4_m4m4(matd, mati, mat_to);

  mat4_to_quat(qd, matd);

  mul_qt_qtqt(q, qd, q); /* rotate to the final rotation to mat_to */
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
      global_forward_axis, global_up_axis, BC_DEFAULT_FORWARD, BC_DEFAULT_UP, mrot);
  copy_m4_m3(mat, mrot);
  set_transform(mat);
}

void BCMatrix::add_transform(const Matrix &mat, bool inverted)
{
  add_transform(this->matrix, mat, this->matrix, inverted);
}

void BCMatrix::add_transform(const BCMatrix &mat, bool inverted)
{
  add_transform(this->matrix, mat.matrix, this->matrix, inverted);
}

void BCMatrix::apply_transform(const BCMatrix &mat, bool inverted)
{
  apply_transform(this->matrix, mat.matrix, this->matrix, inverted);
}

void BCMatrix::add_transform(Matrix &to,
                             const Matrix &transform,
                             const Matrix &from,
                             bool inverted)
{
  if (inverted) {
    Matrix globinv;
    invert_m4_m4(globinv, transform);
    add_transform(to, globinv, from, /*inverted=*/false);
  }
  else {
    mul_m4_m4m4(to, transform, from);
  }
}

void BCMatrix::apply_transform(Matrix &to,
                               const Matrix &transform,
                               const Matrix &from,
                               bool inverse)
{
  Matrix globinv;
  invert_m4_m4(globinv, transform);
  if (inverse) {
    add_transform(to, globinv, from, /*inverted=*/false);
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

void BCMatrix::copy(Matrix &r, Matrix &a)
{
  /* destination comes first: */
  memcpy(r, a, sizeof(Matrix));
}

void BCMatrix::transpose(Matrix &mat)
{
  transpose_m4(mat);
}

void BCMatrix::sanitize(Matrix &mat, int precision)
{
  for (auto &row : mat) {
    for (float &cell : row) {
      double val = double(cell);
      val = double_round(val, precision);
      cell = float(val);
    }
  }
}

void BCMatrix::sanitize(DMatrix &mat, int precision)
{
  for (auto &row : mat) {
    for (double &cell : row) {
      cell = double_round(cell, precision);
    }
  }
}

void BCMatrix::unit()
{
  unit_m4(this->matrix);
  mat4_decompose(this->loc, this->q, this->size, this->matrix);
  quat_to_eul(this->rot, this->q);
}

void BCMatrix::get_matrix(DMatrix &mat, const bool transposed, const int precision) const
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      float val = (transposed) ? matrix[j][i] : matrix[i][j];
      if (precision >= 0) {
        val = floor(val * pow(10, precision) + 0.5) / pow(10, precision);
      }
      mat[i][j] = val;
    }
  }
}

void BCMatrix::get_matrix(Matrix &mat,
                          const bool transposed,
                          const int precision,
                          const bool inverted) const
{
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      float val = (transposed) ? matrix[j][i] : matrix[i][j];
      if (precision >= 0) {
        val = floor(val * pow(10, precision) + 0.5) / pow(10, precision);
      }
      mat[i][j] = val;
    }
  }

  if (inverted) {
    invert_m4(mat);
  }
}

bool BCMatrix::in_range(const BCMatrix &other, float distance) const
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
