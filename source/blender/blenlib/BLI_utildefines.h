/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_UTILDEFINES_H__
#define __BLI_UTILDEFINES_H__

/** \file BLI_utildefines.h
 *  \ingroup bli
 */

#ifdef __cplusplus
extern "C" {
#endif

/* avoid many includes for now */
#include "BLI_sys_types.h"
#include "BLI_compiler_compat.h"

#ifndef NDEBUG /* for BLI_assert */
#include <stdio.h>
#endif


/* varargs macros (keep first so others can use) */
/* --- internal helpers --- */
#define _VA_NARGS_GLUE(x, y) x y
#define _VA_NARGS_RETURN_COUNT(\
	_1_, _2_, _3_, _4_, _5_, _6_, _7_, _8_, _9_, _10_, _11_, _12_, _13_, _14_, _15_, _16_, \
	_17_, _18_, _19_, _20_, _21_, _22_, _23_, _24_, _25_, _26_, _27_, _28_, _29_, _30_, _31_, _32_, \
	_33_, _34_, _35_, _36_, _37_, _38_, _39_, _40_, _41_, _42_, _43_, _44_, _45_, _46_, _47_, _48_, \
	_49_, _50_, _51_, _52_, _53_, _54_, _55_, _56_, _57_, _58_, _59_, _60_, _61_, _62_, _63_, _64_, \
	count, ...) count
#define _VA_NARGS_EXPAND(args) _VA_NARGS_RETURN_COUNT args
/* 64 args max */
#define _VA_NARGS_COUNT(...) _VA_NARGS_EXPAND((__VA_ARGS__, \
	64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, \
	48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, \
	32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, \
	16, 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2, 1, 0))
#define _VA_NARGS_OVERLOAD_MACRO2(name, count) name##count
#define _VA_NARGS_OVERLOAD_MACRO1(name, count) _VA_NARGS_OVERLOAD_MACRO2(name, count)
#define _VA_NARGS_OVERLOAD_MACRO(name,  count) _VA_NARGS_OVERLOAD_MACRO1(name, count)
/* --- expose for re-use --- */
#define VA_NARGS_CALL_OVERLOAD(name, ...) \
	_VA_NARGS_GLUE(_VA_NARGS_OVERLOAD_MACRO(name, _VA_NARGS_COUNT(__VA_ARGS__)), (__VA_ARGS__))

/* useful for finding bad use of min/max */
#if 0
/* gcc only */
#  define _TYPECHECK(a, b)  ((void)(((typeof(a) *)0) == ((typeof(b) *)0)))
#  define MIN2(x, y)          (_TYPECHECK(x, y), (((x) < (y) ? (x) : (y))))
#  define MAX2(x, y)          (_TYPECHECK(x, y), (((x) > (y) ? (x) : (y))))
#endif

/* include after _VA_NARGS macro */
#include "BLI_compiler_typecheck.h"

/* min/max */
#if defined(__GNUC__) || defined(__clang__)

#define MIN2(a, b) __extension__ ({  \
	typeof(a) a_ = (a); typeof(b) b_ = (b); \
	((a_) < (b_) ? (a_) : (b_)); })

#define MAX2(a, b) __extension__ ({  \
	typeof(a) a_ = (a); typeof(b) b_ = (b); \
	((a_) > (b_) ? (a_) : (b_)); })

#define MIN3(a, b, c) __extension__ ({  \
	typeof(a) a_ = (a); typeof(b) b_ = (b); typeof(c) c_ = (c); \
	((a_ < b_) ? ((a_ < c_) ? a_ : c_) : ((b_ < c_) ? b_ : c_)); })

#define MAX3(a, b, c) __extension__ ({  \
	typeof(a) a_ = (a); typeof(b) b_ = (b);  typeof(c) c_ = (c); \
	((a_ > b_) ? ((a_ > c_) ? a_ : c_) : ((b_ > c_) ? b_ : c_)); })

