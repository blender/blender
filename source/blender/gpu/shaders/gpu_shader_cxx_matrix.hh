/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * C++ stubs for shading language.
 *
 * IMPORTANT: Please ask the module team if you need some feature that are not listed in this file.
 */

#pragma once

#include "gpu_shader_cxx_builtin.hh"
#include "gpu_shader_cxx_vector.hh"

/* Some compilers complain about lack of return values. Keep it short. */
#define RET \
  { \
    return {}; \
  }

/* -------------------------------------------------------------------- */
/** \name Matrix Types
 * \{ */

template<int C, int R> struct MatBase {};

template<int C, int R> struct MatOp {
  using MatT = MatBase<C, R>;
  using ColT = VecBase<float, R>;
  using RowT = VecBase<float, C>;

  const ColT &operator[](int) const
  {
    return *reinterpret_cast<const ColT *>(this);
  }
  ColT &operator[](int)
  {
    return *reinterpret_cast<ColT *>(this);
  }
  const ColT &operator[](uint) const
  {
    return *reinterpret_cast<const ColT *>(this);
  }
  ColT &operator[](uint)
  {
    return *reinterpret_cast<ColT *>(this);
  }

  MatT operator+() RET;
  MatT operator-() RET;

  MatT operator*(MatT) const RET;

  friend RowT operator*(ColT, MatT) RET;
  friend ColT operator*(MatT, RowT) RET;
};

template<int R> struct MatBase<2, R> : MatOp<2, R> {
  using T = float;
  using ColT = VecBase<float, R>;
  ColT x, y;

  MatBase() = default;
  explicit MatBase(T) {}
  explicit MatBase(T, T, T, T) {}
  explicit MatBase(ColT, ColT) {}
  template<int OtherC, int OtherR> explicit MatBase(const MatBase<OtherC, OtherR> &) {}
};

template<int R> struct MatBase<3, R> : MatOp<3, R> {
  using T = float;
  using ColT = VecBase<float, R>;
  ColT x, y, z;

  MatBase() = default;
  explicit MatBase(T) {}
  explicit MatBase(T, T, T, T, T, T, T, T, T) {}
  explicit MatBase(ColT, ColT, ColT) {}
  template<int OtherC, int OtherR> explicit MatBase(const MatBase<OtherC, OtherR> &) {}
};

template<int R> struct MatBase<4, R> : MatOp<4, R> {
  using T = float;
  using ColT = VecBase<float, R>;
  ColT x, y, z, w;

  MatBase() = default;
  explicit MatBase(T) {}
  explicit MatBase(T, T, T, T, T, T, T, T, T, T, T, T, T, T, T, T) {}
  explicit MatBase(ColT, ColT, ColT, ColT) {}
  template<int OtherC, int OtherR> explicit MatBase(const MatBase<OtherC, OtherR> &) {}
};

using float2x2 = MatBase<2, 2>;
using float2x3 = MatBase<2, 3>;
using float2x4 = MatBase<2, 4>;
using float3x2 = MatBase<3, 2>;
using float3x3 = MatBase<3, 3>;
using float3x4 = MatBase<3, 4>;
using float4x2 = MatBase<4, 2>;
using float4x3 = MatBase<4, 3>;
using float4x4 = MatBase<4, 4>;

/* Matrix reshaping functions. */
#define RESHAPE(mat_to, mat_from, ...) \
  mat_to to_##mat_to(mat_from m) \
  { \
    return mat_to(__VA_ARGS__); \
  }

/* clang-format off */
RESHAPE(float2x2, float3x3, m[0].xy, m[1].xy)
RESHAPE(float2x2, float4x4, m[0].xy, m[1].xy)
RESHAPE(float3x3, float4x4, m[0].xyz, m[1].xyz, m[2].xyz)
RESHAPE(float3x3, float2x2, m[0].x, m[0].y, 0, m[1].x, m[1].y, 0, 0, 0, 1)
RESHAPE(float4x4, float2x2, m[0].x, m[0].y, 0, 0, m[1].x, m[1].y, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)
RESHAPE(float4x4, float3x3, m[0].x, m[0].y, m[0].z, 0, m[1].x, m[1].y, m[1].z, 0, m[2].x, m[2].y, m[2].z, 0, 0, 0, 0, 1)
/* clang-format on */
/* TODO(fclem): Remove. Use Transform instead. */
RESHAPE(float3x3, float3x4, m[0].xyz, m[1].xyz, m[2].xyz)
#undef RESHAPE

/* Matrix compare operators. */
#define EQ_OP(type, ...) \
  inline bool operator==(type a, type b) \
  { \
    return __VA_ARGS__; \
  }
EQ_OP(float2x2, all(equal(a[0], b[0])) && all(equal(a[1], b[1])))
EQ_OP(float2x3, all(equal(a[0], b[0])) && all(equal(a[1], b[1])))
EQ_OP(float2x4, all(equal(a[0], b[0])) && all(equal(a[1], b[1])))
EQ_OP(float3x2, all(equal(a[0], b[0])) && all(equal(a[1], b[1])) && all(equal(a[2], b[2])))
EQ_OP(float3x3, all(equal(a[0], b[0])) && all(equal(a[1], b[1])) && all(equal(a[2], b[2])))
EQ_OP(float3x4, all(equal(a[0], b[0])) && all(equal(a[1], b[1])) && all(equal(a[2], b[2])))
EQ_OP(float4x2,
      all(equal(a[0], b[0])) && all(equal(a[1], b[1])) && all(equal(a[2], b[2])) &&
          all(equal(a[3], b[3])))
EQ_OP(float4x3,
      all(equal(a[0], b[0])) && all(equal(a[1], b[1])) && all(equal(a[2], b[2])) &&
          all(equal(a[3], b[3])))
EQ_OP(float4x4,
      all(equal(a[0], b[0])) && all(equal(a[1], b[1])) && all(equal(a[2], b[2])) &&
          all(equal(a[3], b[3])))
#undef EQ_OP

/* Matrices functions. */
template<int C, int R> float determinant(MatBase<C, R>) RET;
template<int C, int R> MatBase<C, R> inverse(MatBase<C, R>) RET;
template<int C, int R> MatBase<R, C> transpose(MatBase<C, R>) RET;

/** \} */

#undef RET
