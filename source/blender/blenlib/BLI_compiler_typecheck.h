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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_COMPILER_TYPECHECK_H__
#define __BLI_COMPILER_TYPECHECK_H__

/** \file BLI_compiler_typecheck.h
 *  \ingroup bli
 *
 * Type checking macros (often used to ensure valid use of macro args).
 * These depend on compiler extensions and c11 in some cases.
 */

/* Causes warning:
 * incompatible types when assigning to type 'Foo' from type 'Bar'
 * ... the compiler optimizes away the temp var */
#ifdef __GNUC__
#define CHECK_TYPE(var, type)  {  \
	typeof(var) *__tmp;           \
	__tmp = (type *)NULL;         \
	(void)__tmp;                  \
} (void)0

#define CHECK_TYPE_PAIR(var_a, var_b)  {  \
	typeof(var_a) *__tmp;                 \
	__tmp = (typeof(var_b) *)NULL;        \
	(void)__tmp;                          \
} (void)0

#define CHECK_TYPE_PAIR_INLINE(var_a, var_b)  ((void)({  \
	typeof(var_a) *__tmp;                                \
	__tmp = (typeof(var_b) *)NULL;                       \
	(void)__tmp;                                         \
}))

#else
#  define CHECK_TYPE(var, type)  { EXPR_NOP(var); }(void)0
#  define CHECK_TYPE_PAIR(var_a, var_b)  { (EXPR_NOP(var_a), EXPR_NOP(var_b)); }(void)0
#  define CHECK_TYPE_PAIR_INLINE(var_a, var_b)  (EXPR_NOP(var_a), EXPR_NOP(var_b))
#endif

/* can be used in simple macros */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define CHECK_TYPE_INLINE(val, type) \
	(void)((void)(((type)0) != (0 ? (val) : ((type)0))), \
	       _Generic((val), type: 0, const type: 0))
#else
#  define CHECK_TYPE_INLINE(val, type) \
	((void)(((type)0) != (0 ? (val) : ((type)0))))
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define CHECK_TYPE_NONCONST(var) __extension__ ({ \
	void *non_const = 0 ? (var) : NULL; \
	(void)non_const; \
})
#else
#  define CHECK_TYPE_NONCONST(var) EXPR_NOP(var)
#endif


/**
 * CHECK_TYPE_ANY: handy macro, eg:
 * ``CHECK_TYPE_ANY(var, Foo *, Bar *, Baz *)``
 *
 * excuse ridiculously long generated args.
 * <pre>
 * for i in range(63):
 *     args = [(chr(ord('a') + (c % 26)) + (chr(ord('0') + (c // 26)))) for c in range(i + 1)]
 *     print("#define _VA_CHECK_TYPE_ANY%d(v, %s) \\" % (i + 2, ", ".join(args)))
 *     print("    ((void)_Generic((v), %s))" % (": 0, ".join(args) + ": 0"))
 * </pre>
 */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)

#define _VA_CHECK_TYPE_ANY2(v, a0) \
	((void)_Generic((v), a0: 0))
#define _VA_CHECK_TYPE_ANY3(v, a0, b0) \
	((void)_Generic((v), a0: 0, b0: 0))
