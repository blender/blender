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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/* Use a define instead of `#pragma once` because of `BLI_memory_utils.h` */
#ifndef __BLI_UTILDEFINES_H__
#define __BLI_UTILDEFINES_H__

/** \file
 * \ingroup bli
 */

/* avoid many includes for now */
#include "BLI_compiler_compat.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines_variadic.h"

/* We could remove in future. */
#include "BLI_assert.h"

/* include after _VA_NARGS macro */
#include "BLI_compiler_typecheck.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Min/Max Macros
 * \{ */

/* useful for finding bad use of min/max */
#if 0
/* gcc only */
#  define _TYPECHECK(a, b) ((void)(((typeof(a) *)0) == ((typeof(b) *)0)))
#  define MIN2(x, y) (_TYPECHECK(x, y), (((x) < (y) ? (x) : (y))))
#  define MAX2(x, y) (_TYPECHECK(x, y), (((x) > (y) ? (x) : (y))))
#endif

/* min/max */
#if defined(__GNUC__) || defined(__clang__)

#  define MIN2(a, b) \
    __extension__({ \
      typeof(a) a_ = (a); \
      typeof(b) b_ = (b); \
      ((a_) < (b_) ? (a_) : (b_)); \
    })

#  define MAX2(a, b) \
    __extension__({ \
      typeof(a) a_ = (a); \
      typeof(b) b_ = (b); \
      ((a_) > (b_) ? (a_) : (b_)); \
    })

#  define MIN3(a, b, c) \
    __extension__({ \
      typeof(a) a_ = (a); \
      typeof(b) b_ = (b); \
      typeof(c) c_ = (c); \
      ((a_ < b_) ? ((a_ < c_) ? a_ : c_) : ((b_ < c_) ? b_ : c_)); \
    })

#  define MAX3(a, b, c) \
    __extension__({ \
      typeof(a) a_ = (a); \
      typeof(b) b_ = (b); \
      typeof(c) c_ = (c); \
      ((a_ > b_) ? ((a_ > c_) ? a_ : c_) : ((b_ > c_) ? b_ : c_)); \
    })

#  define MIN4(a, b, c, d) \
    __extension__({ \
      typeof(a) a_ = (a); \
      typeof(b) b_ = (b); \
      typeof(c) c_ = (c); \
      typeof(d) d_ = (d); \
      ((a_ < b_) ? ((a_ < c_) ? ((a_ < d_) ? a_ : d_) : ((c_ < d_) ? c_ : d_)) : \
                   ((b_ < c_) ? ((b_ < d_) ? b_ : d_) : ((c_ < d_) ? c_ : d_))); \
    })

#  define MAX4(a, b, c, d) \
    __extension__({ \
      typeof(a) a_ = (a); \
      typeof(b) b_ = (b); \
      typeof(c) c_ = (c); \
      typeof(d) d_ = (d); \
      ((a_ > b_) ? ((a_ > c_) ? ((a_ > d_) ? a_ : d_) : ((c_ > d_) ? c_ : d_)) : \
                   ((b_ > c_) ? ((b_ > d_) ? b_ : d_) : ((c_ > d_) ? c_ : d_))); \
    })

#else
#  define MIN2(a, b) ((a) < (b) ? (a) : (b))
#  define MAX2(a, b) ((a) > (b) ? (a) : (b))

#  define MIN3(a, b, c) (MIN2(MIN2((a), (b)), (c)))
#  define MIN4(a, b, c, d) (MIN2(MIN2((a), (b)), MIN2((c), (d))))

#  define MAX3(a, b, c) (MAX2(MAX2((a), (b)), (c)))
#  define MAX4(a, b, c, d) (MAX2(MAX2((a), (b)), MAX2((c), (d))))
#endif

/* min/max that return a value of our choice */
#define MAX3_PAIR(cmp_a, cmp_b, cmp_c, ret_a, ret_b, ret_c) \
  ((cmp_a > cmp_b) ? ((cmp_a > cmp_c) ? ret_a : ret_c) : ((cmp_b > cmp_c) ? ret_b : ret_c))

#define MIN3_PAIR(cmp_a, cmp_b, cmp_c, ret_a, ret_b, ret_c) \
  ((cmp_a < cmp_b) ? ((cmp_a < cmp_c) ? ret_a : ret_c) : ((cmp_b < cmp_c) ? ret_b : ret_c))

#define INIT_MINMAX(min, max) \
  { \
    (min)[0] = (min)[1] = (min)[2] = 1.0e30f; \
    (max)[0] = (max)[1] = (max)[2] = -1.0e30f; \
  } \
  (void)0
#define INIT_MINMAX2(min, max) \
  { \
    (min)[0] = (min)[1] = 1.0e30f; \
    (max)[0] = (max)[1] = -1.0e30f; \
  } \
  (void)0
#define DO_MIN(vec, min) \
  { \
    if ((min)[0] > (vec)[0]) { \
      (min)[0] = (vec)[0]; \
    } \
    if ((min)[1] > (vec)[1]) { \
      (min)[1] = (vec)[1]; \
    } \
    if ((min)[2] > (vec)[2]) { \
      (min)[2] = (vec)[2]; \
    } \
  } \
  (void)0
#define DO_MAX(vec, max) \
  { \
    if ((max)[0] < (vec)[0]) { \
      (max)[0] = (vec)[0]; \
    } \
    if ((max)[1] < (vec)[1]) { \
      (max)[1] = (vec)[1]; \
    } \
    if ((max)[2] < (vec)[2]) { \
      (max)[2] = (vec)[2]; \
    } \
  } \
  (void)0
#define DO_MINMAX(vec, min, max) \
  { \
    if ((min)[0] > (vec)[0]) { \
      (min)[0] = (vec)[0]; \
    } \
    if ((min)[1] > (vec)[1]) { \
      (min)[1] = (vec)[1]; \
    } \
    if ((min)[2] > (vec)[2]) { \
      (min)[2] = (vec)[2]; \
    } \
    if ((max)[0] < (vec)[0]) { \
      (max)[0] = (vec)[0]; \
    } \
    if ((max)[1] < (vec)[1]) { \
      (max)[1] = (vec)[1]; \
    } \
    if ((max)[2] < (vec)[2]) { \
      (max)[2] = (vec)[2]; \
    } \
  } \
  (void)0
#define DO_MINMAX2(vec, min, max) \
  { \
    if ((min)[0] > (vec)[0]) { \
      (min)[0] = (vec)[0]; \
    } \
    if ((min)[1] > (vec)[1]) { \
      (min)[1] = (vec)[1]; \
    } \
    if ((max)[0] < (vec)[0]) { \
      (max)[0] = (vec)[0]; \
    } \
    if ((max)[1] < (vec)[1]) { \
      (max)[1] = (vec)[1]; \
    } \
  } \
  (void)0

/** \} */

/* -------------------------------------------------------------------- */
/** \name Swap/Shift Macros
 * \{ */

#define SWAP(type, a, b) \
  { \
    type sw_ap; \
    CHECK_TYPE(a, type); \
    CHECK_TYPE(b, type); \
    sw_ap = (a); \
    (a) = (b); \
    (b) = sw_ap; \
  } \
  (void)0

/* swap with a temp value */
#define SWAP_TVAL(tval, a, b) \
  { \
    CHECK_TYPE_PAIR(tval, a); \
    CHECK_TYPE_PAIR(tval, b); \
    (tval) = (a); \
    (a) = (b); \
    (b) = (tval); \
  } \
  (void)0

/* shift around elements */
#define SHIFT3(type, a, b, c) \
  { \
    type tmp; \
    CHECK_TYPE(a, type); \
    CHECK_TYPE(b, type); \
    CHECK_TYPE(c, type); \
    tmp = a; \
    a = c; \
    c = b; \
    b = tmp; \
  } \
  (void)0

#define SHIFT4(type, a, b, c, d) \
  { \
    type tmp; \
    CHECK_TYPE(a, type); \
    CHECK_TYPE(b, type); \
    CHECK_TYPE(c, type); \
    CHECK_TYPE(d, type); \
    tmp = a; \
    a = d; \
    d = c; \
    c = b; \
    b = tmp; \
  } \
  (void)0

/** \} */

/* -------------------------------------------------------------------- */
/** \name Equal to Any Element (ELEM) Macro
 * \{ */

/* Manual line breaks for readability. */
/* clang-format off */

/* ELEM#(v, ...): is the first arg equal any others? */
/* internal helpers. */
#define _VA_ELEM2(v, a) ((v) == (a))
#define _VA_ELEM3(v, a, b) \
  (_VA_ELEM2(v, a) || _VA_ELEM2(v, b))
#define _VA_ELEM4(v, a, b, c) \
  (_VA_ELEM3(v, a, b) || _VA_ELEM2(v, c))
#define _VA_ELEM5(v, a, b, c, d) \
  (_VA_ELEM4(v, a, b, c) || _VA_ELEM2(v, d))
#define _VA_ELEM6(v, a, b, c, d, e) \
  (_VA_ELEM5(v, a, b, c, d) || _VA_ELEM2(v, e))
#define _VA_ELEM7(v, a, b, c, d, e, f) \
  (_VA_ELEM6(v, a, b, c, d, e) || _VA_ELEM2(v, f))
#define _VA_ELEM8(v, a, b, c, d, e, f, g) \
  (_VA_ELEM7(v, a, b, c, d, e, f) || _VA_ELEM2(v, g))
#define _VA_ELEM9(v, a, b, c, d, e, f, g, h) \
  (_VA_ELEM8(v, a, b, c, d, e, f, g) || _VA_ELEM2(v, h))
#define _VA_ELEM10(v, a, b, c, d, e, f, g, h, i) \
  (_VA_ELEM9(v, a, b, c, d, e, f, g, h) || _VA_ELEM2(v, i))
#define _VA_ELEM11(v, a, b, c, d, e, f, g, h, i, j) \
  (_VA_ELEM10(v, a, b, c, d, e, f, g, h, i) || _VA_ELEM2(v, j))
#define _VA_ELEM12(v, a, b, c, d, e, f, g, h, i, j, k) \
  (_VA_ELEM11(v, a, b, c, d, e, f, g, h, i, j) || _VA_ELEM2(v, k))
#define _VA_ELEM13(v, a, b, c, d, e, f, g, h, i, j, k, l) \
  (_VA_ELEM12(v, a, b, c, d, e, f, g, h, i, j, k) || _VA_ELEM2(v, l))
#define _VA_ELEM14(v, a, b, c, d, e, f, g, h, i, j, k, l, m) \
  (_VA_ELEM13(v, a, b, c, d, e, f, g, h, i, j, k, l) || _VA_ELEM2(v, m))
#define _VA_ELEM15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
  (_VA_ELEM14(v, a, b, c, d, e, f, g, h, i, j, k, l, m) || _VA_ELEM2(v, n))
#define _VA_ELEM16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) \
  (_VA_ELEM15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n) || _VA_ELEM2(v, o))
