/*
 * Copyright 2013, Blender Foundation.
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
 */

CCL_NAMESPACE_BEGIN

/* "Correlated Multi-Jittered Sampling"
 * Andrew Kensler, Pixar Technical Memo 13-01, 2013 */

/* todo: find good value, suggested 64 gives pattern on cornell box ceiling */
#define CMJ_RANDOM_OFFSET_LIMIT 4096

__device_inline bool cmj_is_pow2(int i)
{
	return (i & (i - 1)) == 0;
}

__device_inline int cmj_fast_mod_pow2(int a, int b)
{
	return (a & (b - 1));
}

/* a must be > 0 and b must be > 1 */
__device_inline int cmj_fast_div_pow2(int a, int b)
{
#if defined(__KERNEL_SSE2__) && !defined(_MSC_VER)
	return a >> __builtin_ctz(b);
#else
	return a/b;
#endif
}

__device_inline uint cmj_w_mask(uint w)
{
#if defined(__KERNEL_SSE2__) && !defined(_MSC_VER)
	return ((1 << (32 - __builtin_clz(w))) - 1);
#else
	w |= w >> 1;
	w |= w >> 2;
	w |= w >> 4;
	w |= w >> 8;
	w |= w >> 16;

	return w;
#endif
}

__device_inline uint cmj_permute(uint i, uint l, uint p)
{
	uint w = l - 1;

	if((l & w) == 0) {
		/* l is a power of two (fast) */
		i ^= p;
		i *= 0xe170893d;
		i ^= p >> 16;
		i ^= (i & w) >> 4;
		i ^= p >> 8;
		i *= 0x0929eb3f;
		i ^= p >> 23;
		i ^= (i & w) >> 1;
		i *= 1 | p >> 27;
		i *= 0x6935fa69;
		i ^= (i & w) >> 11;
		i *= 0x74dcb303;
		i ^= (i & w) >> 2;
		i *= 0x9e501cc3;
		i ^= (i & w) >> 2;
		i *= 0xc860a3df;
		i &= w;
		i ^= i >> 5;

		return (i + p) & w;
	}
	else {
		/* l is not a power of two (slow) */
		w = cmj_w_mask(w);

		do {
			i ^= p;
			i *= 0xe170893d;
			i ^= p >> 16;
			i ^= (i & w) >> 4;
			i ^= p >> 8;
			i *= 0x0929eb3f;
			i ^= p >> 23;
			i ^= (i & w) >> 1;
			i *= 1 | p >> 27;
			i *= 0x6935fa69;
			i ^= (i & w) >> 11;
			i *= 0x74dcb303;
			i ^= (i & w) >> 2;
			i *= 0x9e501cc3;
			i ^= (i & w) >> 2;
			i *= 0xc860a3df;
			i &= w;
			i ^= i >> 5;
		} while (i >= l);

		return (i + p) % l;
	}
}

__device_inline uint cmj_hash(uint i, uint p)
{
	i ^= p;
	i ^= i >> 17;
	i ^= i >> 10;
	i *= 0xb36534e5;
	i ^= i >> 12;
	i ^= i >> 21;
	i *= 0x93fc4795;
	i ^= 0xdf6e307f;
	i ^= i >> 17;
	i *= 1 | p >> 18;

	return i;
}

__device_inline float cmj_randfloat(uint i, uint p)
{
	return cmj_hash(i, p) * (1.0f / 4294967808.0f);
}

#ifdef __CMJ__
__device float cmj_sample_1D(int s, int N, int p)
{
	uint x = cmj_permute(s, N, p * 0x68bc21eb);
	float jx = cmj_randfloat(s, p * 0x967a889b);

	float invN = 1.0f/N;
	return (x + jx)*invN;
}

__device void cmj_sample_2D(int s, int N, int p, float *fx, float *fy)
{
	int m = float_to_int(sqrtf(N));
	int n = (N + m - 1)/m;
	float invN = 1.0f/N;
	float invm = 1.0f/m;
	float invn = 1.0f/n;

	s = cmj_permute(s, N, p * 0x51633e2d);

	int sdivm, smodm;

	if(cmj_is_pow2(m)) {
		sdivm = cmj_fast_div_pow2(s, m);
		smodm = cmj_fast_mod_pow2(s, m);
	}
	else {
		sdivm = float_to_int(s * invm);
		smodm = s - sdivm*m;
	}

	uint sx = cmj_permute(smodm, m, p * 0x68bc21eb);
	uint sy = cmj_permute(sdivm, n, p * 0x02e5be93);

	float jx = cmj_randfloat(s, p * 0x967a889b);
	float jy = cmj_randfloat(s, p * 0x368cc8b7);

	*fx = (sx + (sy + jx)*invn)*invm;
	*fy = (s + jy)*invN;
}
#endif

CCL_NAMESPACE_END