#define MIN4(a, b, c, d) __extension__ ({  \
	typeof(a) a_ = (a); typeof(b) b_ = (b); typeof(c) c_ = (c); typeof(d) d_ = (d); \
	((a_ < b_) ? ((a_ < c_) ? ((a_ < d_) ? a_ : d_) : ((c_ < d_) ? c_ : d_)) : \
	             ((b_ < c_) ? ((b_ < d_) ? b_ : d_) : ((c_ < d_) ? c_ : d_))); })

#define MAX4(a, b, c, d) __extension__ ({  \
	typeof(a) a_ = (a); typeof(b) b_ = (b); typeof(c) c_ = (c); typeof(d) d_ = (d); \
	((a_ > b_) ? ((a_ > c_) ? ((a_ > d_) ? a_ : d_) : ((c_ > d_) ? c_ : d_)) : \
	             ((b_ > c_) ? ((b_ > d_) ? b_ : d_) : ((c_ > d_) ? c_ : d_))); })

#else
#define MIN2(a, b)  ((a) < (b) ? (a) : (b))
#define MAX2(a, b)  ((a) > (b) ? (a) : (b))

#define MIN3(a, b, c)       (MIN2(MIN2((a), (b)), (c)))
#define MIN4(a, b, c, d)    (MIN2(MIN2((a), (b)), MIN2((c), (d))))

#define MAX3(a, b, c)       (MAX2(MAX2((a), (b)), (c)))
#define MAX4(a, b, c, d)    (MAX2(MAX2((a), (b)), MAX2((c), (d))))
#endif

/* min/max that return a value of our choice */
#define MAX3_PAIR(cmp_a, cmp_b, cmp_c, ret_a, ret_b, ret_c) \
	((cmp_a > cmp_b) ? ((cmp_a > cmp_c) ? ret_a : ret_c) : \
	                   ((cmp_b > cmp_c) ? ret_b : ret_c))

#define MIN3_PAIR(cmp_a, cmp_b, cmp_c, ret_a, ret_b, ret_c) \
	((cmp_a < cmp_b) ? ((cmp_a < cmp_c) ? ret_a : ret_c) : \
	                   ((cmp_b < cmp_c) ? ret_b : ret_c))

#define INIT_MINMAX(min, max) {                                               \
		(min)[0] = (min)[1] = (min)[2] =  1.0e30f;                            \
		(max)[0] = (max)[1] = (max)[2] = -1.0e30f;                            \
	} (void)0
#define INIT_MINMAX2(min, max) {                                              \
		(min)[0] = (min)[1] = 1.0e30f;                                        \
		(max)[0] = (max)[1] = -1.0e30f;                                       \
	} (void)0
#define DO_MIN(vec, min) {                                                    \
		if ((min)[0] > (vec)[0]) (min)[0] = (vec)[0];                         \
		if ((min)[1] > (vec)[1]) (min)[1] = (vec)[1];                         \
		if ((min)[2] > (vec)[2]) (min)[2] = (vec)[2];                         \
	} (void)0
#define DO_MAX(vec, max) {                                                    \
		if ((max)[0] < (vec)[0]) (max)[0] = (vec)[0];                         \
		if ((max)[1] < (vec)[1]) (max)[1] = (vec)[1];                         \
		if ((max)[2] < (vec)[2]) (max)[2] = (vec)[2];                         \
	} (void)0
#define DO_MINMAX(vec, min, max) {                                            \
		if ((min)[0] > (vec)[0] ) (min)[0] = (vec)[0];                        \
		if ((min)[1] > (vec)[1] ) (min)[1] = (vec)[1];                        \
		if ((min)[2] > (vec)[2] ) (min)[2] = (vec)[2];                        \
		if ((max)[0] < (vec)[0] ) (max)[0] = (vec)[0];                        \
		if ((max)[1] < (vec)[1] ) (max)[1] = (vec)[1];                        \
		if ((max)[2] < (vec)[2] ) (max)[2] = (vec)[2];                        \
	} (void)0
#define DO_MINMAX2(vec, min, max) {                                           \
		if ((min)[0] > (vec)[0] ) (min)[0] = (vec)[0];                        \
		if ((min)[1] > (vec)[1] ) (min)[1] = (vec)[1];                        \
		if ((max)[0] < (vec)[0] ) (max)[0] = (vec)[0];                        \
		if ((max)[1] < (vec)[1] ) (max)[1] = (vec)[1];                        \
	} (void)0

