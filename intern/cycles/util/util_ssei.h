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
 * limitations under the License
 */

#ifndef __UTIL_SSEI_H__
#define __UTIL_SSEI_H__

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_SSE2__

/*! 4-wide SSE integer type. */
struct ssei
{
	typedef sseb Mask;                    // mask type
	typedef ssei Int;                     // int type
	typedef ssef Float;                   // float type

	enum   { size = 4 };                  // number of SIMD elements
	union  { __m128i m128; int32_t i[4]; }; // data

	////////////////////////////////////////////////////////////////////////////////
	/// Constructors, Assignment & Cast Operators
	////////////////////////////////////////////////////////////////////////////////
	
	__forceinline ssei           ( ) {}
	__forceinline ssei           ( const ssei& a ) { m128 = a.m128; }
	__forceinline ssei& operator=( const ssei& a ) { m128 = a.m128; return *this; }

	__forceinline ssei( const __m128i a ) : m128(a) {}
	__forceinline operator const __m128i&( void ) const { return m128; }
	__forceinline operator       __m128i&( void )       { return m128; }

	__forceinline ssei           ( const int a ) : m128(_mm_set1_epi32(a)) {}
	__forceinline ssei           ( int a, int b, int c, int d ) : m128(_mm_setr_epi32(a, b, c, d)) {}

	__forceinline explicit ssei( const __m128 a ) : m128(_mm_cvtps_epi32(a)) {}

	////////////////////////////////////////////////////////////////////////////////
	/// Array Access
	////////////////////////////////////////////////////////////////////////////////

	__forceinline const int32_t& operator []( const size_t index ) const { assert(index < 4); return i[index]; }
	__forceinline       int32_t& operator []( const size_t index )       { assert(index < 4); return i[index]; }
};

////////////////////////////////////////////////////////////////////////////////
/// Unary Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const ssei cast      ( const __m128& a ) { return _mm_castps_si128(a); }
__forceinline const ssei operator +( const ssei& a ) { return a; }
__forceinline const ssei operator -( const ssei& a ) { return _mm_sub_epi32(_mm_setzero_si128(), a.m128); }
#if defined(__KERNEL_SSSE3__)
__forceinline const ssei abs       ( const ssei& a ) { return _mm_abs_epi32(a.m128); }
#endif

////////////////////////////////////////////////////////////////////////////////
/// Binary Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const ssei operator +( const ssei& a, const ssei& b ) { return _mm_add_epi32(a.m128, b.m128); }
__forceinline const ssei operator +( const ssei& a, const int32_t&  b ) { return a + ssei(b); }
__forceinline const ssei operator +( const int32_t&  a, const ssei& b ) { return ssei(a) + b; }

__forceinline const ssei operator -( const ssei& a, const ssei& b ) { return _mm_sub_epi32(a.m128, b.m128); }
__forceinline const ssei operator -( const ssei& a, const int32_t&  b ) { return a - ssei(b); }
__forceinline const ssei operator -( const int32_t&  a, const ssei& b ) { return ssei(a) - b; }

#if defined(__KERNEL_SSE41__)
__forceinline const ssei operator *( const ssei& a, const ssei& b ) { return _mm_mullo_epi32(a.m128, b.m128); }
__forceinline const ssei operator *( const ssei& a, const int32_t&  b ) { return a * ssei(b); }
__forceinline const ssei operator *( const int32_t&  a, const ssei& b ) { return ssei(a) * b; }
#endif

__forceinline const ssei operator &( const ssei& a, const ssei& b ) { return _mm_and_si128(a.m128, b.m128); }
__forceinline const ssei operator &( const ssei& a, const int32_t&  b ) { return a & ssei(b); }
__forceinline const ssei operator &( const int32_t& a, const ssei& b ) { return ssei(a) & b; }

__forceinline const ssei operator |( const ssei& a, const ssei& b ) { return _mm_or_si128(a.m128, b.m128); }
__forceinline const ssei operator |( const ssei& a, const int32_t&  b ) { return a | ssei(b); }
__forceinline const ssei operator |( const int32_t& a, const ssei& b ) { return ssei(a) | b; }

