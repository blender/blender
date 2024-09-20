/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 */

/*
 * Macros that implement an arithmetic operator or a math function
 * on vector types. Only for internal BLI vector math library use!
 *
 * Just doing per-element loop is enough for correct result, however
 * in debug / non-optimized builds (or even release builds with
 * assertions enabled), these result in very sub-optimal generated code.
 * So instead of the loop, also explicitly implement the operator for
 * common vector sizes (4, 3, 2).
 */

/* Binary operator `op` on vectors `a` and `b`. */
#define BLI_UNROLL_MATH_VEC_OP_VEC_VEC(op, a, b) \
  if constexpr (Size == 4) { \
    return VecBase<T, Size>(a.x op b.x, a.y op b.y, a.z op b.z, a.w op b.w); \
  } \
  else if constexpr (Size == 3) { \
    return VecBase<T, Size>(a.x op b.x, a.y op b.y, a.z op b.z); \
  } \
  else if constexpr (Size == 2) { \
    return VecBase<T, Size>(a.x op b.x, a.y op b.y); \
  } \
  else { \
    VecBase<T, Size> result; \
    for (int i = 0; i < Size; i++) { \
      result[i] = a[i] op b[i]; \
    } \
    return result; \
  }

/* Binary function `op` on vectors `a` and `b`. */
#define BLI_UNROLL_MATH_VEC_FUNC_VEC_VEC(op, a, b) \
  if constexpr (Size == 4) { \
    return VecBase<T, Size>(op(a.x, b.x), op(a.y, b.y), op(a.z, b.z), op(a.w, b.w)); \
  } \
  else if constexpr (Size == 3) { \
    return VecBase<T, Size>(op(a.x, b.x), op(a.y, b.y), op(a.z, b.z)); \
  } \
  else if constexpr (Size == 2) { \
    return VecBase<T, Size>(op(a.x, b.x), op(a.y, b.y)); \
  } \
  else { \
    VecBase<T, Size> result; \
    for (int i = 0; i < Size; i++) { \
      result[i] = op(a[i], b[i]); \
    } \
    return result; \
  }

/* Unary operator or function `op` on vector `a`. */
#define BLI_UNROLL_MATH_VEC_OP_VEC(op, a) \
  if constexpr (Size == 4) { \
    return VecBase<T, Size>(op(a.x), op(a.y), op(a.z), op(a.w)); \
  } \
  else if constexpr (Size == 3) { \
    return VecBase<T, Size>(op(a.x), op(a.y), op(a.z)); \
  } \
  else if constexpr (Size == 2) { \
    return VecBase<T, Size>(op(a.x), op(a.y)); \
  } \
  else { \
    VecBase<T, Size> result; \
    for (int i = 0; i < Size; i++) { \
      result[i] = op(a[i]); \
    } \
    return result; \
  }

/* Binary operator `op` on scalar `a` and vector `b`. */
#define BLI_UNROLL_MATH_VEC_OP_SCALAR_VEC(op, a, b) \
  if constexpr (Size == 4) { \
    return VecBase<T, Size>(a op b.x, a op b.y, a op b.z, a op b.w); \
  } \
  else if constexpr (Size == 3) { \
    return VecBase<T, Size>(a op b.x, a op b.y, a op b.z); \
  } \
  else if constexpr (Size == 2) { \
    return VecBase<T, Size>(a op b.x, a op b.y); \
  } \
  else { \
    VecBase<T, Size> result; \
    for (int i = 0; i < Size; i++) { \
      result[i] = a op b[i]; \
    } \
    return result; \
  }

/* Binary operator `op` on vector `a` and scalar `b`. */
#define BLI_UNROLL_MATH_VEC_OP_VEC_SCALAR(op, a, b) \
  if constexpr (Size == 4) { \
    return VecBase<T, Size>(a.x op b, a.y op b, a.z op b, a.w op b); \
  } \
  else if constexpr (Size == 3) { \
    return VecBase<T, Size>(a.x op b, a.y op b, a.z op b); \
  } \
  else if constexpr (Size == 2) { \
    return VecBase<T, Size>(a.x op b, a.y op b); \
  } \
  else { \
    VecBase<T, Size> result; \
    for (int i = 0; i < Size; i++) { \
      result[i] = a[i] op b; \
    } \
    return result; \
  }

/* Assignment-like operator `op` with vector argument `b`. */
#define BLI_UNROLL_MATH_VEC_OP_ASSIGN_VEC(op, b) \
  if constexpr (Size == 4) { \
    this->x op b.x; \
    this->y op b.y; \
    this->z op b.z; \
    this->w op b.w; \
  } \
  else if constexpr (Size == 3) { \
    this->x op b.x; \
    this->y op b.y; \
    this->z op b.z; \
  } \
  else if constexpr (Size == 2) { \
    this->x op b.x; \
    this->y op b.y; \
  } \
  else { \
    for (int i = 0; i < Size; i++) { \
      (*this)[i] op b[i]; \
    } \
  } \
  return *this

/* Assignment-like operator `op` with scalar argument `b`. */
#define BLI_UNROLL_MATH_VEC_OP_ASSIGN_SCALAR(op, b) \
  if constexpr (Size == 4) { \
    this->x op b; \
    this->y op b; \
    this->z op b; \
    this->w op b; \
  } \
  else if constexpr (Size == 3) { \
    this->x op b; \
    this->y op b; \
    this->z op b; \
  } \
  else if constexpr (Size == 2) { \
    this->x op b; \
    this->y op b; \
  } \
  else { \
    for (int i = 0; i < Size; i++) { \
      (*this)[i] op b; \
    } \
  } \
  return *this

/* Initialization from a pointer or indexed argument `a`. */
#define BLI_UNROLL_MATH_VEC_OP_INIT_INDEX(a) \
  if constexpr (Size == 4) { \
    this->x = a[0]; \
    this->y = a[1]; \
    this->z = a[2]; \
    this->w = a[3]; \
  } \
  else if constexpr (Size == 3) { \
    this->x = a[0]; \
    this->y = a[1]; \
    this->z = a[2]; \
  } \
  else if constexpr (Size == 2) { \
    this->x = a[0]; \
    this->y = a[1]; \
  } \
  else { \
    for (int i = 0; i < Size; i++) { \
      (*this)[i] = a[i]; \
    } \
  }

/* Initialization from another vector `a`. */
#define BLI_UNROLL_MATH_VEC_OP_INIT_VECTOR(a) \
  if constexpr (Size == 4) { \
    this->x = T(a.x); \
    this->y = T(a.y); \
    this->z = T(a.z); \
    this->w = T(a.w); \
  } \
  else if constexpr (Size == 3) { \
    this->x = T(a.x); \
    this->y = T(a.y); \
    this->z = T(a.z); \
  } \
  else if constexpr (Size == 2) { \
    this->x = T(a.x); \
    this->y = T(a.y); \
  } \
  else { \
    for (int i = 0; i < Size; i++) { \
      (*this)[i] = T(a[i]); \
    } \
  }