/* some math and copy defines */


#define SWAP(type, a, b)  {    \
	type sw_ap;                \
	CHECK_TYPE(a, type);       \
	CHECK_TYPE(b, type);       \
	sw_ap = (a);               \
	(a) = (b);                 \
	(b) = sw_ap;               \
} (void)0

/* swap with a temp value */
#define SWAP_TVAL(tval, a, b)  {  \
	CHECK_TYPE_PAIR(tval, a);     \
	CHECK_TYPE_PAIR(tval, b);     \
	(tval) = (a);                 \
	(a) = (b);                    \
	(b) = (tval);                 \
} (void)0

/* ELEM#(v, ...): is the first arg equal any others? */
/* internal helpers*/
#define _VA_ELEM2(v, a) \
       ((v) == (a))
#define _VA_ELEM3(v, a, b) \
       (_VA_ELEM2(v, a) || ((v) == (b)))
#define _VA_ELEM4(v, a, b, c) \
       (_VA_ELEM3(v, a, b) || ((v) == (c)))
#define _VA_ELEM5(v, a, b, c, d) \
       (_VA_ELEM4(v, a, b, c) || ((v) == (d)))
#define _VA_ELEM6(v, a, b, c, d, e) \
       (_VA_ELEM5(v, a, b, c, d) || ((v) == (e)))
#define _VA_ELEM7(v, a, b, c, d, e, f) \
       (_VA_ELEM6(v, a, b, c, d, e) || ((v) == (f)))
#define _VA_ELEM8(v, a, b, c, d, e, f, g) \
       (_VA_ELEM7(v, a, b, c, d, e, f) || ((v) == (g)))
#define _VA_ELEM9(v, a, b, c, d, e, f, g, h) \
       (_VA_ELEM8(v, a, b, c, d, e, f, g) || ((v) == (h)))
#define _VA_ELEM10(v, a, b, c, d, e, f, g, h, i) \
       (_VA_ELEM9(v, a, b, c, d, e, f, g, h) || ((v) == (i)))
#define _VA_ELEM11(v, a, b, c, d, e, f, g, h, i, j) \
       (_VA_ELEM10(v, a, b, c, d, e, f, g, h, i) || ((v) == (j)))
#define _VA_ELEM12(v, a, b, c, d, e, f, g, h, i, j, k) \
       (_VA_ELEM11(v, a, b, c, d, e, f, g, h, i, j) || ((v) == (k)))
#define _VA_ELEM13(v, a, b, c, d, e, f, g, h, i, j, k, l) \
       (_VA_ELEM12(v, a, b, c, d, e, f, g, h, i, j, k) || ((v) == (l)))
#define _VA_ELEM14(v, a, b, c, d, e, f, g, h, i, j, k, l, m) \
       (_VA_ELEM13(v, a, b, c, d, e, f, g, h, i, j, k, l) || ((v) == (m)))
#define _VA_ELEM15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
       (_VA_ELEM14(v, a, b, c, d, e, f, g, h, i, j, k, l, m) || ((v) == (n)))
#define _VA_ELEM16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) \
       (_VA_ELEM15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n) || ((v) == (o)))
#define _VA_ELEM17(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
       (_VA_ELEM16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) || ((v) == (p)))

/* reusable ELEM macro */
#define ELEM(...) VA_NARGS_CALL_OVERLOAD(_VA_ELEM, __VA_ARGS__)

/* no-op for expressions we don't want to instansiate, but must remian valid */
#define EXPR_NOP(expr) (void)(0 ? ((void)(expr), 1) : 0)

/* shift around elements */
#define SHIFT3(type, a, b, c)  {                                              \
	type tmp;                                                                 \
	CHECK_TYPE(a, type);                                                      \
	CHECK_TYPE(b, type);                                                      \
	CHECK_TYPE(c, type);                                                      \
	tmp = a;                                                                  \
	a = c;                                                                    \
	c = b;                                                                    \
	b = tmp;                                                                  \
} (void)0