__forceinline const ssei operator ^( const ssei& a, const ssei& b ) { return _mm_xor_si128(a.m128, b.m128); }
__forceinline const ssei operator ^( const ssei& a, const int32_t&  b ) { return a ^ ssei(b); }
__forceinline const ssei operator ^( const int32_t& a, const ssei& b ) { return ssei(a) ^ b; }

__forceinline const ssei operator <<( const ssei& a, const int32_t& n ) { return _mm_slli_epi32(a.m128, n); }
__forceinline const ssei operator >>( const ssei& a, const int32_t& n ) { return _mm_srai_epi32(a.m128, n); }

__forceinline const ssei andnot(const ssei& a, const ssei& b) { return _mm_andnot_si128(a.m128,b.m128); }
__forceinline const ssei andnot(const sseb& a, const ssei& b) { return _mm_andnot_si128(cast(a.m128),b.m128); }
__forceinline const ssei andnot(const ssei& a, const sseb& b) { return _mm_andnot_si128(a.m128,cast(b.m128)); }

__forceinline const ssei sra ( const ssei& a, const int32_t& b ) { return _mm_srai_epi32(a.m128, b); }
__forceinline const ssei srl ( const ssei& a, const int32_t& b ) { return _mm_srli_epi32(a.m128, b); }

#if defined(__KERNEL_SSE41__)
__forceinline const ssei min( const ssei& a, const ssei& b ) { return _mm_min_epi32(a.m128, b.m128); }
__forceinline const ssei min( const ssei& a, const int32_t&  b ) { return min(a,ssei(b)); }
__forceinline const ssei min( const int32_t&  a, const ssei& b ) { return min(ssei(a),b); }

__forceinline const ssei max( const ssei& a, const ssei& b ) { return _mm_max_epi32(a.m128, b.m128); }
__forceinline const ssei max( const ssei& a, const int32_t&  b ) { return max(a,ssei(b)); }
__forceinline const ssei max( const int32_t&  a, const ssei& b ) { return max(ssei(a),b); }
#endif

////////////////////////////////////////////////////////////////////////////////
/// Assignment Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline ssei& operator +=( ssei& a, const ssei& b ) { return a = a + b; }
__forceinline ssei& operator +=( ssei& a, const int32_t&  b ) { return a = a + b; }

__forceinline ssei& operator -=( ssei& a, const ssei& b ) { return a = a - b; }
__forceinline ssei& operator -=( ssei& a, const int32_t&  b ) { return a = a - b; }

#if defined(__KERNEL_SSE41__)
__forceinline ssei& operator *=( ssei& a, const ssei& b ) { return a = a * b; }
__forceinline ssei& operator *=( ssei& a, const int32_t&  b ) { return a = a * b; }
#endif

__forceinline ssei& operator &=( ssei& a, const ssei& b ) { return a = a & b; }
__forceinline ssei& operator &=( ssei& a, const int32_t&  b ) { return a = a & b; }

__forceinline ssei& operator |=( ssei& a, const ssei& b ) { return a = a | b; }
__forceinline ssei& operator |=( ssei& a, const int32_t&  b ) { return a = a | b; }

__forceinline ssei& operator <<=( ssei& a, const int32_t&  b ) { return a = a << b; }
__forceinline ssei& operator >>=( ssei& a, const int32_t&  b ) { return a = a >> b; }

////////////////////////////////////////////////////////////////////////////////
/// Comparison Operators + Select
////////////////////////////////////////////////////////////////////////////////

__forceinline const sseb operator ==( const ssei& a, const ssei& b ) { return _mm_castsi128_ps(_mm_cmpeq_epi32 (a.m128, b.m128)); }
__forceinline const sseb operator ==( const ssei& a, const int32_t& b ) { return a == ssei(b); }
__forceinline const sseb operator ==( const int32_t& a, const ssei& b ) { return ssei(a) == b; }

