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

	//__forceinline avxb           ( bool  a )
	//	: m256(_mm_lookupmask_ps[(size_t(a) << 3) | (size_t(a) << 2) | (size_t(a) << 1) | size_t(a)]) {}
	//__forceinline avxb           ( bool  a, bool  b)
	//	: m256(_mm_lookupmask_ps[(size_t(b) << 3) | (size_t(a) << 2) | (size_t(b) << 1) | size_t(a)]) {}
	//__forceinline avxb           ( bool  a, bool  b, bool  c, bool  d)
	//	: m256(_mm_lookupmask_ps[(size_t(d) << 3) | (size_t(c) << 2) | (size_t(b) << 1) | size_t(a)]) {}
	//__forceinline avxb(int mask) {
	//	assert(mask >= 0 && mask < 16);
	//	m128 = _mm_lookupmask_ps[mask];
	//}

	////////////////////////////////////////////////////////////////////////////////
	/// Constants
	////////////////////////////////////////////////////////////////////////////////

	__forceinline avxb( FalseTy ) : m256(_mm256_setzero_ps()) {}
	__forceinline avxb( TrueTy  ) : m256(_mm256_castsi256_ps(_mm256_cmpeq_epi32(_mm256_setzero_si256(), _mm256_setzero_si256()))) {}

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
__forceinline const avxb operator ==( const avxb& a, const avxb& b ) { return _mm256_castsi256_ps(_mm256_cmpeq_epi32(a, b)); }

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

#define _MM256_SHUFFLE(fp7,fp6,fp5,fp4,fp3,fp2,fp1,fp0) (((fp7) << 14) | ((fp6) << 12) | ((fp5) << 10) | ((fp4) << 8) | \
                                                      ((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

template<size_t i0, size_t i1, size_t i2, size_t i3, size_t i4, size_t i5, size_t i6, size_t i7>
__forceinline const avxb shuffle( const avxb& a ) {
	return _mm256_cvtepi32_ps(_mm256_shuffle_epi32(a, _MM256_SHUFFLE(i7, i6, i5, i4, i3, i2, i1, i0)));
}

/*
template<> __forceinline const avxb shuffle<0, 1, 0, 1, 0, 1, 0, 1>( const avxb& a ) {
	return _mm_movelh_ps(a, a);
}

template<> __forceinline const sseb shuffle<2, 3, 2, 3>( const sseb& a ) {
	return _mm_movehl_ps(a, a);
}

template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const sseb shuffle( const sseb& a, const sseb& b ) {
	return _mm_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0));
}

template<> __forceinline const sseb shuffle<0, 1, 0, 1>( const sseb& a, const sseb& b ) {
	return _mm_movelh_ps(a, b);
}

template<> __forceinline const sseb shuffle<2, 3, 2, 3>( const sseb& a, const sseb& b ) {
	return _mm_movehl_ps(b, a);
}

#if defined(__KERNEL_SSE3__)
template<> __forceinline const sseb shuffle<0, 0, 2, 2>( const sseb& a ) { return _mm_moveldup_ps(a); }
template<> __forceinline const sseb shuffle<1, 1, 3, 3>( const sseb& a ) { return _mm_movehdup_ps(a); }
#endif

#if defined(__KERNEL_SSE41__)
template<size_t dst, size_t src, size_t clr> __forceinline const sseb insert( const sseb& a, const sseb& b ) { return _mm_insert_ps(a, b, (dst << 4) | (src << 6) | clr); }
template<size_t dst, size_t src> __forceinline const sseb insert( const sseb& a, const sseb& b ) { return insert<dst, src, 0>(a, b); }
template<size_t dst>             __forceinline const sseb insert( const sseb& a, const bool b ) { return insert<dst,0>(a, sseb(b)); }
#endif
*/

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
	printf("%s: %df %df %df %df %df %df %df %d\n",
	       label, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
}

#endif

CCL_NAMESPACE_END

//#endif
