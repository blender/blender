/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup bli
 */

#ifndef __MATH_VECTOR_INLINE_C__
#define __MATH_VECTOR_INLINE_C__

#include "BLI_math.h"

/********************************** Init *************************************/

MINLINE void zero_v2(float r[2])
{
  r[0] = 0.0f;
  r[1] = 0.0f;
}

MINLINE void zero_v3(float r[3])
{
  r[0] = 0.0f;
  r[1] = 0.0f;
  r[2] = 0.0f;
}

MINLINE void zero_v4(float r[4])
{
  r[0] = 0.0f;
  r[1] = 0.0f;
  r[2] = 0.0f;
  r[3] = 0.0f;
}

MINLINE void copy_v2_v2(float r[2], const float a[2])
{
  r[0] = a[0];
  r[1] = a[1];
}

MINLINE void copy_v3_v3(float r[3], const float a[3])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
}

MINLINE void copy_v3fl_v3s(float r[3], const short a[3])
{
  r[0] = (float)a[0];
  r[1] = (float)a[1];
  r[2] = (float)a[2];
}

MINLINE void copy_v4_v4(float r[4], const float a[4])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
  r[3] = a[3];
}

MINLINE void copy_v2_fl(float r[2], float f)
{
  r[0] = f;
  r[1] = f;
}

MINLINE void copy_v3_fl(float r[3], float f)
{
  r[0] = f;
  r[1] = f;
  r[2] = f;
}

MINLINE void copy_v4_fl(float r[4], float f)
{
  r[0] = f;
  r[1] = f;
  r[2] = f;
  r[3] = f;
}

/* unsigned char */

MINLINE void copy_v2_v2_uchar(unsigned char r[2], const unsigned char a[2])
{
  r[0] = a[0];
  r[1] = a[1];
}

MINLINE void copy_v3_v3_uchar(unsigned char r[3], const unsigned char a[3])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
}

MINLINE void copy_v4_v4_uchar(unsigned char r[4], const unsigned char a[4])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
  r[3] = a[3];
}

MINLINE void copy_v2_uchar(unsigned char r[2], const unsigned char a)
{
  r[0] = a;
  r[1] = a;
}

MINLINE void copy_v3_uchar(unsigned char r[3], const unsigned char a)
{
  r[0] = a;
  r[1] = a;
  r[2] = a;
}

MINLINE void copy_v4_uchar(unsigned char r[4], const unsigned char a)
{
  r[0] = a;
  r[1] = a;
  r[2] = a;
  r[3] = a;
}

/* char */

MINLINE void copy_v2_v2_char(char r[2], const char a[2])
{
  r[0] = a[0];
  r[1] = a[1];
}

MINLINE void copy_v3_v3_char(char r[3], const char a[3])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
}

MINLINE void copy_v4_v4_char(char r[4], const char a[4])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
  r[3] = a[3];
}

/* short */

MINLINE void copy_v2_v2_short(short r[2], const short a[2])
{
  r[0] = a[0];
  r[1] = a[1];
}

MINLINE void copy_v3_v3_short(short r[3], const short a[3])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
}

MINLINE void copy_v4_v4_short(short r[4], const short a[4])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
  r[3] = a[3];
}

/* int */
MINLINE void zero_v2_int(int r[2])
{
  r[0] = 0;
  r[1] = 0;
}

MINLINE void zero_v3_int(int r[3])
{
  r[0] = 0;
  r[1] = 0;
  r[2] = 0;
}

MINLINE void copy_v2_v2_int(int r[2], const int a[2])
{
  r[0] = a[0];
  r[1] = a[1];
}

MINLINE void copy_v3_v3_int(int r[3], const int a[3])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
}

MINLINE void copy_v4_v4_int(int r[4], const int a[4])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
  r[3] = a[3];
}

/* double */

MINLINE void zero_v3_db(double r[3])
{
  r[0] = 0.0;
  r[1] = 0.0;
  r[2] = 0.0;
}

MINLINE void copy_v2_v2_db(double r[2], const double a[2])
{
  r[0] = a[0];
  r[1] = a[1];
}

MINLINE void copy_v3_v3_db(double r[3], const double a[3])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
}

MINLINE void copy_v4_v4_db(double r[4], const double a[4])
{
  r[0] = a[0];
  r[1] = a[1];
  r[2] = a[2];
  r[3] = a[3];
}

MINLINE void round_v2i_v2fl(int r[2], const float a[2])
{
  r[0] = (int)roundf(a[0]);
  r[1] = (int)roundf(a[1]);
}

MINLINE void copy_v2fl_v2i(float r[2], const int a[2])
{
  r[0] = (float)a[0];
  r[1] = (float)a[1];
}

/* double -> float */

MINLINE void copy_v2fl_v2db(float r[2], const double a[2])
{
  r[0] = (float)a[0];
  r[1] = (float)a[1];
}

MINLINE void copy_v3fl_v3db(float r[3], const double a[3])
{
  r[0] = (float)a[0];
  r[1] = (float)a[1];
  r[2] = (float)a[2];
}

