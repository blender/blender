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
 * \ingroup collada
 */

#ifndef __BCMATRIX_H__
#define __BCMATRIX_H__

#include "BlenderTypes.h"

extern "C" {
#include "BKE_object.h"
#include "BLI_math.h"
}

class BCQuat {
 private:
  mutable Quat q;

  void unit();
  void copy(Quat &r, Quat &a);

 public:
  BCQuat(const BCQuat &other)
  {
    copy_v4_v4(q, other.q);
  }

  BCQuat(Quat &other)
  {
    copy_v4_v4(q, other);
  }

  BCQuat()
  {
    unit_qt(q);
  }

  Quat &quat()
  {
    return q;
  }
};

class BCMatrix {

 private:
  mutable float matrix[4][4];
  mutable float size[3];
  mutable float rot[3];
  mutable float loc[3];
  mutable float q[4];

  void unit();
  void copy(Matrix &r, Matrix &a);

 public:
  float (&location() const)[3];
  float (&rotation() const)[3];
  float (&scale() const)[3];
  float (&quat() const)[4];

  BCMatrix(BC_global_forward_axis global_forward_axis, BC_global_up_axis global_up_axis);
  BCMatrix(const BCMatrix &mat);
  BCMatrix(Matrix &mat);
  BCMatrix(Object *ob);
  BCMatrix();

  void get_matrix(DMatrix &matrix, const bool transposed = false, const int precision = -1) const;
  void get_matrix(Matrix &matrix,
                  const bool transposed = false,
                  const int precision = -1,
                  const bool inverted = false) const;
  void set_transform(Object *ob);
  void set_transform(Matrix &mat);
  void add_transform(Matrix &to,
                     const Matrix &transform,
                     const Matrix &from,
                     const bool inverted = false);
  void apply_transform(Matrix &to,
                       const Matrix &transform,
                       const Matrix &from,
                       const bool inverted = false);
  void add_inverted_transform(Matrix &to, const Matrix &transform, const Matrix &from);
  void add_transform(const Matrix &matrix, const bool inverted = false);
  void add_transform(const BCMatrix &matrix, const bool inverted = false);
  void apply_transform(const BCMatrix &matrix, const bool inverted = false);

  const bool in_range(const BCMatrix &other, float distance) const;
  static void sanitize(Matrix &matrix, int precision);
  static void transpose(Matrix &matrix);
};

#endif
