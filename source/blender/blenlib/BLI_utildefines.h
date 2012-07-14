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

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif


#define ELEM(a, b, c)           ((a) == (b) || (a) == (c))
#define ELEM3(a, b, c, d)       (ELEM(a, b, c) || (a) == (d) )
#define ELEM4(a, b, c, d, e)    (ELEM(a, b, c) || ELEM(a, d, e) )
#define ELEM5(a, b, c, d, e, f) (ELEM(a, b, c) || ELEM3(a, d, e, f) )
#define ELEM6(a, b, c, d, e, f, g)      (ELEM(a, b, c) || ELEM4(a, d, e, f, g) )
#define ELEM7(a, b, c, d, e, f, g, h)   (ELEM3(a, b, c, d) || ELEM4(a, e, f, g, h) )
#define ELEM8(a, b, c, d, e, f, g, h, i)        (ELEM4(a, b, c, d, e) || ELEM4(a, f, g, h, i) )
#define ELEM9(a, b, c, d, e, f, g, h, i, j)        (ELEM4(a, b, c, d, e) || ELEM5(a, f, g, h, i, j) )
#define ELEM10(a, b, c, d, e, f, g, h, i, j, k)        (ELEM4(a, b, c, d, e) || ELEM6(a, f, g, h, i, j, k) )
#define ELEM11(a, b, c, d, e, f, g, h, i, j, k, l)        (ELEM4(a, b, c, d, e) || ELEM7(a, f, g, h, i, j, k, l) )

/* shift around elements */
#define SHIFT3(type, a, b, c)  {                                              \
		type tmp;                                                             \
		tmp = a;                                                              \
		a = c;                                                                \
		c = b;                                                                \
		b = tmp;                                                              \
} (void)0
#define SHIFT4(type, a, b, c, d)  {                                           \
		type tmp;                                                             \
		tmp = a;                                                              \
		a = d;                                                                \
		d = c;                                                                \
		c = b;                                                                \
		b = tmp;                                                              \
} (void)0

/* min/max */
#define MIN2(x, y)               ( (x) < (y) ? (x) : (y) )
#define MIN3(x, y, z)             MIN2(MIN2((x), (y)), (z) )
#define MIN4(x, y, z, a)           MIN2(MIN2((x), (y)), MIN2((z), (a)) )

#define MAX2(x, y)               ( (x) > (y) ? (x) : (y) )
#define MAX3(x, y, z)             MAX2(MAX2((x), (y)), (z) )
#define MAX4(x, y, z, a)           MAX2(MAX2((x), (y)), MAX2((z), (a)) )

#define INIT_MINMAX(min, max) {                                               \
		(min)[0] = (min)[1] = (min)[2] =  1.0e30f;                            \
		(max)[0] = (max)[1] = (max)[2] = -1.0e30f;                            \
	}
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

#ifndef SWAP
#  define SWAP(type, a, b)       { type sw_ap; sw_ap = (a); (a) = (b); (b) = sw_ap; } (void)0
#endif

#define ABS(a)          ( (a) < 0 ? (-(a)) : (a) )

#define FTOCHAR(val) ((val) <= 0.0f) ? 0 : (((val) > (1.0f - 0.5f / 255.0f)) ? 255 : (char)((255.0f * (val)) + 0.5f))
#define FTOUSHORT(val) ((val >= 1.0f - 0.5f / 65535) ? 65535 : (val <= 0.0f) ? 0 : (unsigned short)(val * 65535.0f + 0.5f))
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
#define VECSUBFAC(v1, v2, v3, fac) {                                          \
		*(v1) =   *(v2)   - *(v3) * (fac);                                    \
		*(v1 + 1) = *(v2 + 1) - *(v3 + 1) * (fac);                            \
		*(v1 + 2) = *(v2 + 2) - *(v3 + 2) * (fac);                            \
} (void)0

#define INPR(v1, v2) ( (v1)[0] * (v2)[0] + (v1)[1] * (v2)[1] + (v1)[2] * (v2)[2])

/* some misc stuff.... */
#define CLAMP(a, b, c)  if ((a) < (b)) (a) = (b); else if ((a) > (c)) (a) = (c)

#define CLAMPIS(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))
#define CLAMPTEST(a, b, c)                                                    \
	if ((b) < (c)) {                                                          \
		CLAMP(a, b, c);                                                       \
	}                                                                         \
	else {                                                                    \
		CLAMP(a, c, b);                                                       \
	} (void)

#define IS_EQ(a, b) ((fabs((double)(a) - (b)) >= (double) FLT_EPSILON) ? 0 : 1)
#define IS_EQF(a, b) ((fabsf((float)(a) - (b)) >= (float) FLT_EPSILON) ? 0 : 1)