MINLINE void copy_v4fl_v4db(float r[4], const double a[4])
{
  r[0] = (float)a[0];
  r[1] = (float)a[1];
  r[2] = (float)a[2];
  r[3] = (float)a[3];
}

/* float -> double */

MINLINE void copy_v2db_v2fl(double r[2], const float a[2])
{
  r[0] = (double)a[0];
  r[1] = (double)a[1];
}

MINLINE void copy_v3db_v3fl(double r[3], const float a[3])
{
  r[0] = (double)a[0];
  r[1] = (double)a[1];
  r[2] = (double)a[2];
}

MINLINE void copy_v4db_v4fl(double r[4], const float a[4])
{
  r[0] = (double)a[0];
  r[1] = (double)a[1];
  r[2] = (double)a[2];
  r[3] = (double)a[3];
}

MINLINE void swap_v2_v2(float a[2], float b[2])
{
  SWAP(float, a[0], b[0]);
  SWAP(float, a[1], b[1]);
}

MINLINE void swap_v3_v3(float a[3], float b[3])
{
  SWAP(float, a[0], b[0]);
  SWAP(float, a[1], b[1]);
  SWAP(float, a[2], b[2]);
}

MINLINE void swap_v4_v4(float a[4], float b[4])
{
  SWAP(float, a[0], b[0]);
  SWAP(float, a[1], b[1]);
  SWAP(float, a[2], b[2]);
  SWAP(float, a[3], b[3]);
}

MINLINE void swap_v2_v2_db(double a[2], double b[2])
{
  SWAP(double, a[0], b[0]);
  SWAP(double, a[1], b[1]);
}

MINLINE void swap_v3_v3_db(double a[3], double b[3])
{
  SWAP(double, a[0], b[0]);
  SWAP(double, a[1], b[1]);
  SWAP(double, a[2], b[2]);
}

MINLINE void swap_v4_v4_db(double a[4], double b[4])
{
  SWAP(double, a[0], b[0]);
  SWAP(double, a[1], b[1]);
  SWAP(double, a[2], b[2]);
  SWAP(double, a[3], b[3]);
}

/* float args -> vec */

MINLINE void copy_v2_fl2(float v[2], float x, float y)
{
  v[0] = x;
  v[1] = y;
}

MINLINE void copy_v3_fl3(float v[3], float x, float y, float z)
{
  v[0] = x;
  v[1] = y;
  v[2] = z;
}

MINLINE void copy_v4_fl4(float v[4], float x, float y, float z, float w)
{
  v[0] = x;
  v[1] = y;
  v[2] = z;
  v[3] = w;
}

/********************************* Arithmetic ********************************/

MINLINE void add_v2_fl(float r[2], float f)
{
  r[0] += f;
  r[1] += f;
}

MINLINE void add_v3_fl(float r[3], float f)
{
  r[0] += f;
  r[1] += f;
  r[2] += f;
}

MINLINE void add_v4_fl(float r[4], float f)
{
  r[0] += f;
  r[1] += f;
  r[2] += f;
  r[3] += f;
}

MINLINE void add_v2_v2(float r[2], const float a[2])
{
  r[0] += a[0];
  r[1] += a[1];
}

MINLINE void add_v2_v2_db(double r[2], const double a[2])
{
  r[0] += a[0];
  r[1] += a[1];
}

MINLINE void add_v2_v2v2(float r[2], const float a[2], const float b[2])
{
  r[0] = a[0] + b[0];
  r[1] = a[1] + b[1];
}

MINLINE void add_v2_v2_int(int r[2], const int a[2])
{
  r[0] = r[0] + a[0];
  r[1] = r[1] + a[1];
}

MINLINE void add_v2_v2v2_int(int r[2], const int a[2], const int b[2])
{
  r[0] = a[0] + b[0];
  r[1] = a[1] + b[1];
}

MINLINE void add_v3_v3(float r[3], const float a[3])
{
  r[0] += a[0];
  r[1] += a[1];
  r[2] += a[2];
}

MINLINE void add_v3_v3_db(double r[3], const double a[3])
{
  r[0] += a[0];
  r[1] += a[1];
  r[2] += a[2];
}

MINLINE void add_v3_v3v3(float r[3], const float a[3], const float b[3])
{
  r[0] = a[0] + b[0];
  r[1] = a[1] + b[1];
  r[2] = a[2] + b[2];
}

MINLINE void add_v3fl_v3fl_v3i(float r[3], const float a[3], const int b[3])
{
  r[0] = a[0] + (float)b[0];
  r[1] = a[1] + (float)b[1];
  r[2] = a[2] + (float)b[2];
}

MINLINE void add_v4_v4(float r[4], const float a[4])
{
  r[0] += a[0];
  r[1] += a[1];
  r[2] += a[2];
  r[3] += a[3];
}

MINLINE void add_v4_v4v4(float r[4], const float a[4], const float b[4])
{
  r[0] = a[0] + b[0];
  r[1] = a[1] + b[1];
  r[2] = a[2] + b[2];
  r[3] = a[3] + b[3];
}