__forceinline const sseb operator !=( const ssei& a, const ssei& b ) { return !(a == b); }
__forceinline const sseb operator !=( const ssei& a, const int32_t& b ) { return a != ssei(b); }
__forceinline const sseb operator !=( const int32_t& a, const ssei& b ) { return ssei(a) != b; }

__forceinline const sseb operator < ( const ssei& a, const ssei& b ) { return _mm_castsi128_ps(_mm_cmplt_epi32 (a.m128, b.m128)); }
__forceinline const sseb operator < ( const ssei& a, const int32_t& b ) { return a <  ssei(b); }
__forceinline const sseb operator < ( const int32_t& a, const ssei& b ) { return ssei(a) <  b; }

__forceinline const sseb operator >=( const ssei& a, const ssei& b ) { return !(a <  b); }
__forceinline const sseb operator >=( const ssei& a, const int32_t& b ) { return a >= ssei(b); }
__forceinline const sseb operator >=( const int32_t& a, const ssei& b ) { return ssei(a) >= b; }

__forceinline const sseb operator > ( const ssei& a, const ssei& b ) { return _mm_castsi128_ps(_mm_cmpgt_epi32 (a.m128, b.m128)); }
__forceinline const sseb operator > ( const ssei& a, const int32_t& b ) { return a >  ssei(b); }
__forceinline const sseb operator > ( const int32_t& a, const ssei& b ) { return ssei(a) >  b; }

__forceinline const sseb operator <=( const ssei& a, const ssei& b ) { return !(a >  b); }
__forceinline const sseb operator <=( const ssei& a, const int32_t& b ) { return a <= ssei(b); }
__forceinline const sseb operator <=( const int32_t& a, const ssei& b ) { return ssei(a) <= b; }

__forceinline const ssei select( const sseb& m, const ssei& t, const ssei& f ) { 
#ifdef __KERNEL_SSE41__
	return _mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(f), _mm_castsi128_ps(t), m)); 
#else
	return _mm_or_si128(_mm_and_si128(m, t), _mm_andnot_si128(m, f)); 
#endif
}

__forceinline const ssei select( const int mask, const ssei& t, const ssei& f ) { 
#if defined(__KERNEL_SSE41__) && ((!defined(__clang__) && !defined(_MSC_VER)) || defined(__INTEL_COMPILER))
	return _mm_castps_si128(_mm_blend_ps(_mm_castsi128_ps(f), _mm_castsi128_ps(t), mask));
#else
	return select(sseb(mask),t,f);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// Movement/Shifting/Shuffling Functions
////////////////////////////////////////////////////////////////////////////////

__forceinline ssei unpacklo( const ssei& a, const ssei& b ) { return _mm_castps_si128(_mm_unpacklo_ps(_mm_castsi128_ps(a.m128), _mm_castsi128_ps(b.m128))); }
__forceinline ssei unpackhi( const ssei& a, const ssei& b ) { return _mm_castps_si128(_mm_unpackhi_ps(_mm_castsi128_ps(a.m128), _mm_castsi128_ps(b.m128))); }

template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const ssei shuffle( const ssei& a ) {
	return _mm_shuffle_epi32(a, _MM_SHUFFLE(i3, i2, i1, i0));
}

template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const ssei shuffle( const ssei& a, const ssei& b ) {
	return _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b), _MM_SHUFFLE(i3, i2, i1, i0)));
}

#if defined(__KERNEL_SSE3__)
template<> __forceinline const ssei shuffle<0, 0, 2, 2>( const ssei& a ) { return _mm_castps_si128(_mm_moveldup_ps(_mm_castsi128_ps(a))); }
template<> __forceinline const ssei shuffle<1, 1, 3, 3>( const ssei& a ) { return _mm_castps_si128(_mm_movehdup_ps(_mm_castsi128_ps(a))); }
template<> __forceinline const ssei shuffle<0, 1, 0, 1>( const ssei& a ) { return _mm_castpd_si128(_mm_movedup_pd (_mm_castsi128_pd(a))); }
#endif

template<size_t i0> __forceinline const ssei shuffle( const ssei& b ) {
	return shuffle<i0,i0,i0,i0>(b);
}

