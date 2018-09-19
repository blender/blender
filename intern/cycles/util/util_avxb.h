/*
 * Copyright 2011-2013 Intel Corporation
 * Modifications Copyright 2014, Blender Foundation.
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

#ifndef __UTIL_AVXB_H__
#define __UTIL_AVXB_H__

CCL_NAMESPACE_BEGIN

struct avxf;

/*! 4-wide SSE bool type. */
struct avxb
{
	typedef avxb Mask;                    // mask type
	typedef avxf Float;                   // float type

	enum   { size = 8 };                  // number of SIMD elements
	union  { __m256 m256; int32_t v[8]; };  // data

	////////////////////////////////////////////////////////////////////////////////
	/// Constructors, Assignment & Cast Operators
	////////////////////////////////////////////////////////////////////////////////

	__forceinline avxb           ( ) {}
	__forceinline avxb           ( const avxb& other ) { m256 = other.m256; }
	__forceinline avxb& operator=( const avxb& other ) { m256 = other.m256; return *this; }

	__forceinline avxb( const __m256  input ) : m256(input) {}
	__forceinline operator const __m256&( void ) const { return m256; }
	__forceinline operator const __m256i( void ) const { return _mm256_castps_si256(m256); }
	__forceinline operator const __m256d( void ) const { return _mm256_castps_pd(m256); }

	////////////////////////////////////////////////////////////////////////////////
	/// Constants
	////////////////////////////////////////////////////////////////////////////////

	__forceinline avxb( FalseTy ) : m256(_mm256_setzero_ps()) {}
	__forceinline avxb( TrueTy  ) : m256(_mm256_castsi256_ps(_mm256_set1_epi32(-1))) {}

	////////////////////////////////////////////////////////////////////////////////
	/// Array Access
	////////////////////////////////////////////////////////////////////////////////

	__forceinline bool   operator []( const size_t i ) const { assert(i < 8); return (_mm256_movemask_ps(m256) >> i) & 1; }
	__forceinline int32_t& operator []( const size_t i )       { assert(i < 8); return v[i]; }
};

////////////////////////////////////////////////////////////////////////////////
/// Unary Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const avxb operator !( const avxb& a ) { return _mm256_xor_ps(a, avxb(True)); }

////////////////////////////////////////////////////////////////////////////////
/// Binary Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const avxb operator &( const avxb& a, const avxb& b ) { return _mm256_and_ps(a, b); }
__forceinline const avxb operator |( const avxb& a, const avxb& b ) { return _mm256_or_ps (a, b); }
__forceinline const avxb operator ^( const avxb& a, const avxb& b ) { return _mm256_xor_ps(a, b); }

////////////////////////////////////////////////////////////////////////////////
/// Assignment Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const avxb operator &=( avxb& a, const avxb& b ) { return a = a & b; }
__forceinline const avxb operator |=( avxb& a, const avxb& b ) { return a = a | b; }
__forceinline const avxb operator ^=( avxb& a, const avxb& b ) { return a = a ^ b; }

////////////////////////////////////////////////////////////////////////////////
/// Comparison Operators + Select
////////////////////////////////////////////////////////////////////////////////

__forceinline const avxb operator !=( const avxb& a, const avxb& b ) { return _mm256_xor_ps(a, b); }
__forceinline const avxb operator ==( const avxb& a, const avxb& b )
{
#ifdef __KERNEL_AVX2__
	return _mm256_castsi256_ps(_mm256_cmpeq_epi32(a, b));
#else
	__m128i a_lo = _mm_castps_si128(_mm256_extractf128_ps(a, 0));
	__m128i a_hi = _mm_castps_si128(_mm256_extractf128_ps(a, 1));
	__m128i b_lo = _mm_castps_si128(_mm256_extractf128_ps(b, 0));
	__m128i b_hi = _mm_castps_si128(_mm256_extractf128_ps(b, 1));
	__m128i c_lo = _mm_cmpeq_epi32(a_lo, b_lo);
	__m128i c_hi = _mm_cmpeq_epi32(a_hi, b_hi);
	__m256i result = _mm256_insertf128_si256(_mm256_castsi128_si256(c_lo), c_hi, 1);
	return _mm256_castsi256_ps(result);
#endif
}

__forceinline const avxb select( const avxb& m, const avxb& t, const avxb& f ) {
#if defined(__KERNEL_SSE41__)
	return _mm256_blendv_ps(f, t, m);
#else
	return _mm256_or_ps(_mm256_and_ps(m, t), _mm256_andnot_ps(m, f));
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// Movement/Shifting/Shuffling Functions
////////////////////////////////////////////////////////////////////////////////

__forceinline const avxb unpacklo( const avxb& a, const avxb& b ) { return _mm256_unpacklo_ps(a, b); }
__forceinline const avxb unpackhi( const avxb& a, const avxb& b ) { return _mm256_unpackhi_ps(a, b); }

////////////////////////////////////////////////////////////////////////////////
/// Reduction Operations
////////////////////////////////////////////////////////////////////////////////

#if defined(__KERNEL_SSE41__)
__forceinline size_t popcnt( const avxb& a ) { return __popcnt(_mm256_movemask_ps(a)); }
#else
__forceinline size_t popcnt( const avxb& a ) { return bool(a[0])+bool(a[1])+bool(a[2])+bool(a[3])+bool(a[4])+
                                                      bool(a[5])+bool(a[6])+bool(a[7]); }
#endif

__forceinline bool reduce_and( const avxb& a ) { return _mm256_movemask_ps(a) == 0xf; }
__forceinline bool reduce_or ( const avxb& a ) { return _mm256_movemask_ps(a) != 0x0; }
__forceinline bool all       ( const avxb& b ) { return _mm256_movemask_ps(b) == 0xf; }
__forceinline bool any       ( const avxb& b ) { return _mm256_movemask_ps(b) != 0x0; }
__forceinline bool none      ( const avxb& b ) { return _mm256_movemask_ps(b) == 0x0; }

__forceinline size_t movemask( const avxb& a ) { return _mm256_movemask_ps(a); }

////////////////////////////////////////////////////////////////////////////////
/// Debug Functions
////////////////////////////////////////////////////////////////////////////////

ccl_device_inline void print_avxb(const char *label, const avxb &a)
{
	printf("%s: %d %d %d %d %d %d %d %d\n",
	       label, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
}

#endif

CCL_NAMESPACE_END

//#endif