#define SHIFT4(type, a, b, c, d)  {                                           \
	type tmp;                                                                 \
	CHECK_TYPE(a, type);                                                      \
	CHECK_TYPE(b, type);                                                      \
	CHECK_TYPE(c, type);                                                      \
	CHECK_TYPE(d, type);                                                      \
	tmp = a;                                                                  \
	a = d;                                                                    \
	d = c;                                                                    \
	c = b;                                                                    \
	b = tmp;                                                                  \
} (void)0


#define FTOCHAR(val) ((CHECK_TYPE_INLINE(val, float)), \
		(char)(((val) <= 0.0f) ? 0 : (((val) > (1.0f - 0.5f / 255.0f)) ? 255 : ((255.0f * (val)) + 0.5f))))
#define FTOUSHORT(val) ((CHECK_TYPE_INLINE(val, float)), \
		((val >= 1.0f - 0.5f / 65535) ? 65535 : (val <= 0.0f) ? 0 : (unsigned short)(val * 65535.0f + 0.5f)))
#define USHORTTOUCHAR(val) ((unsigned char)(((val) >= 65535 - 128) ? 255 : ((val) + 128) >> 8))
#define F3TOCHAR3(v2, v1) {                                                   \
		(v1)[0] = FTOCHAR((v2[0]));                                           \
		(v1)[1] = FTOCHAR((v2[1]));                                           \
		(v1)[2] = FTOCHAR((v2[2]));                                           \
} (void)0
#define F3TOCHAR4(v2, v1) {                                                   \
		(v1)[0] = FTOCHAR((v2[0]));                                           \
		(v1)[1] = FTOCHAR((v2[1]));                                           \
		(v1)[2] = FTOCHAR((v2[2]));                                           \
		(v1)[3] = 255;                                                        \
} (void)0
#define F4TOCHAR4(v2, v1) {                                                   \
		(v1)[0] = FTOCHAR((v2[0]));                                           \
		(v1)[1] = FTOCHAR((v2[1]));                                           \
		(v1)[2] = FTOCHAR((v2[2]));                                           \
		(v1)[3] = FTOCHAR((v2[3]));                                           \
} (void)0
#define VECCOPY(v1, v2) {                                                     \
		*(v1) =   *(v2);                                                      \
		*(v1 + 1) = *(v2 + 1);                                                \
		*(v1 + 2) = *(v2 + 2);                                                \
} (void)0
#define VECCOPY2D(v1, v2) {                                                   \
		*(v1) =   *(v2);                                                      \
		*(v1 + 1) = *(v2 + 1);                                                \
} (void)0
#define VECADD(v1, v2, v3) {                                                  \
		*(v1) =   *(v2)   + *(v3);                                            \
		*(v1 + 1) = *(v2 + 1) + *(v3 + 1);                                    \
		*(v1 + 2) = *(v2 + 2) + *(v3 + 2);                                    \
} (void)0
#define VECSUB(v1, v2, v3) {                                                  \
		*(v1) =   *(v2)   - *(v3);                                            \
		*(v1 + 1) = *(v2 + 1) - *(v3 + 1);                                    \
		*(v1 + 2) = *(v2 + 2) - *(v3 + 2);                                    \
} (void)0
#define VECSUB2D(v1, v2, v3)     {                                            \
		*(v1) =   *(v2)   - *(v3);                                            \
		*(v1 + 1) = *(v2 + 1) - *(v3 + 1);                                    \
} (void)0
#define VECADDFAC(v1, v2, v3, fac) {                                          \
		*(v1) =   *(v2)   + *(v3) * (fac);                                    \
		*(v1 + 1) = *(v2 + 1) + *(v3 + 1) * (fac);                            \
		*(v1 + 2) = *(v2 + 2) + *(v3 + 2) * (fac);                            \
} (void)0
#define VECMADD(v1, v2, v3, v4) {                                             \
		*(v1) =   *(v2)   + *(v3) * (*(v4));                                  \
		*(v1 + 1) = *(v2 + 1) + *(v3 + 1) * (*(v4 + 1));                      \
		*(v1 + 2) = *(v2 + 2) + *(v3 + 2) * (*(v4 + 2));                      \
} (void)0
#define VECSUBFAC(v1, v2, v3, fac) {                                          \
		*(v1) =   *(v2)   - *(v3) * (fac);                                    \
		*(v1 + 1) = *(v2 + 1) - *(v3 + 1) * (fac);                            \
		*(v1 + 2) = *(v2 + 2) - *(v3 + 2) * (fac);                            \
} (void)0