MINLINE void sub_v2_v2(float r[2], const float a[2])
{
  r[0] -= a[0];
  r[1] -= a[1];
}

MINLINE void sub_v2_v2v2(float r[2], const float a[2], const float b[2])
{
  r[0] = a[0] - b[0];
  r[1] = a[1] - b[1];
}

MINLINE void sub_v2_v2v2_db(double r[2], const double a[2], const double b[2])
{
  r[0] = a[0] - b[0];
  r[1] = a[1] - b[1];
}

MINLINE void sub_v2_v2v2_int(int r[2], const int a[2], const int b[2])
{
  r[0] = a[0] - b[0];
  r[1] = a[1] - b[1];
}

MINLINE void sub_v3_v3(float r[3], const float a[3])
{
  r[0] -= a[0];
  r[1] -= a[1];
  r[2] -= a[2];
}

MINLINE void sub_v3_v3v3(float r[3], const float a[3], const float b[3])
{
  r[0] = a[0] - b[0];
  r[1] = a[1] - b[1];
  r[2] = a[2] - b[2];
}

MINLINE void sub_v3_v3v3_int(int r[3], const int a[3], const int b[3])
{
  r[0] = a[0] - b[0];
  r[1] = a[1] - b[1];
  r[2] = a[2] - b[2];
}

MINLINE void sub_v3_v3v3_db(double r[3], const double a[3], const double b[3])
{
  r[0] = a[0] - b[0];
  r[1] = a[1] - b[1];
  r[2] = a[2] - b[2];
}

MINLINE void sub_v2db_v2fl_v2fl(double r[2], const float a[2], const float b[2])
{
  r[0] = (double)a[0] - (double)b[0];
  r[1] = (double)a[1] - (double)b[1];
}

MINLINE void sub_v3db_v3fl_v3fl(double r[3], const float a[3], const float b[3])
{
  r[0] = (double)a[0] - (double)b[0];
  r[1] = (double)a[1] - (double)b[1];
  r[2] = (double)a[2] - (double)b[2];
}

MINLINE void sub_v4_v4(float r[4], const float a[4])
{
  r[0] -= a[0];
  r[1] -= a[1];
  r[2] -= a[2];
  r[3] -= a[3];
}

MINLINE void sub_v4_v4v4(float r[4], const float a[4], const float b[4])
{
  r[0] = a[0] - b[0];
  r[1] = a[1] - b[1];
  r[2] = a[2] - b[2];
  r[3] = a[3] - b[3];
}

MINLINE void mul_v2_fl(float r[2], float f)
{
  r[0] *= f;
  r[1] *= f;
}

MINLINE void mul_v2_v2fl(float r[2], const float a[2], float f)
{
  r[0] = a[0] * f;
  r[1] = a[1] * f;
}

MINLINE void mul_v3_fl(float r[3], float f)
{
  r[0] *= f;
  r[1] *= f;
  r[2] *= f;
}

MINLINE void mul_v3db_db(double r[3], double f)
{
  r[0] *= f;
  r[1] *= f;
  r[2] *= f;
}

MINLINE void mul_v3_v3fl(float r[3], const float a[3], float f)
{
  r[0] = a[0] * f;
  r[1] = a[1] * f;
  r[2] = a[2] * f;
}

MINLINE void mul_v3_v3db_db(double r[3], const double a[3], double f)
{
  r[0] = a[0] * f;
  r[1] = a[1] * f;
  r[2] = a[2] * f;
}

MINLINE void mul_v2_v2(float r[2], const float a[2])
{
  r[0] *= a[0];
  r[1] *= a[1];
}

MINLINE void mul_v3_v3(float r[3], const float a[3])
{
  r[0] *= a[0];
  r[1] *= a[1];
  r[2] *= a[2];
}

MINLINE void mul_v4_fl(float r[4], float f)
{
  r[0] *= f;
  r[1] *= f;
  r[2] *= f;
  r[3] *= f;
}

MINLINE void mul_v4_v4(float r[4], const float a[4])
{
  r[0] *= a[0];
  r[1] *= a[1];
  r[2] *= a[2];
  r[3] *= a[3];
}

MINLINE void mul_v4_v4fl(float r[4], const float a[4], float f)
{
  r[0] = a[0] * f;
  r[1] = a[1] * f;
  r[2] = a[2] * f;
  r[3] = a[3] * f;
}

/**
 * Avoid doing:
 *
 * angle = atan2f(dvec[0], dvec[1]);
 * angle_to_mat2(mat, angle);
 *
 * instead use a vector as a matrix.
 */

MINLINE void mul_v2_v2_cw(float r[2], const float mat[2], const float vec[2])
{
  BLI_assert(r != vec);

  r[0] = mat[0] * vec[0] + (+mat[1]) * vec[1];
  r[1] = mat[1] * vec[0] + (-mat[0]) * vec[1];
}

MINLINE void mul_v2_v2_ccw(float r[2], const float mat[2], const float vec[2])
{
  float r0 = mat[0] * vec[0] + (-mat[1]) * vec[1];
  float r1 = mat[1] * vec[0] + (+mat[0]) * vec[1];
  r[0] = r0;
  r[1] = r1;
}