#if defined(__KERNEL_SSE41__)
template<size_t src> __forceinline int extract( const ssei& b ) { return _mm_extract_epi32(b, src); }
template<size_t dst> __forceinline const ssei insert( const ssei& a, const int32_t b ) { return _mm_insert_epi32(a, b, dst); }
#else
template<size_t src> __forceinline int extract( const ssei& b ) { return b[src]; }
template<size_t dst> __forceinline const ssei insert( const ssei& a, const int32_t b ) { ssei c = a; c[dst] = b; return c; }
#endif

////////////////////////////////////////////////////////////////////////////////
/// Reductions
////////////////////////////////////////////////////////////////////////////////

#if defined(__KERNEL_SSE41__)
__forceinline const ssei vreduce_min(const ssei& v) { ssei h = min(shuffle<1,0,3,2>(v),v); return min(shuffle<2,3,0,1>(h),h); }
__forceinline const ssei vreduce_max(const ssei& v) { ssei h = max(shuffle<1,0,3,2>(v),v); return max(shuffle<2,3,0,1>(h),h); }
__forceinline const ssei vreduce_add(const ssei& v) { ssei h = shuffle<1,0,3,2>(v)   + v ; return shuffle<2,3,0,1>(h)   + h ; }

__forceinline int reduce_min(const ssei& v) { return extract<0>(vreduce_min(v)); }
__forceinline int reduce_max(const ssei& v) { return extract<0>(vreduce_max(v)); }
__forceinline int reduce_add(const ssei& v) { return extract<0>(vreduce_add(v)); }

__forceinline size_t select_min(const ssei& v) { return __bsf(movemask(v == vreduce_min(v))); }
__forceinline size_t select_max(const ssei& v) { return __bsf(movemask(v == vreduce_max(v))); }

__forceinline size_t select_min(const sseb& valid, const ssei& v) { const ssei a = select(valid,v,ssei((int)pos_inf)); return __bsf(movemask(valid & (a == vreduce_min(a)))); }
__forceinline size_t select_max(const sseb& valid, const ssei& v) { const ssei a = select(valid,v,ssei((int)neg_inf)); return __bsf(movemask(valid & (a == vreduce_max(a)))); }

#else

__forceinline int reduce_min(const ssei& v) { return min(min(v[0],v[1]),min(v[2],v[3])); }
__forceinline int reduce_max(const ssei& v) { return max(max(v[0],v[1]),max(v[2],v[3])); }
__forceinline int reduce_add(const ssei& v) { return v[0]+v[1]+v[2]+v[3]; }

#endif

////////////////////////////////////////////////////////////////////////////////
/// Memory load and store operations
////////////////////////////////////////////////////////////////////////////////

__forceinline ssei load4i( const void* const a ) { 
	return _mm_load_si128((__m128i*)a); 
}

__forceinline void store4i(void* ptr, const ssei& v) {
	_mm_store_si128((__m128i*)ptr,v);
}

__forceinline void storeu4i(void* ptr, const ssei& v) {
	_mm_storeu_si128((__m128i*)ptr,v);
}

__forceinline void store4i( const sseb& mask, void* ptr, const ssei& i ) { 
#if defined (__KERNEL_AVX__)
	_mm_maskstore_ps((float*)ptr,(__m128i)mask,_mm_castsi128_ps(i));
#else
	*(ssei*)ptr = select(mask,i,*(ssei*)ptr);
#endif
}

__forceinline ssei load4i_nt (void* ptr) { 
#if defined(__KERNEL_SSE41__)
	return _mm_stream_load_si128((__m128i*)ptr); 
#else
	return _mm_load_si128((__m128i*)ptr); 
#endif
}

__forceinline void store4i_nt(void* ptr, const ssei& v) { 
#if defined(__KERNEL_SSE41__)
	_mm_stream_ps((float*)ptr,_mm_castsi128_ps(v)); 
#else
	_mm_store_si128((__m128i*)ptr,v);
#endif
}

#endif

CCL_NAMESPACE_END

#endif

