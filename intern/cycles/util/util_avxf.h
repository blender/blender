/*
 * Copyright 2016 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0(the "License");
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

#ifndef __UTIL_AVXF_H__
#define __UTIL_AVXF_H__

CCL_NAMESPACE_BEGIN

struct avxb;

struct avxf
{
	typedef avxf Float;

	enum { size = 8 };  /* Number of SIMD elements. */

	union {
		__m256 m256;
		float f[8];
		int i[8];
	};

	__forceinline avxf           () {}
	__forceinline avxf           (const avxf& other) { m256 = other.m256; }
	__forceinline avxf& operator=(const avxf& other) { m256 = other.m256; return *this; }

	__forceinline avxf(const __m256 a) : m256(a) {}
	__forceinline avxf(const __m256i a) : m256(_mm256_castsi256_ps (a)) {}

	__forceinline operator const __m256&(void) const { return m256; }
	__forceinline operator       __m256&(void)       { return m256; }

	__forceinline avxf          (float a) : m256(_mm256_set1_ps(a)) {}

	__forceinline avxf(float high32x4, float low32x4) :
	   m256(_mm256_set_ps(high32x4, high32x4, high32x4, high32x4, low32x4, low32x4, low32x4, low32x4)) {}

	__forceinline avxf(float a3, float a2, float a1, float a0) :
	   m256(_mm256_set_ps(a3, a2, a1, a0, a3, a2, a1, a0)) {}

	__forceinline avxf(float a7, float a6, float a5, float a4, float a3, float a2, float a1, float a0) :
		m256(_mm256_set_ps(a7, a6, a5, a4, a3, a2, a1, a0)) {}

	__forceinline avxf(float3 a) :
		m256(_mm256_set_ps(a.w, a.z, a.y, a.x, a.w, a.z, a.y, a.x)) {}


	__forceinline avxf(int a3, int a2, int a1, int a0)
	{
		const __m256i foo = _mm256_set_epi32(a3, a2, a1, a0, a3, a2, a1, a0);
		m256 = _mm256_castsi256_ps(foo);
	}


	__forceinline avxf(int a7, int a6, int a5, int a4, int a3, int a2, int a1, int a0)
	{
		const __m256i foo = _mm256_set_epi32(a7, a6, a5, a4, a3, a2, a1, a0);
		m256 = _mm256_castsi256_ps(foo);
	}

	__forceinline avxf(__m128 a, __m128 b)
	{
		const __m256 foo = _mm256_castps128_ps256(a);
		m256 = _mm256_insertf128_ps(foo, b, 1);
	}

	__forceinline const float& operator [](const size_t i) const { assert(i < 8); return f[i]; }
	__forceinline       float& operator [](const size_t i) { assert(i < 8); return f[i]; }
};

__forceinline avxf cross(const avxf& a, const avxf& b)
{
	avxf r(0.0, a[4]*b[5] - a[5]*b[4], a[6]*b[4] - a[4]*b[6], a[5]*b[6] - a[6]*b[5],
		   0.0, a[0]*b[1] - a[1]*b[0], a[2]*b[0] - a[0]*b[2], a[1]*b[2] - a[2]*b[1]);
	return r;
}

__forceinline void dot3(const avxf& a, const avxf& b, float &den, float &den2)
{
	const avxf t = _mm256_mul_ps(a.m256, b.m256);
	den = ((float*)&t)[0] + ((float*)&t)[1] + ((float*)&t)[2];
	den2 = ((float*)&t)[4] + ((float*)&t)[5] + ((float*)&t)[6];
}

////////////////////////////////////////////////////////////////////////////////
/// Unary Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const avxf mm256_sqrt(const avxf& a) { return _mm256_sqrt_ps(a.m256); }

////////////////////////////////////////////////////////////////////////////////
/// Binary Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const avxf operator +(const avxf& a, const avxf& b) { return _mm256_add_ps(a.m256, b.m256); }
__forceinline const avxf operator +(const avxf& a, const float& b) { return a + avxf(b); }
__forceinline const avxf operator +(const float& a, const avxf& b) { return avxf(a) + b; }

__forceinline const avxf operator -(const avxf& a, const avxf& b) { return _mm256_sub_ps(a.m256, b.m256); }
__forceinline const avxf operator -(const avxf& a, const float& b) { return a - avxf(b); }
__forceinline const avxf operator -(const float& a, const avxf& b) { return avxf(a) - b; }

__forceinline const avxf operator *(const avxf& a, const avxf& b) { return _mm256_mul_ps(a.m256, b.m256); }
__forceinline const avxf operator *(const avxf& a, const float& b) { return a * avxf(b); }
__forceinline const avxf operator *(const float& a, const avxf& b) { return avxf(a) * b; }

__forceinline const avxf operator /(const avxf& a, const avxf& b) { return _mm256_div_ps(a.m256,b.m256); }
__forceinline const avxf operator /(const avxf& a, const float& b) { return a/avxf(b); }
__forceinline const avxf operator /(const float& a, const avxf& b) { return avxf(a)/b; }