#define IS_EQT(a, b, c) ((a > b) ? (((a - b) <= c) ? 1 : 0) : ((((b - a) <= c) ? 1 : 0)))
#define IN_RANGE(a, b, c) ((b < c) ? ((b < a && a < c) ? 1 : 0) : ((c < a && a < b) ? 1 : 0))
#define IN_RANGE_INCL(a, b, c) ((b < c) ? ((b <= a && a <= c) ? 1 : 0) : ((c <= a && a <= b) ? 1 : 0))

/* array helpers */
#define ARRAY_LAST_ITEM(arr_start, arr_dtype, elem_size, tot)                 \
	(arr_dtype *)((char *)arr_start + (elem_size * (tot - 1)))

#define ARRAY_HAS_ITEM(item, arr_start, arr_dtype, elem_size, tot) (          \
		(item >= arr_start) &&                                                \
		(item <= ARRAY_LAST_ITEM(arr_start, arr_dtype, elem_size, tot))       \
	)

/* This one rotates the bytes in an int64, int (32) and short (16) */
#define SWITCH_INT64(a) {                                                     \
		char s_i, *p_i;                                                       \
		p_i = (char *)&(a);                                                   \
		s_i = p_i[0]; p_i[0] = p_i[7]; p_i[7] = s_i;                          \
		s_i = p_i[1]; p_i[1] = p_i[6]; p_i[6] = s_i;                          \
		s_i = p_i[2]; p_i[2] = p_i[5]; p_i[5] = s_i;                          \
		s_i = p_i[3]; p_i[3] = p_i[4]; p_i[4] = s_i;                          \
	} (void)0

#define SWITCH_INT(a) {                                                       \
		char s_i, *p_i;                                                       \
		p_i = (char *)&(a);                                                   \
		s_i = p_i[0]; p_i[0] = p_i[3]; p_i[3] = s_i;                          \
		s_i = p_i[1]; p_i[1] = p_i[2]; p_i[2] = s_i;                          \
	} (void)0

#define SWITCH_SHORT(a) {                                                     \
		char s_i, *p_i;                                                       \
		p_i = (char *)&(a);                                                   \
		s_i = p_i[0]; p_i[0] = p_i[1]; p_i[1] = s_i;                          \
	} (void)0


/* Warning-free macros for storing ints in pointers. Use these _only_
 * for storing an int in a pointer, not a pointer in an int (64bit)! */
#define SET_INT_IN_POINTER(i)    ((void *)(intptr_t)(i))
#define GET_INT_FROM_POINTER(i)  ((int)(intptr_t)(i))

#define SET_UINT_IN_POINTER(i)    ((void *)(uintptr_t)(i))
#define GET_UINT_FROM_POINTER(i)  ((unsigned int)(uintptr_t)(i))


/* Macro to convert a value to string in the preprocessor
 * STRINGIFY_ARG: gives the argument as a string
 * STRINGIFY_APPEND: appends any argument 'b' onto the string argument 'a',
 *   used by STRINGIFY because some preprocessors warn about zero arguments
 * STRINGIFY: gives the argument's value as a string */
#define STRINGIFY_ARG(x) "" #x
#define STRINGIFY_APPEND(a, b) "" a #b
#define STRINGIFY(x) STRINGIFY_APPEND("", x)

/* useful for debugging */
#define AT __FILE__ ":" STRINGIFY(__LINE__)

/* so we can use __func__ everywhere */
#if defined(_MSC_VER)
#  define __func__ __FUNCTION__
#endif


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

#ifdef __GNUC__
#  define WARN_UNUSED  __attribute__((warn_unused_result))
#else
#  define WARN_UNUSED
#endif

/*little macro so inline keyword works*/
#if defined(_MSC_VER)
#  define BLI_INLINE static __forceinline
#elif defined(__GNUC__)
#  define BLI_INLINE static inline __attribute((always_inline))
#else
/* #warning "MSC/GNUC defines not found, inline non-functional" */
#  define BLI_INLINE static
#endif


/* BLI_assert(), default only to print
 * for aborting need to define WITH_ASSERT_ABORT
 */
#ifndef NDEBUG
#  ifdef WITH_ASSERT_ABORT
#    define _BLI_DUMMY_ABORT abort
#  else
#    define _BLI_DUMMY_ABORT() (void)0
#  endif
#  if defined(__GNUC__) || defined(_MSC_VER) /* check __func__ is available */
#    define BLI_assert(a)                                                     \
	(void)((!(a)) ?  (                                                        \
		(                                                                     \
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

/* hints for branch pradiction, only use in code that runs a _lot_ where */
#ifdef __GNUC__
#  define LIKELY(x)       __builtin_expect(!!(x), 1)
#  define UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#  define LIKELY(x)       (x)
#  define UNLIKELY(x)     (x)
#endif

#endif // __BLI_UTILDEFINES_H__
