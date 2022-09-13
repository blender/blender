/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

#pragma once

/** \file
 * \ingroup bli
 */

#include "BLI_compiler_attrs.h"
#include "BLI_math_inline.h"
#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Init
 * \{ */

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wredundant-decls"
#endif

MINLINE void zero_v2(float r[2]);
MINLINE void zero_v3(float r[3]);
MINLINE void zero_v4(float r[4]);

MINLINE void copy_v2_v2(float r[2], const float a[2]);
MINLINE void copy_v3_v3(float r[3], const float a[3]);
MINLINE void copy_v4_v4(float r[4], const float a[4]);

MINLINE void copy_v2_fl(float r[2], float f);
MINLINE void copy_v3_fl(float r[3], float f);
MINLINE void copy_v4_fl(float r[4], float f);

MINLINE void swap_v2_v2(float a[2], float b[2]);
MINLINE void swap_v3_v3(float a[3], float b[3]);
MINLINE void swap_v4_v4(float a[4], float b[4]);

MINLINE void swap_v2_v2_db(double a[2], double b[2]);
MINLINE void swap_v3_v3_db(double a[3], double b[3]);
MINLINE void swap_v4_v4_db(double a[4], double b[4]);

/* unsigned char */

MINLINE void copy_v2_v2_uchar(unsigned char r[2], const unsigned char a[2]);
MINLINE void copy_v3_v3_uchar(unsigned char r[3], const unsigned char a[3]);
MINLINE void copy_v4_v4_uchar(unsigned char r[4], const unsigned char a[4]);

MINLINE void copy_v2_uchar(unsigned char r[2], unsigned char a);
MINLINE void copy_v3_uchar(unsigned char r[3], unsigned char a);
MINLINE void copy_v4_uchar(unsigned char r[4], unsigned char a);

/* char */

MINLINE void copy_v2_v2_char(char r[2], const char a[2]);
MINLINE void copy_v3_v3_char(char r[3], const char a[3]);
MINLINE void copy_v4_v4_char(char r[4], const char a[4]);

/* short */

MINLINE void copy_v2_v2_short(short r[2], const short a[2]);
MINLINE void copy_v3_v3_short(short r[3], const short a[3]);
MINLINE void copy_v4_v4_short(short r[4], const short a[4]);

/* int */

MINLINE void zero_v3_int(int r[3]);
MINLINE void copy_v2_v2_int(int r[2], const int a[2]);
MINLINE void copy_v3_v3_int(int r[3], const int a[3]);
MINLINE void copy_v4_v4_int(int r[4], const int a[4]);

/* double */

MINLINE void zero_v3_db(double r[3]);
MINLINE void copy_v2_v2_db(double r[2], const double a[2]);
MINLINE void copy_v3_v3_db(double r[3], const double a[3]);
MINLINE void copy_v4_v4_db(double r[4], const double a[4]);

/* short -> float */

MINLINE void copy_v3fl_v3s(float r[3], const short a[3]);

/* int <-> float */

MINLINE void copy_v2fl_v2i(float r[2], const int a[2]);

/* int <-> float */

MINLINE void round_v2i_v2fl(int r[2], const float a[2]);

/* double -> float */

MINLINE void copy_v2fl_v2db(float r[2], const double a[2]);
MINLINE void copy_v3fl_v3db(float r[3], const double a[3]);
MINLINE void copy_v4fl_v4db(float r[4], const double a[4]);

/* float -> double */

MINLINE void copy_v2db_v2fl(double r[2], const float a[2]);
MINLINE void copy_v3db_v3fl(double r[3], const float a[3]);
MINLINE void copy_v4db_v4fl(double r[4], const float a[4]);

/* float args -> vec */

MINLINE void copy_v2_fl2(float v[2], float x, float y);
MINLINE void copy_v3_fl3(float v[3], float x, float y, float z);
MINLINE void copy_v4_fl4(float v[4], float x, float y, float z, float w);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arithmetic
 * \{ */

MINLINE void add_v2_fl(float r[2], float f);
MINLINE void add_v3_fl(float r[3], float f);
MINLINE void add_v4_fl(float r[4], float f);
MINLINE void add_v2_v2(float r[2], const float a[2]);
MINLINE void add_v2_v2_db(double r[2], const double a[2]);
MINLINE void add_v2_v2v2(float r[2], const float a[2], const float b[2]);
MINLINE void add_v2_v2_int(int r[2], const int a[2]);
MINLINE void add_v2_v2v2_int(int r[2], const int a[2], const int b[2]);
MINLINE void add_v3_v3(float r[3], const float a[3]);
MINLINE void add_v3_v3_db(double r[3], const double a[3]);
MINLINE void add_v3_v3v3(float r[3], const float a[3], const float b[3]);
MINLINE void add_v4_v4(float r[4], const float a[4]);
MINLINE void add_v4_v4v4(float r[4], const float a[4], const float b[4]);

MINLINE void add_v3fl_v3fl_v3i(float r[3], const float a[3], const int b[3]);

MINLINE void sub_v2_v2(float r[2], const float a[2]);
MINLINE void sub_v2_v2v2(float r[2], const float a[2], const float b[2]);
MINLINE void sub_v2_v2v2_db(double r[2], const double a[2], const double b[2]);
MINLINE void sub_v2_v2v2_int(int r[2], const int a[2], const int b[2]);
MINLINE void sub_v3_v3(float r[3], const float a[3]);
MINLINE void sub_v3_v3v3(float r[3], const float a[3], const float b[3]);
MINLINE void sub_v3_v3v3_int(int r[3], const int a[3], const int b[3]);
MINLINE void sub_v3_v3v3_db(double r[3], const double a[3], const double b[3]);
MINLINE void sub_v4_v4(float r[4], const float a[4]);
MINLINE void sub_v4_v4v4(float r[4], const float a[4], const float b[4]);

MINLINE void sub_v2db_v2fl_v2fl(double r[2], const float a[2], const float b[2]);
MINLINE void sub_v3db_v3fl_v3fl(double r[3], const float a[3], const float b[3]);

MINLINE void mul_v2_fl(float r[2], float f);
MINLINE void mul_v2_v2fl(float r[2], const float a[2], float f);
MINLINE void mul_v3_fl(float r[3], float f);
MINLINE void mul_v3db_db(double r[3], double f);
MINLINE void mul_v3_v3fl(float r[3], const float a[3], float f);
MINLINE void mul_v3_v3db_db(double r[3], const double a[3], double f);
MINLINE void mul_v2_v2(float r[2], const float a[2]);
MINLINE void mul_v2_v2v2(float r[2], const float a[2], const float b[2]);
MINLINE void mul_v3_v3(float r[3], const float a[3]);
MINLINE void mul_v3_v3v3(float r[3], const float v1[3], const float v2[3]);
MINLINE void mul_v4_fl(float r[4], float f);
MINLINE void mul_v4_v4(float r[4], const float a[4]);
MINLINE void mul_v4_v4fl(float r[4], const float a[4], float f);
MINLINE void mul_v2_v2_cw(float r[2], const float mat[2], const float vec[2]);
MINLINE void mul_v2_v2_ccw(float r[2], const float mat[2], const float vec[2]);
/**
 * Convenience function to get the projected depth of a position.
 * This avoids creating a temporary 4D vector and multiplying it - only for the 4th component.
 *
 * Matches logic for:
 *
 * \code{.c}
 * float co_4d[4] = {co[0], co[1], co[2], 1.0};
 * mul_m4_v4(mat, co_4d);
 * return co_4d[3];
 * \endcode
 */
MINLINE float mul_project_m4_v3_zfac(const float mat[4][4],
                                     const float co[3]) ATTR_WARN_UNUSED_RESULT;
/**
 * Has the effect of #mul_m3_v3(), on a single axis.
 */
MINLINE float dot_m3_v3_row_x(const float M[3][3], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m3_v3_row_y(const float M[3][3], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m3_v3_row_z(const float M[3][3], const float a[3]) ATTR_WARN_UNUSED_RESULT;
/**
 * Has the effect of #mul_mat3_m4_v3(), on a single axis.
 * (no adding translation)
 */
MINLINE float dot_m4_v3_row_x(const float M[4][4], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m4_v3_row_y(const float M[4][4], const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_m4_v3_row_z(const float M[4][4], const float a[3]) ATTR_WARN_UNUSED_RESULT;

MINLINE void madd_v2_v2fl(float r[2], const float a[2], float f);
MINLINE void madd_v3_v3fl(float r[3], const float a[3], float f);
MINLINE void madd_v3_v3v3(float r[3], const float a[3], const float b[3]);
MINLINE void madd_v2_v2v2fl(float r[2], const float a[2], const float b[2], float f);
MINLINE void madd_v3_v3v3fl(float r[3], const float a[3], const float b[3], float f);
MINLINE void madd_v3_v3v3db_db(double r[3], const double a[3], const double b[3], double f);
MINLINE void madd_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float c[3]);
MINLINE void madd_v4_v4fl(float r[4], const float a[4], float f);
MINLINE void madd_v4_v4v4(float r[4], const float a[4], const float b[4]);

MINLINE void madd_v3fl_v3fl_v3fl_v3i(float r[3],
                                     const float a[3],
                                     const float b[3],
                                     const int c[3]);

MINLINE void negate_v2(float r[2]);
MINLINE void negate_v2_v2(float r[2], const float a[2]);
MINLINE void negate_v3(float r[3]);
MINLINE void negate_v3_v3(float r[3], const float a[3]);
MINLINE void negate_v4(float r[4]);
MINLINE void negate_v4_v4(float r[4], const float a[4]);

/* could add more... */

MINLINE void negate_v3_short(short r[3]);
MINLINE void negate_v3_db(double r[3]);

MINLINE void invert_v2(float r[2]);
MINLINE void invert_v3(float r[3]);
/**
 * Invert the vector, but leaves zero values as zero.
 */
MINLINE void invert_v3_safe(float r[3]);

MINLINE void abs_v2(float r[2]);
MINLINE void abs_v2_v2(float r[2], const float a[2]);
MINLINE void abs_v3(float r[3]);
MINLINE void abs_v3_v3(float r[3], const float a[3]);
MINLINE void abs_v4(float r[4]);
MINLINE void abs_v4_v4(float r[4], const float a[4]);

MINLINE float dot_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE double dot_v2v2_db(const double a[2], const double b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_v3v3v3(const float p[3],
                         const float a[3],
                         const float b[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float dot_v4v4(const float a[4], const float b[4]) ATTR_WARN_UNUSED_RESULT;

MINLINE double dot_v3db_v3fl(const double a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;

MINLINE double dot_v3v3_db(const double a[3], const double b[3]) ATTR_WARN_UNUSED_RESULT;

MINLINE float cross_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE double cross_v2v2_db(const double a[2], const double b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE void cross_v3_v3v3(float r[3], const float a[3], const float b[3]);
/**
 * Cross product suffers from severe precision loss when vectors are
 * nearly parallel or opposite; doing the computation in double helps a lot.
 */
MINLINE void cross_v3_v3v3_hi_prec(float r[3], const float a[3], const float b[3]);
MINLINE void cross_v3_v3v3_db(double r[3], const double a[3], const double b[3]);

/**
 * Excuse this fairly specific function, its used for polygon normals all over the place
 * (could use a better name).
 */
MINLINE void add_newell_cross_v3_v3v3(float n[3], const float v_prev[3], const float v_curr[3]);

MINLINE void star_m3_v3(float rmat[3][3], const float a[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Length
 * \{ */

MINLINE float len_squared_v2(const float v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_squared_v3(const float v[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_manhattan_v2(const float v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE int len_manhattan_v2_int(const int v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_manhattan_v3(const float v[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v2(const float v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE double len_v2_db(const double v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v2v2(const float v1[2], const float v2[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE double len_v2v2_db(const double v1[2], const double v2[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v2v2_int(const int v1[2], const int v2[2]);
MINLINE float len_squared_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE double len_squared_v2v2_db(const double a[2], const double b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_squared_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_squared_v4v4(const float a[4], const float b[4]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_manhattan_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE int len_manhattan_v2v2_int(const int a[2], const int b[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_manhattan_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v3(const float a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float len_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;

MINLINE double len_v3_db(const double a[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE double len_squared_v3_db(const double v[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE float normalize_v2_length(float n[2], float unit_length);
/**
 * \note any vectors containing `nan` will be zeroed out.
 */
MINLINE float normalize_v2_v2_length(float r[2], const float a[2], float unit_length);
MINLINE float normalize_v3_length(float n[3], float unit_length);
/**
 * \note any vectors containing `nan` will be zeroed out.
 */
MINLINE float normalize_v3_v3_length(float r[3], const float a[3], float unit_length);
MINLINE double normalize_v3_length_db(double n[3], double unit_length);
MINLINE double normalize_v3_v3_length_db(double r[3], const double a[3], double unit_length);

MINLINE float normalize_v2(float n[2]);
MINLINE float normalize_v2_v2(float r[2], const float a[2]);
MINLINE float normalize_v3(float n[3]);
MINLINE float normalize_v3_v3(float r[3], const float a[3]);
MINLINE double normalize_v3_v3_db(double r[3], const double a[3]);
MINLINE double normalize_v3_db(double n[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interpolation
 * \{ */

void interp_v2_v2v2(float r[2], const float a[2], const float b[2], float t);
void interp_v2_v2v2_db(double target[2], const double a[2], const double b[2], double t);
/**
 * Weight 3 2D vectors,
 * 'w' must be unit length but is not a vector, just 3 weights.
 */
void interp_v2_v2v2v2(
    float r[2], const float a[2], const float b[2], const float c[2], const float t[3]);
void interp_v3_v3v3(float r[3], const float a[3], const float b[3], float t);
void interp_v3_v3v3_db(double target[3], const double a[3], const double b[3], double t);
/**
 * Weight 3 vectors,
 * 'w' must be unit length but is not a vector, just 3 weights.
 */
void interp_v3_v3v3v3(
    float p[3], const float v1[3], const float v2[3], const float v3[3], const float w[3]);
/**
 * Weight 3 vectors,
 * 'w' must be unit length but is not a vector, just 4 weights.
 */
void interp_v3_v3v3v3v3(float p[3],
                        const float v1[3],
                        const float v2[3],
                        const float v3[3],
                        const float v4[3],
                        const float w[4]);
void interp_v4_v4v4(float r[4], const float a[4], const float b[4], float t);
void interp_v4_v4v4v4(
    float p[4], const float v1[4], const float v2[4], const float v3[4], const float w[3]);
void interp_v4_v4v4v4v4(float p[4],
                        const float v1[4],
                        const float v2[4],
                        const float v3[4],
                        const float v4[4],
                        const float w[4]);
void interp_v3_v3v3v3_uv(
    float p[3], const float v1[3], const float v2[3], const float v3[3], const float uv[2]);

/**
 * slerp, treat vectors as spherical coordinates
 * \see #interp_qt_qtqt
 *
 * \return success
 */
bool interp_v3_v3v3_slerp(float target[3], const float a[3], const float b[3], float t)
    ATTR_WARN_UNUSED_RESULT;
bool interp_v2_v2v2_slerp(float target[2], const float a[2], const float b[2], float t)
    ATTR_WARN_UNUSED_RESULT;

/**
 * Same as #interp_v3_v3v3_slerp but uses fallback values for opposite vectors.
 */
void interp_v3_v3v3_slerp_safe(float target[3], const float a[3], const float b[3], float t);
void interp_v2_v2v2_slerp_safe(float target[2], const float a[2], const float b[2], float t);

void interp_v2_v2v2v2v2_cubic(float p[2],
                              const float v1[2],
                              const float v2[2],
                              const float v3[2],
                              const float v4[2],
                              float u);

void interp_v3_v3v3_char(char target[3], const char a[3], const char b[3], float t);
void interp_v3_v3v3_uchar(unsigned char target[3],
                          const unsigned char a[3],
                          const unsigned char b[3],
                          float t);
void interp_v4_v4v4_char(char target[4], const char a[4], const char b[4], float t);
void interp_v4_v4v4_uchar(unsigned char target[4],
                          const unsigned char a[4],
                          const unsigned char b[4],
                          float t);

void mid_v3_v3v3(float r[3], const float a[3], const float b[3]);
void mid_v2_v2v2(float r[2], const float a[2], const float b[2]);
void mid_v3_v3v3v3(float v[3], const float v1[3], const float v2[3], const float v3[3]);
void mid_v2_v2v2v2(float v[2], const float v1[2], const float v2[2], const float v3[2]);
void mid_v3_v3v3v3v3(
    float v[3], const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
void mid_v3_v3_array(float r[3], const float (*vec_arr)[3], unsigned int vec_arr_num);

/**
 * Specialized function for calculating normals.
 * Fast-path for:
 *
 * \code{.c}
 * add_v3_v3v3(r, a, b);
 * normalize_v3(r)
 * mul_v3_fl(r, angle_normalized_v3v3(a, b) / M_PI_2);
 * \endcode
 *
 * We can use the length of (a + b) to calculate the angle.
 */
void mid_v3_v3v3_angle_weighted(float r[3], const float a[3], const float b[3]);
/**
 * Same as mid_v3_v3v3_angle_weighted
 * but \a r is assumed to be accumulated normals, divided by their total.
 */
void mid_v3_angle_weighted(float r[3]);

void flip_v4_v4v4(float v[4], const float v1[4], const float v2[4]);
void flip_v3_v3v3(float v[3], const float v1[3], const float v2[3]);
void flip_v2_v2v2(float v[2], const float v1[2], const float v2[2]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Comparison
 * \{ */

MINLINE bool is_zero_v2(const float v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE bool is_zero_v3(const float v[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE bool is_zero_v4(const float v[4]) ATTR_WARN_UNUSED_RESULT;

MINLINE bool is_zero_v2_db(const double v[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE bool is_zero_v3_db(const double v[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE bool is_zero_v4_db(const double v[4]) ATTR_WARN_UNUSED_RESULT;

bool is_finite_v2(const float v[2]) ATTR_WARN_UNUSED_RESULT;
bool is_finite_v3(const float v[3]) ATTR_WARN_UNUSED_RESULT;
bool is_finite_v4(const float v[4]) ATTR_WARN_UNUSED_RESULT;

MINLINE bool is_one_v3(const float v[3]) ATTR_WARN_UNUSED_RESULT;

MINLINE bool equals_v2v2(const float v1[2], const float v2[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE bool equals_v3v3(const float v1[3], const float v2[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE bool equals_v4v4(const float v1[4], const float v2[4]) ATTR_WARN_UNUSED_RESULT;

MINLINE bool equals_v2v2_int(const int v1[2], const int v2[2]) ATTR_WARN_UNUSED_RESULT;
MINLINE bool equals_v3v3_int(const int v1[3], const int v2[3]) ATTR_WARN_UNUSED_RESULT;
MINLINE bool equals_v4v4_int(const int v1[4], const int v2[4]) ATTR_WARN_UNUSED_RESULT;

MINLINE bool compare_v2v2(const float v1[2],
                          const float v2[2],
                          float limit) ATTR_WARN_UNUSED_RESULT;
MINLINE bool compare_v3v3(const float v1[3],
                          const float v2[3],
                          float limit) ATTR_WARN_UNUSED_RESULT;
MINLINE bool compare_v4v4(const float v1[4],
                          const float v2[4],
                          float limit) ATTR_WARN_UNUSED_RESULT;

MINLINE bool compare_v2v2_relative(const float v1[2], const float v2[2], float limit, int max_ulps)
    ATTR_WARN_UNUSED_RESULT;
MINLINE bool compare_v3v3_relative(const float v1[3], const float v2[3], float limit, int max_ulps)
    ATTR_WARN_UNUSED_RESULT;
MINLINE bool compare_v4v4_relative(const float v1[4], const float v2[4], float limit, int max_ulps)
    ATTR_WARN_UNUSED_RESULT;

MINLINE bool compare_len_v3v3(const float v1[3],
                              const float v2[3],
                              float limit) ATTR_WARN_UNUSED_RESULT;

MINLINE bool compare_size_v3v3(const float v1[3],
                               const float v2[3],
                               float limit) ATTR_WARN_UNUSED_RESULT;

/**
 * <pre>
 *        + l1
 *        |
 * neg <- | -> pos
 *        |
 *        + l2
 * </pre>
 *
 * \return Positive value when 'pt' is left-of-line
 * (looking from 'l1' -> 'l2').
 */
MINLINE float line_point_side_v2(const float l1[2],
                                 const float l2[2],
                                 const float pt[2]) ATTR_WARN_UNUSED_RESULT;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Angles
 * \{ */

/* - angle with 2 arguments is angle between vector.
 * - angle with 3 arguments is angle between 3 points at the middle point.
 * - angle_normalized_* is faster equivalent if vectors are normalized.
 */

/**
 * Return the shortest angle in radians between the 2 vectors.
 */
float angle_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
float angle_signed_v2v2(const float v1[2], const float v2[2]) ATTR_WARN_UNUSED_RESULT;
float angle_v2v2v2(const float a[2], const float b[2], const float c[2]) ATTR_WARN_UNUSED_RESULT;
float angle_normalized_v2v2(const float a[2], const float b[2]) ATTR_WARN_UNUSED_RESULT;
/**
 * Return the shortest angle in radians between the 2 vectors.
 */
float angle_v3v3(const float a[3], const float b[3]) ATTR_WARN_UNUSED_RESULT;
/**
 * Return the angle in radians between vecs 1-2 and 2-3 in radians
 * If v1 is a shoulder, v2 is the elbow and v3 is the hand,
 * this would return the angle at the elbow.
 *
 * note that when v1/v2/v3 represent 3 points along a straight line
 * that the angle returned will be pi (180deg), rather than 0.0.
 */
float angle_v3v3v3(const float a[3], const float b[3], const float c[3]) ATTR_WARN_UNUSED_RESULT;
/**
 * Quicker than full angle computation.
 */
float cos_v3v3v3(const float p1[3], const float p2[3], const float p3[3]) ATTR_WARN_UNUSED_RESULT;
/**
 * Quicker than full angle computation.
 */
float cos_v2v2v2(const float p1[2], const float p2[2], const float p3[2]) ATTR_WARN_UNUSED_RESULT;
/**
 * Angle between 2 vectors, about an axis (axis can be considered a plane).
 */
float angle_on_axis_v3v3_v3(const float v1[3],
                            const float v2[3],
                            const float axis[3]) ATTR_WARN_UNUSED_RESULT;
float angle_signed_on_axis_v3v3_v3(const float v1[3],
                                   const float v2[3],
                                   const float axis[3]) ATTR_WARN_UNUSED_RESULT;
float angle_normalized_v3v3(const float v1[3], const float v2[3]) ATTR_WARN_UNUSED_RESULT;
/**
 * Angle between 2 vectors defined by 3 coords, about an axis (axis can be considered a plane).
 */
float angle_on_axis_v3v3v3_v3(const float v1[3],
                              const float v2[3],
                              const float v3[3],
                              const float axis[3]) ATTR_WARN_UNUSED_RESULT;
float angle_signed_on_axis_v3v3v3_v3(const float v1[3],
                                     const float v2[3],
                                     const float v3[3],
                                     const float axis[3]) ATTR_WARN_UNUSED_RESULT;
void angle_tri_v3(float angles[3], const float v1[3], const float v2[3], const float v3[3]);
void angle_quad_v3(
    float angles[4], const float v1[3], const float v2[3], const float v3[3], const float v4[3]);
void angle_poly_v3(float *angles, const float *verts[3], int len);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geometry
 * \{ */

/**
 * Project \a p onto \a v_proj
 */
void project_v2_v2v2(float out[2], const float p[2], const float v_proj[2]);
/**
 * Project \a p onto \a v_proj
 */
void project_v3_v3v3(float out[3], const float p[3], const float v_proj[3]);
void project_v3_v3v3_db(double out[3], const double p[3], const double v_proj[3]);
/**
 * Project \a p onto a unit length \a v_proj
 */
void project_v2_v2v2_normalized(float out[2], const float p[2], const float v_proj[2]);
/**
 * Project \a p onto a unit length \a v_proj
 */
void project_v3_v3v3_normalized(float out[3], const float p[3], const float v_proj[3]);
/**
 * In this case plane is a 3D vector only (no 4th component).
 *
 * Projecting will make \a out a copy of \a p orthogonal to \a v_plane.
 *
 * \note If \a p is exactly perpendicular to \a v_plane, \a out will just be a copy of \a p.
 *
 * \note This function is a convenience to call:
 * \code{.c}
 * project_v3_v3v3(out, p, v_plane);
 * sub_v3_v3v3(out, p, out);
 * \endcode
 */
void project_plane_v3_v3v3(float out[3], const float p[3], const float v_plane[3]);
void project_plane_v2_v2v2(float out[2], const float p[2], const float v_plane[2]);
void project_plane_normalized_v3_v3v3(float out[3], const float p[3], const float v_plane[3]);
void project_plane_normalized_v2_v2v2(float out[2], const float p[2], const float v_plane[2]);
/**
 * Project a vector on a plane defined by normal and a plane point p.
 */
void project_v3_plane(float out[3], const float plane_no[3], const float plane_co[3]);
/**
 * Returns a reflection vector from a vector and a normal vector
 * reflect = vec - ((2 * dot(vec, mirror)) * mirror).
 *
 * <pre>
 * v
 * +  ^
 *  \ |
 *   \|
 *    + normal: axis of reflection
 *   /
 *  /
 * +
 * out: result (negate for a 'bounce').
 * </pre>
 */
void reflect_v3_v3v3(float out[3], const float v[3], const float normal[3]);
void reflect_v3_v3v3_db(double out[3], const double v[3], const double normal[3]);
/**
 * Takes a vector and computes 2 orthogonal directions.
 *
 * \note if \a n is n unit length, computed values will be too.
 */
void ortho_basis_v3v3_v3(float r_n1[3], float r_n2[3], const float n[3]);
/**
 * Calculates \a p - a perpendicular vector to \a v
 *
 * \note return vector won't maintain same length.
 */
void ortho_v3_v3(float out[3], const float v[3]);
/**
 * no brainer compared to v3, just have for consistency.
 */
void ortho_v2_v2(float out[2], const float v[2]);
/**
 * Returns a vector bisecting the angle at b formed by a, b and c.
 */
void bisect_v3_v3v3v3(float r[3], const float a[3], const float b[3], const float c[3]);
/**
 * Rotate a point \a p by \a angle around origin (0, 0)
 */
void rotate_v2_v2fl(float r[2], const float p[2], float angle);
void rotate_v3_v3v3fl(float r[3], const float p[3], const float axis[3], float angle);
/**
 * Rotate a point \a p by \a angle around an arbitrary unit length \a axis.
 * http://local.wasp.uwa.edu.au/~pbourke/geometry/
 */
void rotate_normalized_v3_v3v3fl(float out[3], const float p[3], const float axis[3], float angle);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Other
 * \{ */

void print_v2(const char *str, const float v[2]);
void print_v3(const char *str, const float v[3]);
void print_v4(const char *str, const float v[4]);
void print_vn(const char *str, const float v[], int n);

#define print_v2_id(v) print_v2(STRINGIFY(v), v)
#define print_v3_id(v) print_v3(STRINGIFY(v), v)
#define print_v4_id(v) print_v4(STRINGIFY(v), v)
#define print_vn_id(v, n) print_vn(STRINGIFY(v), v, n)

MINLINE void normal_float_to_short_v2(short out[2], const float in[2]);
MINLINE void normal_short_to_float_v3(float out[3], const short in[3]);
MINLINE void normal_float_to_short_v3(short out[3], const float in[3]);
MINLINE void normal_float_to_short_v4(short out[4], const float in[4]);

void minmax_v4v4_v4(float min[4], float max[4], const float vec[4]);
void minmax_v3v3_v3(float min[3], float max[3], const float vec[3]);
void minmax_v2v2_v2(float min[2], float max[2], const float vec[2]);

void minmax_v3v3_v3_array(float r_min[3],
                          float r_max[3],
                          const float (*vec_arr)[3],
                          int var_arr_num);

/** ensure \a v1 is \a dist from \a v2 */
void dist_ensure_v3_v3fl(float v1[3], const float v2[3], float dist);
void dist_ensure_v2_v2fl(float v1[2], const float v2[2], float dist);

void axis_sort_v3(const float axis_values[3], int r_axis_order[3]);

MINLINE void clamp_v2(float vec[2], float min, float max);
MINLINE void clamp_v3(float vec[3], float min, float max);
MINLINE void clamp_v4(float vec[4], float min, float max);
MINLINE void clamp_v2_v2v2(float vec[2], const float min[2], const float max[2]);
MINLINE void clamp_v3_v3v3(float vec[3], const float min[3], const float max[3]);
MINLINE void clamp_v4_v4v4(float vec[4], const float min[4], const float max[4]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Array Functions
 * \{ */

/**
 * Follow fixed length vector function conventions.
 */

double dot_vn_vn(const float *array_src_a,
                 const float *array_src_b,
                 int size) ATTR_WARN_UNUSED_RESULT;
double len_squared_vn(const float *array, int size) ATTR_WARN_UNUSED_RESULT;
float normalize_vn_vn(float *array_tar, const float *array_src, int size);
float normalize_vn(float *array_tar, int size);
void range_vn_i(int *array_tar, int size, int start);
void range_vn_u(unsigned int *array_tar, int size, unsigned int start);
void range_vn_fl(float *array_tar, int size, float start, float step);
void negate_vn(float *array_tar, int size);
void negate_vn_vn(float *array_tar, const float *array_src, int size);
void mul_vn_vn(float *array_tar, const float *array_src, int size);
void mul_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, int size);
void mul_vn_fl(float *array_tar, int size, float f);
void mul_vn_vn_fl(float *array_tar, const float *array_src, int size, float f);
void add_vn_vn(float *array_tar, const float *array_src, int size);
void add_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, int size);
void madd_vn_vn(float *array_tar, const float *array_src, float f, int size);
void madd_vn_vnvn(
    float *array_tar, const float *array_src_a, const float *array_src_b, float f, int size);
void sub_vn_vn(float *array_tar, const float *array_src, int size);
void sub_vn_vnvn(float *array_tar, const float *array_src_a, const float *array_src_b, int size);
void msub_vn_vn(float *array_tar, const float *array_src, float f, int size);
void msub_vn_vnvn(
    float *array_tar, const float *array_src_a, const float *array_src_b, float f, int size);
void interp_vn_vn(float *array_tar, const float *array_src, float t, int size);
void copy_vn_i(int *array_tar, int size, int val);
void copy_vn_short(short *array_tar, int size, short val);
void copy_vn_ushort(unsigned short *array_tar, int size, unsigned short val);
void copy_vn_uchar(unsigned char *array_tar, int size, unsigned char val);
void copy_vn_fl(float *array_tar, int size, float val);

void add_vn_vn_d(double *array_tar, const double *array_src, int size);
void add_vn_vnvn_d(double *array_tar,
                   const double *array_src_a,
                   const double *array_src_b,
                   int size);
void mul_vn_db(double *array_tar, int size, double f);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Inline Definitions
 * \{ */

#if BLI_MATH_DO_INLINE
#  include "intern/math_vector_inline.c"
#endif

#ifdef BLI_MATH_GCC_WARN_PRAGMA
#  pragma GCC diagnostic pop
#endif

/** \} */

#ifdef __cplusplus
}
#endif