MINLINE float mul_project_m4_v3_zfac(const float mat[4][4], const float co[3])
{
  return (mat[0][3] * co[0]) + (mat[1][3] * co[1]) + (mat[2][3] * co[2]) + mat[3][3];
}

MINLINE float dot_m3_v3_row_x(const float M[3][3], const float a[3])
{
  return M[0][0] * a[0] + M[1][0] * a[1] + M[2][0] * a[2];
}
MINLINE float dot_m3_v3_row_y(const float M[3][3], const float a[3])
{
  return M[0][1] * a[0] + M[1][1] * a[1] + M[2][1] * a[2];
}
MINLINE float dot_m3_v3_row_z(const float M[3][3], const float a[3])
{
  return M[0][2] * a[0] + M[1][2] * a[1] + M[2][2] * a[2];
}

MINLINE float dot_m4_v3_row_x(const float M[4][4], const float a[3])
{
  return M[0][0] * a[0] + M[1][0] * a[1] + M[2][0] * a[2];
}
MINLINE float dot_m4_v3_row_y(const float M[4][4], const float a[3])
{
  return M[0][1] * a[0] + M[1][1] * a[1] + M[2][1] * a[2];
}
MINLINE float dot_m4_v3_row_z(const float M[4][4], const float a[3])
{
  return M[0][2] * a[0] + M[1][2] * a[1] + M[2][2] * a[2];
}

MINLINE void madd_v2_v2fl(float r[2], const float a[2], float f)
{
  r[0] += a[0] * f;
  r[1] += a[1] * f;
}

MINLINE void madd_v3_v3fl(float r[3], const float a[3], float f)
{
  r[0] += a[0] * f;
  r[1] += a[1] * f;
  r[2] += a[2] * f;
}

MINLINE void madd_v3_v3v3(float r[3], const float a[3], const float b[3])
{
  r[0] += a[0] * b[0];
  r[1] += a[1] * b[1];
  r[2] += a[2] * b[2];
}

MINLINE void madd_v2_v2v2fl(float r[2], const float a[2], const float b[2], float f)
{
  r[0] = a[0] + b[0] * f;
  r[1] = a[1] + b[1] * f;
}

MINLINE void madd_v3_v3v3fl(float r[3], const float a[3], const float b[3], float f)
{
  r[0] = a[0] + b[0] * f;
  r[1] = a[1] + b[1] * f;
  r[2] = a[2] + b[2] * f;
}

MINLINE void madd_v3_v3v3db_db(double r[3], const double a[3], const double b[3], double f)
{
  r[0] = a[0] + b[0] * f;
  r[1] = a[1] + b[1] * f;
  r[2] = a[2] + b[2] * f;
}

MINLINE void madd_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float c[3])
{
  r[0] = a[0] + b[0] * c[0];
  r[1] = a[1] + b[1] * c[1];
  r[2] = a[2] + b[2] * c[2];
}

MINLINE void madd_v3fl_v3fl_v3fl_v3i(float r[3],
                                     const float a[3],
                                     const float b[3],
                                     const int c[3])
{
  r[0] = a[0] + b[0] * (float)c[0];
  r[1] = a[1] + b[1] * (float)c[1];
  r[2] = a[2] + b[2] * (float)c[2];
}

MINLINE void madd_v4_v4fl(float r[4], const float a[4], float f)
{
  r[0] += a[0] * f;
  r[1] += a[1] * f;
  r[2] += a[2] * f;
  r[3] += a[3] * f;
}

MINLINE void madd_v4_v4v4(float r[4], const float a[4], const float b[4])
{
  r[0] += a[0] * b[0];
  r[1] += a[1] * b[1];
  r[2] += a[2] * b[2];
  r[3] += a[3] * b[3];
}

MINLINE void mul_v3_v3v3(float r[3], const float v1[3], const float v2[3])
{
  r[0] = v1[0] * v2[0];
  r[1] = v1[1] * v2[1];
  r[2] = v1[2] * v2[2];
}

MINLINE void mul_v2_v2v2(float r[2], const float a[2], const float b[2])
{
  r[0] = a[0] * b[0];
  r[1] = a[1] * b[1];
}

MINLINE void negate_v2(float r[2])
{
  r[0] = -r[0];
  r[1] = -r[1];
}

MINLINE void negate_v2_v2(float r[2], const float a[2])
{
  r[0] = -a[0];
  r[1] = -a[1];
}

MINLINE void negate_v3(float r[3])
{
  r[0] = -r[0];
  r[1] = -r[1];
  r[2] = -r[2];
}

MINLINE void negate_v3_v3(float r[3], const float a[3])
{
  r[0] = -a[0];
  r[1] = -a[1];
  r[2] = -a[2];
}

MINLINE void negate_v4(float r[4])
{
  r[0] = -r[0];
  r[1] = -r[1];
  r[2] = -r[2];
  r[3] = -r[3];
}