#define _VA_ELEM17(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
  (_VA_ELEM16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) || _VA_ELEM2(v, p))
/* clang-format on */

/* reusable ELEM macro */
#define ELEM(...) VA_NARGS_CALL_OVERLOAD(_VA_ELEM, __VA_ARGS__)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Simple Math Macros
 * \{ */

/* Float equality checks. */

#define IS_EQ(a, b) \
  (CHECK_TYPE_INLINE(a, double), \
   CHECK_TYPE_INLINE(b, double), \
   ((fabs((double)((a) - (b))) >= (double)FLT_EPSILON) ? false : true))

#define IS_EQF(a, b) \
  (CHECK_TYPE_INLINE(a, float), \
   CHECK_TYPE_INLINE(b, float), \
   ((fabsf((float)((a) - (b))) >= (float)FLT_EPSILON) ? false : true))

#define IS_EQT(a, b, c) (((a) > (b)) ? ((((a) - (b)) <= (c))) : (((((b) - (a)) <= (c)))))
#define IN_RANGE(a, b, c) (((b) < (c)) ? (((b) < (a) && (a) < (c))) : (((c) < (a) && (a) < (b))))
#define IN_RANGE_INCL(a, b, c) \
  (((b) < (c)) ? (((b) <= (a) && (a) <= (c))) : (((c) <= (a) && (a) <= (b))))

/**
 * Expands to an integer constant expression evaluating to a close upper bound
 * on the number the number of decimal digits in a value expressible in the
 * integer type given by the argument (if it is a type name) or the integer
 * type of the argument (if it is an expression). The meaning of the resulting
 * expression is unspecified for other arguments.
 * i.e: `DECIMAL_DIGITS_BOUND(uchar)` is equal to 3.
 */
#define DECIMAL_DIGITS_BOUND(t) (241 * sizeof(t) / 100 + 1)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Clamp Macros
 * \{ */

#define CLAMPIS(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))

#define CLAMP(a, b, c) \
  { \
    if ((a) < (b)) { \
      (a) = (b); \
    } \
    else if ((a) > (c)) { \
      (a) = (c); \
    } \
  } \
  (void)0

#define CLAMP_MAX(a, c) \
  { \
    if ((a) > (c)) { \
      (a) = (c); \
    } \
  } \
  (void)0

#define CLAMP_MIN(a, b) \
  { \
    if ((a) < (b)) { \
      (a) = (b); \
    } \
  } \
  (void)0

#define CLAMP2(vec, b, c) \
  { \
    CLAMP((vec)[0], b, c); \
    CLAMP((vec)[1], b, c); \
  } \
  (void)0

#define CLAMP2_MIN(vec, b) \
  { \
    CLAMP_MIN((vec)[0], b); \
    CLAMP_MIN((vec)[1], b); \
  } \
  (void)0

#define CLAMP2_MAX(vec, b) \
  { \
    CLAMP_MAX((vec)[0], b); \
    CLAMP_MAX((vec)[1], b); \
  } \
  (void)0

#define CLAMP3(vec, b, c) \
  { \
    CLAMP((vec)[0], b, c); \
    CLAMP((vec)[1], b, c); \
    CLAMP((vec)[2], b, c); \
  } \
  (void)0

#define CLAMP3_MIN(vec, b) \
  { \
    CLAMP_MIN((vec)[0], b); \
    CLAMP_MIN((vec)[1], b); \
    CLAMP_MIN((vec)[2], b); \
  } \
  (void)0

#define CLAMP3_MAX(vec, b) \
  { \
    CLAMP_MAX((vec)[0], b); \
    CLAMP_MAX((vec)[1], b); \
    CLAMP_MAX((vec)[2], b); \
  } \
  (void)0

#define CLAMP4(vec, b, c) \
  { \
    CLAMP((vec)[0], b, c); \
    CLAMP((vec)[1], b, c); \
    CLAMP((vec)[2], b, c); \
    CLAMP((vec)[3], b, c); \
  } \
  (void)0

#define CLAMP4_MIN(vec, b) \
  { \
    CLAMP_MIN((vec)[0], b); \
    CLAMP_MIN((vec)[1], b); \
    CLAMP_MIN((vec)[2], b); \
    CLAMP_MIN((vec)[3], b); \
  } \
  (void)0

#define CLAMP4_MAX(vec, b) \
  { \
    CLAMP_MAX((vec)[0], b); \
    CLAMP_MAX((vec)[1], b); \
    CLAMP_MAX((vec)[2], b); \
    CLAMP_MAX((vec)[3], b); \
  } \
  (void)0

/** \} */

/* -------------------------------------------------------------------- */
/** \name Array Unpacking Macros
 * \{ */

/* unpack vector for args */
#define UNPACK2(a) ((a)[0]), ((a)[1])
#define UNPACK3(a) UNPACK2(a), ((a)[2])
#define UNPACK4(a) UNPACK3(a), ((a)[3])
/* pre may be '&', '*' or func, post may be '->member' */
#define UNPACK2_EX(pre, a, post) (pre((a)[0]) post), (pre((a)[1]) post)
#define UNPACK3_EX(pre, a, post) UNPACK2_EX(pre, a, post), (pre((a)[2]) post)
#define UNPACK4_EX(pre, a, post) UNPACK3_EX(pre, a, post), (pre((a)[3]) post)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Array Macros
 * \{ */

/* array helpers */
#define ARRAY_LAST_ITEM(arr_start, arr_dtype, arr_len) \
  (arr_dtype *)((char *)(arr_start) + (sizeof(*((arr_dtype *)NULL)) * (size_t)(arr_len - 1)))

#define ARRAY_HAS_ITEM(arr_item, arr_start, arr_len) \
  (CHECK_TYPE_PAIR_INLINE(arr_start, arr_item), \
   ((unsigned int)((arr_item) - (arr_start)) < (unsigned int)(arr_len)))

/**
 * \note use faster #ARRAY_DELETE_REORDER_LAST when we can re-order.
 */
#define ARRAY_DELETE(arr, index, delete_len, arr_len) \
  { \
    BLI_assert((&arr[index] >= arr) && ((index) + delete_len <= arr_len)); \
    memmove(&(arr)[index], \
            &(arr)[(index) + (delete_len)], \
            (((arr_len) - (index)) - (delete_len)) * sizeof(*(arr))); \
  } \
  ((void)0)

/**
 * Re-ordering array removal.
 *
 * When removing single items this compiles down to:
 * `if (index + 1 != arr_len) { arr[index] = arr[arr_len - 1]; }` (typical reordering removal),
 * with removing multiple items, overlap is detected to avoid memcpy errors.
 */
#define ARRAY_DELETE_REORDER_LAST(arr, index, delete_len, arr_len) \
  { \
    BLI_assert((&arr[index] >= arr) && ((index) + delete_len <= arr_len)); \
    if ((index) + (delete_len) != (arr_len)) { \
      if (((delete_len) == 1) || ((delete_len) <= ((arr_len) - ((index) + (delete_len))))) { \
        memcpy(&(arr)[index], &(arr)[(arr_len) - (delete_len)], (delete_len) * sizeof(*(arr))); \
      } \
      else { \
        memcpy(&(arr)[index], \
               &(arr)[(arr_len) - ((arr_len) - ((index) + (delete_len)))], \
               ((arr_len) - ((index) + (delete_len))) * sizeof(*(arr))); \
      } \
    } \
  } \
  ((void)0)

/* assuming a static array */
#if defined(__GNUC__) && !defined(__cplusplus) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#  define ARRAY_SIZE(arr) \
    ((sizeof(struct { int isnt_array : ((const void *)&(arr) == &(arr)[0]); }) * 0) + \
     (sizeof(arr) / sizeof(*(arr))))
#else
#  define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(*(arr)))
#endif

/* ARRAY_SET_ITEMS#(v, ...): set indices of array 'v' */
/* internal helpers */
#define _VA_ARRAY_SET_ITEMS2(v, a) ((v)[0] = (a))
#define _VA_ARRAY_SET_ITEMS3(v, a, b) \
  _VA_ARRAY_SET_ITEMS2(v, a); \
  ((v)[1] = (b))
#define _VA_ARRAY_SET_ITEMS4(v, a, b, c) \
  _VA_ARRAY_SET_ITEMS3(v, a, b); \
  ((v)[2] = (c))
#define _VA_ARRAY_SET_ITEMS5(v, a, b, c, d) \
  _VA_ARRAY_SET_ITEMS4(v, a, b, c); \
  ((v)[3] = (d))
#define _VA_ARRAY_SET_ITEMS6(v, a, b, c, d, e) \
  _VA_ARRAY_SET_ITEMS5(v, a, b, c, d); \
  ((v)[4] = (e))
#define _VA_ARRAY_SET_ITEMS7(v, a, b, c, d, e, f) \
  _VA_ARRAY_SET_ITEMS6(v, a, b, c, d, e); \
  ((v)[5] = (f))
#define _VA_ARRAY_SET_ITEMS8(v, a, b, c, d, e, f, g) \
  _VA_ARRAY_SET_ITEMS7(v, a, b, c, d, e, f); \
  ((v)[6] = (g))
#define _VA_ARRAY_SET_ITEMS9(v, a, b, c, d, e, f, g, h) \
  _VA_ARRAY_SET_ITEMS8(v, a, b, c, d, e, f, g); \
  ((v)[7] = (h))
#define _VA_ARRAY_SET_ITEMS10(v, a, b, c, d, e, f, g, h, i) \
  _VA_ARRAY_SET_ITEMS9(v, a, b, c, d, e, f, g, h); \
  ((v)[8] = (i))
#define _VA_ARRAY_SET_ITEMS11(v, a, b, c, d, e, f, g, h, i, j) \
  _VA_ARRAY_SET_ITEMS10(v, a, b, c, d, e, f, g, h, i); \
  ((v)[9] = (j))
#define _VA_ARRAY_SET_ITEMS12(v, a, b, c, d, e, f, g, h, i, j, k) \
  _VA_ARRAY_SET_ITEMS11(v, a, b, c, d, e, f, g, h, i, j); \
  ((v)[10] = (k))
#define _VA_ARRAY_SET_ITEMS13(v, a, b, c, d, e, f, g, h, i, j, k, l) \
  _VA_ARRAY_SET_ITEMS12(v, a, b, c, d, e, f, g, h, i, j, k); \
  ((v)[11] = (l))
#define _VA_ARRAY_SET_ITEMS14(v, a, b, c, d, e, f, g, h, i, j, k, l, m) \
  _VA_ARRAY_SET_ITEMS13(v, a, b, c, d, e, f, g, h, i, j, k, l); \
  ((v)[12] = (m))
#define _VA_ARRAY_SET_ITEMS15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
  _VA_ARRAY_SET_ITEMS14(v, a, b, c, d, e, f, g, h, i, j, k, l, m); \
  ((v)[13] = (n))
#define _VA_ARRAY_SET_ITEMS16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) \
  _VA_ARRAY_SET_ITEMS15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n); \
  ((v)[14] = (o))
#define _VA_ARRAY_SET_ITEMS17(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
  _VA_ARRAY_SET_ITEMS16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o); \
  ((v)[15] = (p))

/* reusable ARRAY_SET_ITEMS macro */
#define ARRAY_SET_ITEMS(...) \
  { \
    VA_NARGS_CALL_OVERLOAD(_VA_ARRAY_SET_ITEMS, __VA_ARGS__); \
  } \
  (void)0

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pointer Macros
 * \{ */

#if defined(__GNUC__) || defined(__clang__)
#  define POINTER_OFFSET(v, ofs) ((typeof(v))((char *)(v) + (ofs)))
#else
#  define POINTER_OFFSET(v, ofs) ((void *)((char *)(v) + (ofs)))
#endif

/* Warning-free macros for storing ints in pointers. Use these _only_
 * for storing an int in a pointer, not a pointer in an int (64bit)! */
#define POINTER_FROM_INT(i) ((void *)(intptr_t)(i))
#define POINTER_AS_INT(i) ((void)0, ((int)(intptr_t)(i)))

#define POINTER_FROM_UINT(i) ((void *)(uintptr_t)(i))
#define POINTER_AS_UINT(i) ((void)0, ((unsigned int)(uintptr_t)(i)))

/** \} */

/* -------------------------------------------------------------------- */
/** \name Struct After Macros
 *
 * Typically used to copy/clear polymorphic structs which have a generic
 * member at the start which needs to be left as-is.
 *
 * \{ */

/** Performs `offsetof(typeof(data), member) + sizeof((data)->member)` for non-gcc compilers. */
#define OFFSETOF_STRUCT_AFTER(_struct, _member) \
  ((((const char *)&((_struct)->_member)) - ((const char *)(_struct))) + \
   sizeof((_struct)->_member))

/**
 * memcpy helper, skipping the first part of a struct,
 * ensures 'struct_dst' isn't const and the offset can be computed at compile time.
 * This isn't inclusive, the value of \a member isn't copied.
 */
#define MEMCPY_STRUCT_AFTER(struct_dst, struct_src, member) \
  { \
    CHECK_TYPE_NONCONST(struct_dst); \
    ((void)(struct_dst == struct_src), \
     memcpy((char *)(struct_dst) + OFFSETOF_STRUCT_AFTER(struct_dst, member), \
            (const char *)(struct_src) + OFFSETOF_STRUCT_AFTER(struct_dst, member), \
            sizeof(*(struct_dst)) - OFFSETOF_STRUCT_AFTER(struct_dst, member))); \
  } \
  ((void)0)

#define MEMSET_STRUCT_AFTER(struct_var, value, member) \
  { \
    CHECK_TYPE_NONCONST(struct_var); \
    memset((char *)(struct_var) + OFFSETOF_STRUCT_AFTER(struct_var, member), \
           value, \
           sizeof(*(struct_var)) - OFFSETOF_STRUCT_AFTER(struct_var, member)); \
  } \
  ((void)0)

/* defined
 * in memory_utils.c for now. I do not know where we should put it actually... */
#ifndef __BLI_MEMORY_UTILS_H__
extern bool BLI_memory_is_zero(const void *arr, const size_t arr_size);
#endif

#define MEMCMP_STRUCT_AFTER_IS_ZERO(struct_var, member) \
  (BLI_memory_is_zero((const char *)(struct_var) + OFFSETOF_STRUCT_AFTER(struct_var, member), \
                      sizeof(*(struct_var)) - OFFSETOF_STRUCT_AFTER(struct_var, member)))

/** \} */

/* -------------------------------------------------------------------- */
/** \name String Macros
 * \{ */

/* Macro to convert a value to string in the pre-processor:
 * - `STRINGIFY_ARG`: gives the argument as a string
 * - `STRINGIFY_APPEND`: appends any argument 'b' onto the string argument 'a',
 *   used by `STRINGIFY` because some preprocessors warn about zero arguments
 * - `STRINGIFY`: gives the argument's value as a string. */
#define STRINGIFY_ARG(x) "" #x
#define STRINGIFY_APPEND(a, b) "" a #b
#define STRINGIFY(x) STRINGIFY_APPEND("", x)

/* generic strcmp macros */
#if defined(_MSC_VER)
#  define strcasecmp _stricmp
#  define strncasecmp _strnicmp
#endif

#define STREQ(a, b) (strcmp(a, b) == 0)
#define STRCASEEQ(a, b) (strcasecmp(a, b) == 0)
#define STREQLEN(a, b, n) (strncmp(a, b, n) == 0)
#define STRCASEEQLEN(a, b, n) (strncasecmp(a, b, n) == 0)

#define STRPREFIX(a, b) (strncmp((a), (b), strlen(b)) == 0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Unused Function/Argument Macros
 * \{ */

/* UNUSED macro, for function argument */
#if defined(__GNUC__) || defined(__clang__)
#  define UNUSED(x) UNUSED_##x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_##x
#endif

/**
 * WARNING: this doesn't warn when returning pointer types (because of the placement of `*`).
 * Use #UNUSED_FUNCTION_WITH_RETURN_TYPE instead in this case.
 */
#if defined(__GNUC__) || defined(__clang__)
#  define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_##x
#else
#  define UNUSED_FUNCTION(x) UNUSED_##x
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define UNUSED_FUNCTION_WITH_RETURN_TYPE(rtype, x) __attribute__((__unused__)) rtype UNUSED_##x
#else
#  define UNUSED_FUNCTION_WITH_RETURN_TYPE(rtype, x) rtype UNUSED_##x
#endif

/**
 * UNUSED_VARS#(a, ...): quiet unused warnings
 *
 * \code{.py}
 * for i in range(16):
 *     args = [(chr(ord('a') + (c % 26)) + (chr(ord('0') + (c // 26)))) for c in range(i + 1)]
 *     print("#define _VA_UNUSED_VARS_%d(%s) \\" % (i + 1, ", ".join(args)))
 *     print("\t((void)(%s)%s)" %
 *             (args[0], ((", _VA_UNUSED_VARS_" + str(i) + "(%s)") if i else "%s") %
 *              ", ".join((args[1:]))))
 * \endcode
 */

#define _VA_UNUSED_VARS_1(a0) ((void)(a0))
#define _VA_UNUSED_VARS_2(a0, b0) ((void)(a0), _VA_UNUSED_VARS_1(b0))
#define _VA_UNUSED_VARS_3(a0, b0, c0) ((void)(a0), _VA_UNUSED_VARS_2(b0, c0))
#define _VA_UNUSED_VARS_4(a0, b0, c0, d0) ((void)(a0), _VA_UNUSED_VARS_3(b0, c0, d0))
#define _VA_UNUSED_VARS_5(a0, b0, c0, d0, e0) ((void)(a0), _VA_UNUSED_VARS_4(b0, c0, d0, e0))
#define _VA_UNUSED_VARS_6(a0, b0, c0, d0, e0, f0) \
  ((void)(a0), _VA_UNUSED_VARS_5(b0, c0, d0, e0, f0))
#define _VA_UNUSED_VARS_7(a0, b0, c0, d0, e0, f0, g0) \
  ((void)(a0), _VA_UNUSED_VARS_6(b0, c0, d0, e0, f0, g0))
#define _VA_UNUSED_VARS_8(a0, b0, c0, d0, e0, f0, g0, h0) \
  ((void)(a0), _VA_UNUSED_VARS_7(b0, c0, d0, e0, f0, g0, h0))
#define _VA_UNUSED_VARS_9(a0, b0, c0, d0, e0, f0, g0, h0, i0) \
  ((void)(a0), _VA_UNUSED_VARS_8(b0, c0, d0, e0, f0, g0, h0, i0))
#define _VA_UNUSED_VARS_10(a0, b0, c0, d0, e0, f0, g0, h0, i0, j0) \
  ((void)(a0), _VA_UNUSED_VARS_9(b0, c0, d0, e0, f0, g0, h0, i0, j0))
#define _VA_UNUSED_VARS_11(a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0) \
  ((void)(a0), _VA_UNUSED_VARS_10(b0, c0, d0, e0, f0, g0, h0, i0, j0, k0))
#define _VA_UNUSED_VARS_12(a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0) \
  ((void)(a0), _VA_UNUSED_VARS_11(b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0))
#define _VA_UNUSED_VARS_13(a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0) \
  ((void)(a0), _VA_UNUSED_VARS_12(b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0))
#define _VA_UNUSED_VARS_14(a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0) \
  ((void)(a0), _VA_UNUSED_VARS_13(b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0))
#define _VA_UNUSED_VARS_15(a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0) \
  ((void)(a0), _VA_UNUSED_VARS_14(b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0))
#define _VA_UNUSED_VARS_16(a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0) \
  ((void)(a0), _VA_UNUSED_VARS_15(b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0))

/* reusable ELEM macro */
#define UNUSED_VARS(...) VA_NARGS_CALL_OVERLOAD(_VA_UNUSED_VARS_, __VA_ARGS__)

/* for debug-only variables */
#ifndef NDEBUG
#  define UNUSED_VARS_NDEBUG(...)
#else
#  define UNUSED_VARS_NDEBUG UNUSED_VARS
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Branch Prediction Macros
 * \{ */

/* hints for branch prediction, only use in code that runs a _lot_ where */
#ifdef __GNUC__
#  define LIKELY(x) __builtin_expect(!!(x), 1)
#  define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x) (x)
#  define UNLIKELY(x) (x)
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flag Macros
 * \{ */

/* Set flag from a single test */
#define SET_FLAG_FROM_TEST(value, test, flag) \
  { \
    if (test) { \
      (value) |= (flag); \
    } \
    else { \
      (value) &= ~(flag); \
    } \
  } \
  ((void)0)

/** \} */

/* -------------------------------------------------------------------- */
/** \name C++ Macros
 * \{ */

#ifdef __cplusplus

/* Useful to port C code using enums to C++ where enums are strongly typed.
 * To use after the enum declaration. */
/* If any enumerator `C` is set to say `A|B`, then `C` would be the max enum value. */
#  define ENUM_OPERATORS(_enum_type, _max_enum_value) \
    extern "C++" { \
    inline constexpr _enum_type operator|(_enum_type a, _enum_type b) \
    { \
      return static_cast<_enum_type>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b)); \
    } \
    inline constexpr _enum_type operator&(_enum_type a, _enum_type b) \
    { \
      return static_cast<_enum_type>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b)); \
    } \
    inline constexpr _enum_type operator~(_enum_type a) \
    { \
      return static_cast<_enum_type>(~static_cast<uint64_t>(a) & \
                                     (2 * static_cast<uint64_t>(_max_enum_value) - 1)); \
    } \
    inline _enum_type &operator|=(_enum_type &a, _enum_type b) \
    { \
      return a = static_cast<_enum_type>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b)); \
    } \
    inline _enum_type &operator&=(_enum_type &a, _enum_type b) \
    { \
      return a = static_cast<_enum_type>(static_cast<uint64_t>(a) & static_cast<uint64_t>(b)); \
    } \
    } /* extern "C++" */

#else
/* Output nothing. */
#  define ENUM_OPERATORS(_type, _max)
#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Misc Macros
 * \{ */

/** Useful for debugging. */
#define AT __FILE__ ":" STRINGIFY(__LINE__)

/** No-op for expressions we don't want to instantiate, but must remain valid. */
#define EXPR_NOP(expr) (void)(0 ? ((void)(expr), 1) : 0)

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __BLI_UTILDEFINES_H__ */