#define _VA_CHECK_TYPE_ANY4(v, a0, b0, c0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0))
#define _VA_CHECK_TYPE_ANY5(v, a0, b0, c0, d0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0))
#define _VA_CHECK_TYPE_ANY6(v, a0, b0, c0, d0, e0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0))
#define _VA_CHECK_TYPE_ANY7(v, a0, b0, c0, d0, e0, f0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0))
#define _VA_CHECK_TYPE_ANY8(v, a0, b0, c0, d0, e0, f0, g0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0))
#define _VA_CHECK_TYPE_ANY9(v, a0, b0, c0, d0, e0, f0, g0, h0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0))
#define _VA_CHECK_TYPE_ANY10(v, a0, b0, c0, d0, e0, f0, g0, h0, i0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0))
#define _VA_CHECK_TYPE_ANY11(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0))
#define _VA_CHECK_TYPE_ANY12(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0))
#define _VA_CHECK_TYPE_ANY13(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0))
#define _VA_CHECK_TYPE_ANY14(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0))
#define _VA_CHECK_TYPE_ANY15(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0))
#define _VA_CHECK_TYPE_ANY16(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0))
#define _VA_CHECK_TYPE_ANY17(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0))
#define _VA_CHECK_TYPE_ANY18(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0))
#define _VA_CHECK_TYPE_ANY19(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0))
#define _VA_CHECK_TYPE_ANY20(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0))
#define _VA_CHECK_TYPE_ANY21(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0))
#define _VA_CHECK_TYPE_ANY22(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0))
#define _VA_CHECK_TYPE_ANY23(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0))
#define _VA_CHECK_TYPE_ANY24(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0))
#define _VA_CHECK_TYPE_ANY25(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0))
#define _VA_CHECK_TYPE_ANY26(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0))
#define _VA_CHECK_TYPE_ANY27(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0))
#define _VA_CHECK_TYPE_ANY28(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0))
#define _VA_CHECK_TYPE_ANY29(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0))
#define _VA_CHECK_TYPE_ANY30(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0))
#define _VA_CHECK_TYPE_ANY31(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0))
#define _VA_CHECK_TYPE_ANY32(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0))
#define _VA_CHECK_TYPE_ANY33(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0))
#define _VA_CHECK_TYPE_ANY34(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0))
#define _VA_CHECK_TYPE_ANY35(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0))
#define _VA_CHECK_TYPE_ANY36(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0))
#define _VA_CHECK_TYPE_ANY37(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0))
#define _VA_CHECK_TYPE_ANY38(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0))
#define _VA_CHECK_TYPE_ANY39(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0))
#define _VA_CHECK_TYPE_ANY40(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0))
#define _VA_CHECK_TYPE_ANY41(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0))
#define _VA_CHECK_TYPE_ANY42(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0))
#define _VA_CHECK_TYPE_ANY43(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0))
#define _VA_CHECK_TYPE_ANY44(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0))
#define _VA_CHECK_TYPE_ANY45(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0))
#define _VA_CHECK_TYPE_ANY46(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0))
#define _VA_CHECK_TYPE_ANY47(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0))
#define _VA_CHECK_TYPE_ANY48(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0))
#define _VA_CHECK_TYPE_ANY49(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0))
#define _VA_CHECK_TYPE_ANY50(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0))
#define _VA_CHECK_TYPE_ANY51(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0))
#define _VA_CHECK_TYPE_ANY52(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0))
#define _VA_CHECK_TYPE_ANY53(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0))
#define _VA_CHECK_TYPE_ANY54(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0))
#define _VA_CHECK_TYPE_ANY55(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0))
#define _VA_CHECK_TYPE_ANY56(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0))
#define _VA_CHECK_TYPE_ANY57(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2, d2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0, d2: 0))
#define _VA_CHECK_TYPE_ANY58(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2, d2, e2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0, d2: 0, e2: 0))
#define _VA_CHECK_TYPE_ANY59(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2, d2, e2, f2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0, d2: 0, e2: 0, f2: 0))
#define _VA_CHECK_TYPE_ANY60(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2, d2, e2, f2, g2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0, d2: 0, e2: 0, f2: 0, g2: 0))
#define _VA_CHECK_TYPE_ANY61(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2, d2, e2, f2, g2, h2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0, d2: 0, e2: 0, f2: 0, g2: 0, h2: 0))
#define _VA_CHECK_TYPE_ANY62(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2, d2, e2, f2, g2, h2, i2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0, d2: 0, e2: 0, f2: 0, g2: 0, h2: 0, i2: 0))
#define _VA_CHECK_TYPE_ANY63(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2, d2, e2, f2, g2, h2, i2, j2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0, d2: 0, e2: 0, f2: 0, g2: 0, h2: 0, i2: 0, \
	j2: 0))
#define _VA_CHECK_TYPE_ANY64(v, a0, b0, c0, d0, e0, f0, g0, h0, i0, j0, k0, l0, m0, n0, o0, p0, q0, r0, s0, t0, u0, \
	v0, w0, x0, y0, z0, a1, b1, c1, d1, e1, f1, g1, h1, i1, j1, k1, l1, m1, n1, o1, p1, q1, r1, s1, t1, u1, v1, w1, \
	x1, y1, z1, a2, b2, c2, d2, e2, f2, g2, h2, i2, j2, k2) \
	((void)_Generic((v), a0: 0, b0: 0, c0: 0, d0: 0, e0: 0, f0: 0, g0: 0, h0: 0, i0: 0, j0: 0, k0: 0, l0: 0, m0: 0, \
	n0: 0, o0: 0, p0: 0, q0: 0, r0: 0, s0: 0, t0: 0, u0: 0, v0: 0, w0: 0, x0: 0, y0: 0, z0: 0, a1: 0, b1: 0, c1: 0, \
	d1: 0, e1: 0, f1: 0, g1: 0, h1: 0, i1: 0, j1: 0, k1: 0, l1: 0, m1: 0, n1: 0, o1: 0, p1: 0, q1: 0, r1: 0, s1: 0, \
	t1: 0, u1: 0, v1: 0, w1: 0, x1: 0, y1: 0, z1: 0, a2: 0, b2: 0, c2: 0, d2: 0, e2: 0, f2: 0, g2: 0, h2: 0, i2: 0, \
	j2: 0, k2: 0))
#  define CHECK_TYPE_ANY(...) VA_NARGS_CALL_OVERLOAD(_VA_CHECK_TYPE_ANY, __VA_ARGS__)
#else
#  define CHECK_TYPE_ANY(...) (void)0
#endif

#endif  /* __BLI_COMPILER_TYPECHECK_H__ */