MINLINE void negate_v4_v4(float r[4], const float a[4])
{
  r[0] = -a[0];
  r[1] = -a[1];
  r[2] = -a[2];
  r[3] = -a[3];
}

MINLINE void negate_v3_short(short r[3])
{
  r[0] = (short)-r[0];
  r[1] = (short)-r[1];
  r[2] = (short)-r[2];
}

MINLINE void negate_v3_db(double r[3])
{
  r[0] = -r[0];
  r[1] = -r[1];
  r[2] = -r[2];
}

MINLINE void invert_v2(float r[2])
{
  BLI_assert(!ELEM(0.0f, r[0], r[1]));
  r[0] = 1.0f / r[0];
  r[1] = 1.0f / r[1];
}

MINLINE void invert_v3(float r[3])
{
  BLI_assert(!ELEM(0.0f, r[0], r[1], r[2]));
  r[0] = 1.0f / r[0];
  r[1] = 1.0f / r[1];
  r[2] = 1.0f / r[2];
}

MINLINE void invert_v3_safe(float r[3])
{
  if (r[0] != 0.0f) {
    r[0] = 1.0f / r[0];
  }
  if (r[1] != 0.0f) {
    r[1] = 1.0f / r[1];
  }
  if (r[2] != 0.0f) {
    r[2] = 1.0f / r[2];
  }
}

MINLINE void abs_v2(float r[2])
{
  r[0] = fabsf(r[0]);
  r[1] = fabsf(r[1]);
}

MINLINE void abs_v2_v2(float r[2], const float a[2])
{
  r[0] = fabsf(a[0]);
  r[1] = fabsf(a[1]);
}

MINLINE void abs_v3(float r[3])
{
  r[0] = fabsf(r[0]);
  r[1] = fabsf(r[1]);
  r[2] = fabsf(r[2]);
}

MINLINE void abs_v3_v3(float r[3], const float a[3])
{
  r[0] = fabsf(a[0]);
  r[1] = fabsf(a[1]);
  r[2] = fabsf(a[2]);
}

MINLINE void abs_v4(float r[4])
{
  r[0] = fabsf(r[0]);
  r[1] = fabsf(r[1]);
  r[2] = fabsf(r[2]);
  r[3] = fabsf(r[3]);
}

MINLINE void abs_v4_v4(float r[4], const float a[4])
{
  r[0] = fabsf(a[0]);
  r[1] = fabsf(a[1]);
  r[2] = fabsf(a[2]);
  r[3] = fabsf(a[3]);
}

MINLINE float dot_v2v2(const float a[2], const float b[2])
{
  return a[0] * b[0] + a[1] * b[1];
}

MINLINE double dot_v2v2_db(const double a[2], const double b[2])
{
  return a[0] * b[0] + a[1] * b[1];
}

MINLINE float dot_v3v3(const float a[3], const float b[3])
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

MINLINE float dot_v3v3v3(const float p[3], const float a[3], const float b[3])
{
  float vec1[3], vec2[3];

  sub_v3_v3v3(vec1, a, p);
  sub_v3_v3v3(vec2, b, p);
  if (is_zero_v3(vec1) || is_zero_v3(vec2)) {
    return 0.0f;
  }
  return dot_v3v3(vec1, vec2);
}

MINLINE float dot_v4v4(const float a[4], const float b[4])
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
}

MINLINE double dot_v3db_v3fl(const double a[3], const float b[3])
{
  return a[0] * (double)b[0] + a[1] * (double)b[1] + a[2] * (double)b[2];
}