/* some misc stuff.... */

/* avoid multiple access for supported compilers */
#if defined(__GNUC__) || defined(__clang__)

#define ABS(a)  ({ \
	typeof(a) a_ = (a); \
	((a_) < 0 ? (-(a_)) : (a_)); })
#define SQUARE(a)  ({ \
	typeof(a) a_ = (a); \
	((a_) * (a_)); })

#else

#define ABS(a)  ((a) < 0 ? (-(a)) : (a))
#define SQUARE(a)  ((a) * (a))

#endif

#define CLAMPIS(a, b, c)  ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))

#define CLAMP(a, b, c)  {           \
	if      ((a) < (b)) (a) = (b);  \
	else if ((a) > (c)) (a) = (c);  \
} (void)0

#define CLAMP_MAX(a, c)  {          \
	if ((a) > (c)) (a) = (c);       \
} (void)0

#define CLAMP_MIN(a, b)  {          \
	if      ((a) < (b)) (a) = (b);  \
} (void)0

#define CLAMP2(vec, b, c) { \
	CLAMP((vec)[0], b, c); \
	CLAMP((vec)[1], b, c); \
} (void)0

#define CLAMP2_MIN(vec, b) { \
	CLAMP_MIN((vec)[0], b); \
	CLAMP_MIN((vec)[1], b); \
} (void)0

#define CLAMP2_MAX(vec, b) { \
	CLAMP_MAX((vec)[0], b); \
	CLAMP_MAX((vec)[1], b); \
} (void)0

#define CLAMP3(vec, b, c) { \
	CLAMP((vec)[0], b, c); \
	CLAMP((vec)[1], b, c); \
	CLAMP((vec)[2], b, c); \
} (void)0

#define CLAMP3_MIN(vec, b) { \
	CLAMP_MIN((vec)[0], b); \
	CLAMP_MIN((vec)[1], b); \
	CLAMP_MIN((vec)[2], b); \
} (void)0

#define CLAMP3_MAX(vec, b) { \
	CLAMP_MAX((vec)[0], b); \
	CLAMP_MAX((vec)[1], b); \
	CLAMP_MAX((vec)[2], b); \
} (void)0

#define CLAMP4(vec, b, c) { \
	CLAMP((vec)[0], b, c); \
	CLAMP((vec)[1], b, c); \
	CLAMP((vec)[2], b, c); \
	CLAMP((vec)[3], b, c); \
} (void)0

#define CLAMP4_MIN(vec, b) { \
	CLAMP_MIN((vec)[0], b); \
	CLAMP_MIN((vec)[1], b); \
	CLAMP_MIN((vec)[2], b); \
	CLAMP_MIN((vec)[3], b); \
} (void)0

#define CLAMP4_MAX(vec, b) { \
	CLAMP_MAX((vec)[0], b); \
	CLAMP_MAX((vec)[1], b); \
	CLAMP_MAX((vec)[2], b); \
	CLAMP_MAX((vec)[3], b); \
} (void)0

#define IS_EQ(a, b)  ( \
	CHECK_TYPE_INLINE(a, double), CHECK_TYPE_INLINE(b, double), \
	((fabs((double)((a) - (b))) >= (double) FLT_EPSILON) ? false : true))

#define IS_EQF(a, b)  ( \
	CHECK_TYPE_INLINE(a, float), CHECK_TYPE_INLINE(b, float), \
	((fabsf((float)((a) - (b))) >= (float) FLT_EPSILON) ? false : true))

#define IS_EQT(a, b, c) ((a > b) ? (((a - b) <= c) ? 1 : 0) : ((((b - a) <= c) ? 1 : 0)))
#define IN_RANGE(a, b, c) ((b < c) ? ((b < a && a < c) ? 1 : 0) : ((c < a && a < b) ? 1 : 0))
#define IN_RANGE_INCL(a, b, c) ((b < c) ? ((b <= a && a <= c) ? 1 : 0) : ((c <= a && a <= b) ? 1 : 0))