__forceinline const avxf operator|(const avxf& a, const avxf& b) { return _mm256_or_ps(a.m256,b.m256); }

__forceinline const avxf operator^(const avxf& a, const avxf& b) { return _mm256_xor_ps(a.m256,b.m256); }

__forceinline const avxf operator&(const avxf& a, const avxf& b) { return _mm256_and_ps(a.m256,b.m256); }

__forceinline const avxf max(const avxf& a, const avxf& b) { return _mm256_max_ps(a.m256, b.m256); }
__forceinline const avxf min(const avxf& a, const avxf& b) { return _mm256_min_ps(a.m256, b.m256); }

////////////////////////////////////////////////////////////////////////////////
/// Movement/Shifting/Shuffling Functions
////////////////////////////////////////////////////////////////////////////////

__forceinline const avxf shuffle(const avxf& a, const __m256i &shuf) {
	return _mm256_permutevar_ps(a, shuf);
}

template<int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7> __forceinline const avxf shuffle(const avxf& a) {
	return _mm256_permutevar_ps(a, _mm256_set_epi32( i7,i6,i5,i4 ,i3,i2,i1,i0));
}

template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const avxf shuffle(const avxf& a, const avxf& b) {
	return _mm256_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0));
}
template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const avxf shuffle(const avxf& a) {
	return shuffle<i0,i1,i2,i3>(a,a);
}
template<size_t i0> __forceinline const avxf shuffle(const avxf& a, const avxf& b) {
	return shuffle<i0,i0,i0,i0>(a, b);
}
template<size_t i0> __forceinline const avxf shuffle(const avxf& a) {
	return shuffle<i0>(a,a);
}

template<int i0, int i1, int i2, int i3, int i4, int i5, int i6, int i7> __forceinline const avxf permute(const avxf& a) {
#ifdef __KERNEL_AVX2__
	return  _mm256_permutevar8x32_ps(a,_mm256_set_epi32( i7,i6,i5,i4 ,i3,i2,i1,i0));
#else
	float temp[8];
	_mm256_storeu_ps((float*)&temp, a);
	return avxf(temp[i7], temp[i6], temp[i5], temp[i4], temp[i3], temp[i2], temp[i1], temp[i0]);
#endif
}

template<int S0, int S1, int S2, int S3,int S4,int S5,int S6, int S7>
ccl_device_inline const avxf set_sign_bit(const avxf &a)
{
	return a ^ avxf(S7 << 31, S6 << 31, S5 << 31, S4 << 31, S3 << 31,S2 << 31,S1 << 31,S0 << 31);
}

template<size_t S0, size_t S1, size_t S2, size_t S3,size_t S4,size_t S5,size_t S6, size_t S7>
ccl_device_inline const avxf blend(const avxf &a, const avxf &b)
{
	return _mm256_blend_ps(a,b,S7 << 0 | S6 << 1 | S5 << 2 | S4 << 3 | S3 << 4 | S2 << 5 | S1 << 6 | S0 << 7);
}

template<size_t S0, size_t S1, size_t S2, size_t S3 >
ccl_device_inline const avxf blend(const avxf &a, const avxf &b)
{
	return blend<S0,S1,S2,S3,S0,S1,S2,S3>(a,b);
}

//#if defined(__KERNEL_SSE41__)
__forceinline avxf maxi(const avxf& a, const avxf& b) {
	const avxf ci = _mm256_max_ps(a, b);
	return ci;
}

__forceinline avxf mini(const avxf& a, const avxf& b) {
	const avxf ci = _mm256_min_ps(a, b);
	return ci;
}
//#endif

////////////////////////////////////////////////////////////////////////////////
/// Ternary Operators
////////////////////////////////////////////////////////////////////////////////
__forceinline const avxf madd (const avxf& a, const avxf& b, const avxf& c) {
#ifdef __KERNEL_AVX2__
	return _mm256_fmadd_ps(a,b,c);
#else
	return c+(a*b);
#endif
}

__forceinline const avxf nmadd(const avxf& a, const avxf& b, const avxf& c) {
#ifdef __KERNEL_AVX2__
	return _mm256_fnmadd_ps(a, b, c);
#else
	return c-(a*b);
#endif
}
__forceinline const avxf msub(const avxf& a, const avxf& b, const avxf& c) {
#ifdef __KERNEL_AVX2__
	return _mm256_fmsub_ps(a, b, c);
#else
	return (a*b) - c;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// Comparison Operators
////////////////////////////////////////////////////////////////////////////////
__forceinline const avxb operator <=(const avxf& a, const avxf& b) {
	return _mm256_cmp_ps(a.m256, b.m256, _CMP_LE_OS);
}

#endif

#ifndef _mm256_set_m128
#  define _mm256_set_m128(/* __m128 */ hi, /* __m128 */ lo) \
    _mm256_insertf128_ps(_mm256_castps128_ps256(lo), (hi), 0x1)
#endif

#define _mm256_loadu2_m128(/* float const* */ hiaddr, /* float const* */ loaddr) \
    _mm256_set_m128(_mm_loadu_ps(hiaddr), _mm_loadu_ps(loaddr))

CCL_NAMESPACE_END