MINLINE double dot_v3v3_db(const double a[3], const double b[3])
{
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

MINLINE float cross_v2v2(const float a[2], const float b[2])
{
  return a[0] * b[1] - a[1] * b[0];
}

MINLINE double cross_v2v2_db(const double a[2], const double b[2])
{
  return a[0] * b[1] - a[1] * b[0];
}

MINLINE void cross_v3_v3v3(float r[3], const float a[3], const float b[3])
{
  BLI_assert(r != a && r != b);
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
}

MINLINE void cross_v3_v3v3_hi_prec(float r[3], const float a[3], const float b[3])
{
  BLI_assert(r != a && r != b);
  r[0] = (float)((double)a[1] * (double)b[2] - (double)a[2] * (double)b[1]);
  r[1] = (float)((double)a[2] * (double)b[0] - (double)a[0] * (double)b[2]);
  r[2] = (float)((double)a[0] * (double)b[1] - (double)a[1] * (double)b[0]);
}

MINLINE void cross_v3_v3v3_db(double r[3], const double a[3], const double b[3])
{
  BLI_assert(r != a && r != b);
  r[0] = a[1] * b[2] - a[2] * b[1];
  r[1] = a[2] * b[0] - a[0] * b[2];
  r[2] = a[0] * b[1] - a[1] * b[0];
}

MINLINE void add_newell_cross_v3_v3v3(float n[3], const float v_prev[3], const float v_curr[3])
{
  n[0] += (v_prev[1] - v_curr[1]) * (v_prev[2] + v_curr[2]);
  n[1] += (v_prev[2] - v_curr[2]) * (v_prev[0] + v_curr[0]);
  n[2] += (v_prev[0] - v_curr[0]) * (v_prev[1] + v_curr[1]);
}

MINLINE void star_m3_v3(float rmat[3][3], const float a[3])
{
  rmat[0][0] = rmat[1][1] = rmat[2][2] = 0.0;
  rmat[0][1] = -a[2];
  rmat[0][2] = a[1];
  rmat[1][0] = a[2];
  rmat[1][2] = -a[0];
  rmat[2][0] = -a[1];
  rmat[2][1] = a[0];
}

/*********************************** Length **********************************/

MINLINE float len_squared_v2(const float v[2])
{
  return v[0] * v[0] + v[1] * v[1];
}

MINLINE float len_squared_v3(const float v[3])
{
  return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

MINLINE double len_squared_v3_db(const double v[3])
{
  return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

MINLINE float len_manhattan_v2(const float v[2])
{
  return fabsf(v[0]) + fabsf(v[1]);
}

MINLINE int len_manhattan_v2_int(const int v[2])
{
  return abs(v[0]) + abs(v[1]);
}

MINLINE float len_manhattan_v3(const float v[3])
{
  return fabsf(v[0]) + fabsf(v[1]) + fabsf(v[2]);
}

MINLINE float len_v2(const float v[2])
{
  return sqrtf(v[0] * v[0] + v[1] * v[1]);
}

MINLINE double len_v2_db(const double v[2])
{
  return sqrt(v[0] * v[0] + v[1] * v[1]);
}

MINLINE float len_v2v2(const float v1[2], const float v2[2])
{
  float x, y;

  x = v1[0] - v2[0];
  y = v1[1] - v2[1];
  return sqrtf(x * x + y * y);
}

MINLINE double len_v2v2_db(const double v1[2], const double v2[2])
{
  double x, y;

  x = v1[0] - v2[0];
  y = v1[1] - v2[1];
  return sqrt(x * x + y * y);
}

MINLINE float len_v2v2_int(const int v1[2], const int v2[2])
{
  float x, y;

  x = (float)(v1[0] - v2[0]);
  y = (float)(v1[1] - v2[1]);
  return sqrtf(x * x + y * y);
}

MINLINE float len_v3(const float a[3])
{
  return sqrtf(dot_v3v3(a, a));
}

MINLINE double len_v3_db(const double a[3])
{
  return sqrt(dot_v3v3_db(a, a));
}

MINLINE float len_squared_v2v2(const float a[2], const float b[2])
{
  float d[2];

  sub_v2_v2v2(d, b, a);
  return dot_v2v2(d, d);
}

MINLINE double len_squared_v2v2_db(const double a[2], const double b[2])
{
  double d[2];

  sub_v2_v2v2_db(d, b, a);
  return dot_v2v2_db(d, d);
}

MINLINE float len_squared_v3v3(const float a[3], const float b[3])
{
  float d[3];

  sub_v3_v3v3(d, b, a);
  return dot_v3v3(d, d);
}

MINLINE float len_squared_v4v4(const float a[4], const float b[4])
{
  float d[4];

  sub_v4_v4v4(d, b, a);
  return dot_v4v4(d, d);
}

MINLINE float len_manhattan_v2v2(const float a[2], const float b[2])
{
  float d[2];

  sub_v2_v2v2(d, b, a);
  return len_manhattan_v2(d);
}

MINLINE int len_manhattan_v2v2_int(const int a[2], const int b[2])
{
  int d[2];

  sub_v2_v2v2_int(d, b, a);
  return len_manhattan_v2_int(d);
}

MINLINE float len_manhattan_v3v3(const float a[3], const float b[3])
{
  float d[3];

  sub_v3_v3v3(d, b, a);
  return len_manhattan_v3(d);
}

MINLINE float len_v3v3(const float a[3], const float b[3])
{
  float d[3];

  sub_v3_v3v3(d, b, a);
  return len_v3(d);
}

MINLINE float len_v4(const float a[4])
{
  return sqrtf(dot_v4v4(a, a));
}

MINLINE float len_v4v4(const float a[4], const float b[4])
{
  float d[4];

  sub_v4_v4v4(d, b, a);
  return len_v4(d);
}

MINLINE float normalize_v2_v2_length(float r[2], const float a[2], const float unit_length)
{
  float d = dot_v2v2(a, a);

  if (d > 1.0e-35f) {
    d = sqrtf(d);
    mul_v2_v2fl(r, a, unit_length / d);
  }
  else {
    /* Either the vector is small or one of it's values contained `nan`. */
    zero_v2(r);
    d = 0.0f;
  }

  return d;
}
MINLINE float normalize_v2_v2(float r[2], const float a[2])
{
  return normalize_v2_v2_length(r, a, 1.0f);
}

MINLINE float normalize_v2(float n[2])
{
  return normalize_v2_v2(n, n);
}

MINLINE float normalize_v2_length(float n[2], const float unit_length)
{
  return normalize_v2_v2_length(n, n, unit_length);
}

MINLINE float normalize_v3_v3_length(float r[3], const float a[3], const float unit_length)
{
  float d = dot_v3v3(a, a);

  /* A larger value causes normalize errors in a scaled down models with camera extreme close. */
  if (d > 1.0e-35f) {
    d = sqrtf(d);
    mul_v3_v3fl(r, a, unit_length / d);
  }
  else {
    /* Either the vector is small or one of it's values contained `nan`. */
    zero_v3(r);
    d = 0.0f;
  }

  return d;
}
MINLINE float normalize_v3_v3(float r[3], const float a[3])
{
  return normalize_v3_v3_length(r, a, 1.0f);
}

MINLINE double normalize_v3_v3_length_db(double r[3], const double a[3], double const unit_length)
{
  double d = dot_v3v3_db(a, a);

  /* a larger value causes normalize errors in a
   * scaled down models with camera extreme close */
  if (d > 1.0e-70) {
    d = sqrt(d);
    mul_v3_v3db_db(r, a, unit_length / d);
  }
  else {
    zero_v3_db(r);
    d = 0.0;
  }

  return d;
}
MINLINE double normalize_v3_v3_db(double r[3], const double a[3])
{
  return normalize_v3_v3_length_db(r, a, 1.0);
}

MINLINE double normalize_v3_length_db(double n[3], const double unit_length)
{
  double d = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];

  /* a larger value causes normalize errors in a
   * scaled down models with camera extreme close */
  if (d > 1.0e-35) {
    double mul;

    d = sqrt(d);
    mul = unit_length / d;

    n[0] *= mul;
    n[1] *= mul;
    n[2] *= mul;
  }
  else {
    n[0] = n[1] = n[2] = 0;
    d = 0.0;
  }

  return d;
}
MINLINE double normalize_v3_db(double n[3])
{
  return normalize_v3_length_db(n, 1.0);
}

MINLINE float normalize_v3_length(float n[3], const float unit_length)
{
  return normalize_v3_v3_length(n, n, unit_length);
}

MINLINE float normalize_v3(float n[3])
{
  return normalize_v3_v3(n, n);
}

MINLINE void normal_float_to_short_v2(short out[2], const float in[2])
{
  out[0] = (short)(in[0] * 32767.0f);
  out[1] = (short)(in[1] * 32767.0f);
}

MINLINE void normal_short_to_float_v3(float out[3], const short in[3])
{
  out[0] = in[0] * (1.0f / 32767.0f);
  out[1] = in[1] * (1.0f / 32767.0f);
  out[2] = in[2] * (1.0f / 32767.0f);
}

MINLINE void normal_float_to_short_v3(short out[3], const float in[3])
{
  out[0] = (short)(in[0] * 32767.0f);
  out[1] = (short)(in[1] * 32767.0f);
  out[2] = (short)(in[2] * 32767.0f);
}

MINLINE void normal_float_to_short_v4(short out[4], const float in[4])
{
  out[0] = (short)(in[0] * 32767.0f);
  out[1] = (short)(in[1] * 32767.0f);
  out[2] = (short)(in[2] * 32767.0f);
  out[3] = (short)(in[3] * 32767.0f);
}

/********************************* Comparison ********************************/

MINLINE bool is_zero_v2(const float v[2])
{
  return (v[0] == 0.0f && v[1] == 0.0f);
}

MINLINE bool is_zero_v3(const float v[3])
{
  return (v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f);
}

MINLINE bool is_zero_v4(const float v[4])
{
  return (v[0] == 0.0f && v[1] == 0.0f && v[2] == 0.0f && v[3] == 0.0f);
}

MINLINE bool is_zero_v2_db(const double v[2])
{
  return (v[0] == 0.0 && v[1] == 0.0);
}

MINLINE bool is_zero_v3_db(const double v[3])
{
  return (v[0] == 0.0 && v[1] == 0.0 && v[2] == 0.0);
}

MINLINE bool is_zero_v4_db(const double v[4])
{
  return (v[0] == 0.0 && v[1] == 0.0 && v[2] == 0.0 && v[3] == 0.0);
}

MINLINE bool is_one_v3(const float v[3])
{
  return (v[0] == 1.0f && v[1] == 1.0f && v[2] == 1.0f);
}

/* -------------------------------------------------------------------- */
/** \name Vector Comparison
 *
 * \note use `value <= limit`, so a limit of zero doesn't fail on an exact match.
 * \{ */

MINLINE bool equals_v2v2(const float v1[2], const float v2[2])
{
  return ((v1[0] == v2[0]) && (v1[1] == v2[1]));
}

MINLINE bool equals_v3v3(const float v1[3], const float v2[3])
{
  return ((v1[0] == v2[0]) && (v1[1] == v2[1]) && (v1[2] == v2[2]));
}

MINLINE bool equals_v4v4(const float v1[4], const float v2[4])
{
  return ((v1[0] == v2[0]) && (v1[1] == v2[1]) && (v1[2] == v2[2]) && (v1[3] == v2[3]));
}

MINLINE bool equals_v2v2_int(const int v1[2], const int v2[2])
{
  return ((v1[0] == v2[0]) && (v1[1] == v2[1]));
}

MINLINE bool equals_v3v3_int(const int v1[3], const int v2[3])
{
  return ((v1[0] == v2[0]) && (v1[1] == v2[1]) && (v1[2] == v2[2]));
}

MINLINE bool equals_v4v4_int(const int v1[4], const int v2[4])
{
  return ((v1[0] == v2[0]) && (v1[1] == v2[1]) && (v1[2] == v2[2]) && (v1[3] == v2[3]));
}

MINLINE bool compare_v2v2(const float v1[2], const float v2[2], const float limit)
{
  return (compare_ff(v1[0], v2[0], limit) && compare_ff(v1[1], v2[1], limit));
}

MINLINE bool compare_v3v3(const float v1[3], const float v2[3], const float limit)
{
  return (compare_ff(v1[0], v2[0], limit) && compare_ff(v1[1], v2[1], limit) &&
          compare_ff(v1[2], v2[2], limit));
}

MINLINE bool compare_v4v4(const float v1[4], const float v2[4], const float limit)
{
  return (compare_ff(v1[0], v2[0], limit) && compare_ff(v1[1], v2[1], limit) &&
          compare_ff(v1[2], v2[2], limit) && compare_ff(v1[3], v2[3], limit));
}

MINLINE bool compare_v2v2_relative(const float v1[2],
                                   const float v2[2],
                                   const float limit,
                                   const int max_ulps)
{
  return (compare_ff_relative(v1[0], v2[0], limit, max_ulps) &&
          compare_ff_relative(v1[1], v2[1], limit, max_ulps));
}

MINLINE bool compare_v3v3_relative(const float v1[3],
                                   const float v2[3],
                                   const float limit,
                                   const int max_ulps)
{
  return (compare_ff_relative(v1[0], v2[0], limit, max_ulps) &&
          compare_ff_relative(v1[1], v2[1], limit, max_ulps) &&
          compare_ff_relative(v1[2], v2[2], limit, max_ulps));
}

MINLINE bool compare_v4v4_relative(const float v1[4],
                                   const float v2[4],
                                   const float limit,
                                   const int max_ulps)
{
  return (compare_ff_relative(v1[0], v2[0], limit, max_ulps) &&
          compare_ff_relative(v1[1], v2[1], limit, max_ulps) &&
          compare_ff_relative(v1[2], v2[2], limit, max_ulps) &&
          compare_ff_relative(v1[3], v2[3], limit, max_ulps));
}

MINLINE bool compare_len_v3v3(const float v1[3], const float v2[3], const float limit)
{
  float d[3];
  sub_v3_v3v3(d, v1, v2);
  return (dot_v3v3(d, d) <= (limit * limit));
}

MINLINE bool compare_size_v3v3(const float v1[3], const float v2[3], const float limit)
{
  for (int i = 0; i < 3; i++) {
    if (v2[i] == 0.0f) {
      /* Catch division by zero. */
      if (v1[i] != v2[i]) {
        return false;
      }
    }
    else {
      if (fabsf(v1[i] / v2[i] - 1.0f) > limit) {
        return false;
      }
    }
  }
  return true;
}

/* -------------------------------------------------------------------- */
/** \name Vector Clamping
 * \{ */

MINLINE void clamp_v2(float vec[2], const float min, const float max)
{
  CLAMP(vec[0], min, max);
  CLAMP(vec[1], min, max);
}

MINLINE void clamp_v3(float vec[3], const float min, const float max)
{
  CLAMP(vec[0], min, max);
  CLAMP(vec[1], min, max);
  CLAMP(vec[2], min, max);
}

MINLINE void clamp_v4(float vec[4], const float min, const float max)
{
  CLAMP(vec[0], min, max);
  CLAMP(vec[1], min, max);
  CLAMP(vec[2], min, max);
  CLAMP(vec[3], min, max);
}

MINLINE void clamp_v2_v2v2(float vec[2], const float min[2], const float max[2])
{
  CLAMP(vec[0], min[0], max[0]);
  CLAMP(vec[1], min[1], max[1]);
}

MINLINE void clamp_v3_v3v3(float vec[3], const float min[3], const float max[3])
{
  CLAMP(vec[0], min[0], max[0]);
  CLAMP(vec[1], min[1], max[1]);
  CLAMP(vec[2], min[2], max[2]);
}

MINLINE void clamp_v4_v4v4(float vec[4], const float min[4], const float max[4])
{
  CLAMP(vec[0], min[0], max[0]);
  CLAMP(vec[1], min[1], max[1]);
  CLAMP(vec[2], min[2], max[2]);
  CLAMP(vec[3], min[3], max[3]);
}

/** \} */

MINLINE float line_point_side_v2(const float l1[2], const float l2[2], const float pt[2])
{
  return (((l1[0] - pt[0]) * (l2[1] - pt[1])) - ((l2[0] - pt[0]) * (l1[1] - pt[1])));
}

/** \} */

#endif /* __MATH_VECTOR_INLINE_C__ */
