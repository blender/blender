/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* TODO(sergey): Consider moving portable ctz/clz stuff to util. */

CCL_NAMESPACE_BEGIN

/* "Correlated Multi-Jittered Sampling"
 * Andrew Kensler, Pixar Technical Memo 13-01, 2013 */

/* todo: find good value, suggested 64 gives pattern on cornell box ceiling */
#define CMJ_RANDOM_OFFSET_LIMIT 4096

ccl_device_inline bool cmj_is_pow2(int i)
{
	return (i > 1) && ((i & (i - 1)) == 0);
}

ccl_device_inline int cmj_fast_mod_pow2(int a, int b)
{
	return (a & (b - 1));
}

/* b must be > 1 */
ccl_device_inline int cmj_fast_div_pow2(int a, int b)
{
	kernel_assert(b > 1);
#if defined(__KERNEL_SSE2__)
#  ifdef _MSC_VER
	unsigned long ctz;
	_BitScanForward(&ctz, b);
	return a >> ctz;
#  else
	return a >> __builtin_ctz(b);
#  endif
#elif defined(__KERNEL_CUDA__)
	return a >> (__ffs(b) - 1);
#else
	return a/b;
#endif
}

ccl_device_inline uint cmj_w_mask(uint w)
{
	kernel_assert(w > 1);
#if defined(__KERNEL_SSE2__)
#  ifdef _MSC_VER
	unsigned long leading_zero;
	_BitScanReverse(&leading_zero, w);
	return ((1 << (1 + leading_zero)) - 1);
#  else
	return ((1 << (32 - __builtin_clz(w))) - 1);
#  endif
#elif defined(__KERNEL_CUDA__)
	return ((1 << (32 - __clz(w))) - 1);
#else
	w |= w >> 1;
	w |= w >> 2;
	w |= w >> 4;
	w |= w >> 8;
	w |= w >> 16;

	return w;
#endif
}

ccl_device_inline uint cmj_permute(uint i, uint l, uint p)
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
		} while(i >= l);

		return (i + p) % l;
	}
}

ccl_device_inline uint cmj_hash(uint i, uint p)
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

ccl_device_inline uint cmj_hash_simple(uint i, uint p)
{
	i = (i ^ 61) ^ p;
	i += i << 3;
	i ^= i >> 4;
	i *= 0x27d4eb2d;
	return i;
}

ccl_device_inline float cmj_randfloat(uint i, uint p)
{
	return cmj_hash(i, p) * (1.0f / 4294967808.0f);
}

#ifdef __CMJ__
ccl_device float cmj_sample_1D(int s, int N, int p)
{
	kernel_assert(s < N);

	uint x = cmj_permute(s, N, p * 0x68bc21eb);
	float jx = cmj_randfloat(s, p * 0x967a889b);

	float invN = 1.0f/N;
	return (x + jx)*invN;
}

/* TODO(sergey): Do some extra tests and consider moving to util_math.h. */
ccl_device_inline int cmj_isqrt(int value)
{
#if defined(__KERNEL_CUDA__)
	return float_to_int(__fsqrt_ru(value));
#elif defined(__KERNEL_GPU__)
	return float_to_int(sqrtf(value));
#else
	/* This is a work around for fast-math on CPU which might replace sqrtf()
	 * with am approximated version.
	 */
	return float_to_int(sqrtf(value) + 1e-6f);
#endif
}

ccl_device void cmj_sample_2D(int s, int N, int p, float *fx, float *fy)
{
	kernel_assert(s < N);

	int m = cmj_isqrt(N);
	int n = (N - 1)/m + 1;
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
		/* Doing s*inmv gives precision issues here. */
		sdivm = s / m;
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