/* unpack vector for args */
#define UNPACK2(a)  ((a)[0]),   ((a)[1])
#define UNPACK3(a)  UNPACK2(a), ((a)[2])
#define UNPACK4(a)  UNPACK3(a), ((a)[3])
/* pre may be '&', '*' or func, post may be '->member' */
#define UNPACK2_EX(pre, a, post)  (pre((a)[0])post),        (pre((a)[1])post)
#define UNPACK3_EX(pre, a, post)  UNPACK2_EX(pre, a, post), (pre((a)[2])post)
#define UNPACK4_EX(pre, a, post)  UNPACK3_EX(pre, a, post), (pre((a)[3])post)

/* array helpers */
#define ARRAY_LAST_ITEM(arr_start, arr_dtype, tot) \
	(arr_dtype *)((char *)arr_start + (sizeof(*((arr_dtype *)NULL)) * (size_t)(tot - 1)))

#define ARRAY_HAS_ITEM(arr_item, arr_start, tot)  ( \
	CHECK_TYPE_PAIR_INLINE(arr_start, arr_item), \
	((unsigned int)((arr_item) - (arr_start)) < (unsigned int)(tot)))

#define ARRAY_DELETE(arr, index, tot_delete, tot)  { \
		BLI_assert(index + tot_delete <= tot);  \
		memmove(&(arr)[(index)], \
		        &(arr)[(index) + (tot_delete)], \
		         (((tot) - (index)) - (tot_delete)) * sizeof(*(arr))); \
	} (void)0

/* assuming a static array */
#if defined(__GNUC__) && !defined(__cplusplus)
#  define ARRAY_SIZE(arr) \
	((sizeof(struct {int isnt_array : ((const void *)&(arr) == &(arr)[0]);}) * 0) + \
	 (sizeof(arr) / sizeof(*(arr))))
#else
#  define ARRAY_SIZE(arr)  (sizeof(arr) / sizeof(*(arr)))
#endif

/* ELEM#(v, ...): is the first arg equal any others? */
/* internal helpers*/
#define _VA_ARRAY_SET_ITEMS2(v, a) \
       ((v)[0] = (a))
#define _VA_ARRAY_SET_ITEMS3(v, a, b) \
       _VA_ARRAY_SET_ITEMS2(v, a); ((v)[1] = (b))
#define _VA_ARRAY_SET_ITEMS4(v, a, b, c) \
       _VA_ARRAY_SET_ITEMS3(v, a, b); ((v)[2] = (c))
#define _VA_ARRAY_SET_ITEMS5(v, a, b, c, d) \
       _VA_ARRAY_SET_ITEMS4(v, a, b, c); ((v)[3] = (d))
#define _VA_ARRAY_SET_ITEMS6(v, a, b, c, d, e) \
       _VA_ARRAY_SET_ITEMS5(v, a, b, c, d); ((v)[4] = (e))
#define _VA_ARRAY_SET_ITEMS7(v, a, b, c, d, e, f) \
       _VA_ARRAY_SET_ITEMS6(v, a, b, c, d, e); ((v)[5] = (f))
#define _VA_ARRAY_SET_ITEMS8(v, a, b, c, d, e, f, g) \
       _VA_ARRAY_SET_ITEMS7(v, a, b, c, d, e, f); ((v)[6] = (g))
#define _VA_ARRAY_SET_ITEMS9(v, a, b, c, d, e, f, g, h) \
       _VA_ARRAY_SET_ITEMS8(v, a, b, c, d, e, f, g); ((v)[7] = (h))
#define _VA_ARRAY_SET_ITEMS10(v, a, b, c, d, e, f, g, h, i) \
       _VA_ARRAY_SET_ITEMS9(v, a, b, c, d, e, f, g, h); ((v)[8] = (i))
#define _VA_ARRAY_SET_ITEMS11(v, a, b, c, d, e, f, g, h, i, j) \
       _VA_ARRAY_SET_ITEMS10(v, a, b, c, d, e, f, g, h, i); ((v)[9] = (j))
