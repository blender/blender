/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "BKE_object.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BlenderTypes.h"

class BCQuat {
 private:
  mutable Quat q;

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

  void rotate_to(Matrix &mat_to);
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

  /**
   * We need double here because the OpenCollada API needs it.
   * precision = -1 indicates to not limit the precision.
   */
  void get_matrix(DMatrix &matrix, bool transposed = false, int precision = -1) const;
  void get_matrix(Matrix &matrix,
                  bool transposed = false,
                  int precision = -1,
                  bool inverted = false) const;
  void set_transform(Object *ob);
  void set_transform(Matrix &mat);
  void add_transform(Matrix &to,
                     const Matrix &transform,
                     const Matrix &from,
                     bool inverted = false);
  void apply_transform(Matrix &to,
                       const Matrix &transform,
                       const Matrix &from,
                       bool inverse = false);
  void add_inverted_transform(Matrix &to, const Matrix &transform, const Matrix &from);
  void add_transform(const Matrix &matrix, bool inverted = false);
  void add_transform(const BCMatrix &matrix, bool inverted = false);
  void apply_transform(const BCMatrix &matrix, bool inverted = false);

  bool in_range(const BCMatrix &other, float distance) const;

  static void sanitize(Matrix &matrix, int precision);
  static void sanitize(DMatrix &matrix, int precision);
  static void transpose(Matrix &matrix);
};