#define _VA_ARRAY_SET_ITEMS12(v, a, b, c, d, e, f, g, h, i, j, k) \
       _VA_ARRAY_SET_ITEMS11(v, a, b, c, d, e, f, g, h, i, j); ((v)[10] = (k))
#define _VA_ARRAY_SET_ITEMS13(v, a, b, c, d, e, f, g, h, i, j, k, l) \
       _VA_ARRAY_SET_ITEMS12(v, a, b, c, d, e, f, g, h, i, j, k); ((v)[11] = (l))
#define _VA_ARRAY_SET_ITEMS14(v, a, b, c, d, e, f, g, h, i, j, k, l, m) \
       _VA_ARRAY_SET_ITEMS13(v, a, b, c, d, e, f, g, h, i, j, k, l); ((v)[12] = (m))
#define _VA_ARRAY_SET_ITEMS15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n) \
       _VA_ARRAY_SET_ITEMS14(v, a, b, c, d, e, f, g, h, i, j, k, l, m); ((v)[13] = (n))
#define _VA_ARRAY_SET_ITEMS16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) \
       _VA_ARRAY_SET_ITEMS15(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n); ((v)[14] = (o))
#define _VA_ARRAY_SET_ITEMS17(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
       _VA_ARRAY_SET_ITEMS16(v, a, b, c, d, e, f, g, h, i, j, k, l, m, n, o); ((v)[15] = (p))

/* reusable ELEM macro */
#define ARRAY_SET_ITEMS(...) { VA_NARGS_CALL_OVERLOAD(_VA_ARRAY_SET_ITEMS, __VA_ARGS__); } (void)0

/* Like offsetof(typeof(), member), for non-gcc compilers */
#define OFFSETOF_STRUCT(_struct, _member) \
	((((char *)&((_struct)->_member)) - ((char *)(_struct))) + sizeof((_struct)->_member))

/* memcpy, skipping the first part of a struct,
 * ensures 'struct_dst' isn't const and that the offset can be computed at compile time */
#define MEMCPY_STRUCT_OFS(struct_dst, struct_src, member)  { \
	CHECK_TYPE_NONCONST(struct_dst); \
	((void)(struct_dst == struct_src), \
	 memcpy((char *)(struct_dst)  + OFFSETOF_STRUCT(struct_dst, member), \
	        (char *)(struct_src)  + OFFSETOF_STRUCT(struct_dst, member), \
	        sizeof(*(struct_dst)) - OFFSETOF_STRUCT(struct_dst, member))); \
} (void)0

/* Warning-free macros for storing ints in pointers. Use these _only_
 * for storing an int in a pointer, not a pointer in an int (64bit)! */
#define SET_INT_IN_POINTER(i)    ((void *)(intptr_t)(i))
#define GET_INT_FROM_POINTER(i)  ((void)0, ((int)(intptr_t)(i)))

#define SET_UINT_IN_POINTER(i)    ((void *)(uintptr_t)(i))
#define GET_UINT_FROM_POINTER(i)  ((void)0, ((unsigned int)(uintptr_t)(i)))


/* Macro to convert a value to string in the preprocessor
 * STRINGIFY_ARG: gives the argument as a string
 * STRINGIFY_APPEND: appends any argument 'b' onto the string argument 'a',
 *   used by STRINGIFY because some preprocessors warn about zero arguments
 * STRINGIFY: gives the argument's value as a string */
#define STRINGIFY_ARG(x) "" #x
#define STRINGIFY_APPEND(a, b) "" a #b
#define STRINGIFY(x) STRINGIFY_APPEND("", x)


/* generic strcmp macros */
#define STREQ(a, b) (strcmp(a, b) == 0)
#define STRCASEEQ(a, b) (strcasecmp(a, b) == 0)
#define STREQLEN(a, b, n) (strncmp(a, b, n) == 0)
#define STRCASEEQLEN(a, b, n) (strncasecmp(a, b, n) == 0)

#define STRPREFIX(a, b) (strncmp((a), (b), strlen(b)) == 0)
/* useful for debugging */
#define AT __FILE__ ":" STRINGIFY(__LINE__)


/* UNUSED macro, for function argument */
#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
#  define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
#  define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

/**
 * UNUSED_VARS#(a, ...): quiet unused warnings
 *
 * <pre>
 * for i in range(16):
 *     args = [(chr(ord('a') + (c % 26)) + (chr(ord('0') + (c // 26)))) for c in range(i + 1)]
 *     print("#define _VA_UNUSED_VARS_%d(%s) \\" % (i + 1, ", ".join(args)))
 *     print("\t((void)(%s)%s)" %
 *             (args[0], ((", _VA_UNUSED_VARS_" + str(i) + "(%s)") if i else "%s") % ", ".join((args[1:]))))
 * </pre>
 *
 */

#define _VA_UNUSED_VARS_1(a0) \
	((void)(a0))
#define _VA_UNUSED_VARS_2(a0, b0) \
	((void)(a0), _VA_UNUSED_VARS_1(b0))
#define _VA_UNUSED_VARS_3(a0, b0, c0) \
	((void)(a0), _VA_UNUSED_VARS_2(b0, c0))
#define _VA_UNUSED_VARS_4(a0, b0, c0, d0) \
	((void)(a0), _VA_UNUSED_VARS_3(b0, c0, d0))
#define _VA_UNUSED_VARS_5(a0, b0, c0, d0, e0) \
	((void)(a0), _VA_UNUSED_VARS_4(b0, c0, d0, e0))
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

/*little macro so inline keyword works*/
#if defined(_MSC_VER)
#  define BLI_INLINE static __forceinline
#else
#  if (defined(__APPLE__) && defined(__ppc__))
/* static inline __attribute__ here breaks osx ppc gcc42 build */
#    define BLI_INLINE static __attribute__((always_inline))
#  else
#    define BLI_INLINE static inline __attribute__((always_inline))
#  endif
#endif


/* BLI_assert(), default only to print
 * for aborting need to define WITH_ASSERT_ABORT
 */
#ifndef NDEBUG
extern void BLI_system_backtrace(FILE *fp);
#  ifdef WITH_ASSERT_ABORT
#    define _BLI_DUMMY_ABORT abort
#  else
#    define _BLI_DUMMY_ABORT() (void)0
#  endif
#  if defined(__GNUC__) || defined(_MSC_VER) /* check __func__ is available */
#    define BLI_assert(a)                                                     \
	(void)((!(a)) ?  (                                                        \
		(                                                                     \
		BLI_system_backtrace(stderr),                                         \
		fprintf(stderr,                                                       \
			"BLI_assert failed: %s:%d, %s(), at \'%s\'\n",                    \
			__FILE__, __LINE__, __func__, STRINGIFY(a)),                      \
		_BLI_DUMMY_ABORT(),                                                   \
		NULL)) : NULL)
#  else
#    define BLI_assert(a)                                                     \
	(void)((!(a)) ?  (                                                        \
		(                                                                     \
		fprintf(stderr,                                                       \
			"BLI_assert failed: %s:%d, at \'%s\'\n",                          \
			__FILE__, __LINE__, STRINGIFY(a)),                                \
		_BLI_DUMMY_ABORT(),                                                   \
		NULL)) : NULL)
#  endif
#else
#  define BLI_assert(a) (void)0
#endif

/* C++ can't use _Static_assert, expects static_assert() but c++0x only,
 * Coverity also errors out. */
#if (!defined(__cplusplus)) && \
    (!defined(__COVERITY__)) && \
    (defined(__GNUC__) && ((__GNUC__ * 100 + __GNUC_MINOR__) >= 406))  /* gcc4.6+ only */
#  define BLI_STATIC_ASSERT(a, msg) __extension__ _Static_assert(a, msg);
#else
   /* TODO msvc, clang */
#  define BLI_STATIC_ASSERT(a, msg)
#endif

/* hints for branch prediction, only use in code that runs a _lot_ where */
#ifdef __GNUC__
#  define LIKELY(x)       __builtin_expect(!!(x), 1)
#  define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x)       (x)
#  define UNLIKELY(x)     (x)
#endif

#ifdef __cplusplus
}
#endif

#endif  /* __BLI_UTILDEFINES_H__ */
