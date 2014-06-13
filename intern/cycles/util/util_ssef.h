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

#ifndef __UTIL_SSEF_H__
#define __UTIL_SSEF_H__

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_SSE2__

/*! 4-wide SSE float type. */
struct ssef
{
	typedef sseb Mask;                    // mask type
	typedef ssei Int;                     // int type
	typedef ssef Float;                   // float type
	
	enum   { size = 4 };  // number of SIMD elements
	union { __m128 m128; float f[4]; int i[4]; }; // data

	////////////////////////////////////////////////////////////////////////////////
	/// Constructors, Assignment & Cast Operators
	////////////////////////////////////////////////////////////////////////////////
	
	__forceinline ssef          () {}
	__forceinline ssef          (const ssef& other) { m128 = other.m128; }
	__forceinline ssef& operator=(const ssef& other) { m128 = other.m128; return *this; }

	__forceinline ssef(const __m128 a) : m128(a) {}
	__forceinline operator const __m128&(void) const { return m128; }
	__forceinline operator       __m128&(void)       { return m128; }

	__forceinline ssef          (float a) : m128(_mm_set1_ps(a)) {}
	__forceinline ssef          (float a, float b, float c, float d) : m128(_mm_setr_ps(a, b, c, d)) {}

	__forceinline explicit ssef(const __m128i a) : m128(_mm_cvtepi32_ps(a)) {}

	////////////////////////////////////////////////////////////////////////////////
	/// Loads and Stores
	////////////////////////////////////////////////////////////////////////////////

#if defined(__KERNEL_AVX__)
	static __forceinline ssef broadcast(const void* const a) { return _mm_broadcast_ss((float*)a); }
#else
	static __forceinline ssef broadcast(const void* const a) { return _mm_set1_ps(*(float*)a); }
#endif

	////////////////////////////////////////////////////////////////////////////////
	/// Array Access
	////////////////////////////////////////////////////////////////////////////////

	__forceinline const float& operator [](const size_t i) const { assert(i < 4); return f[i]; }
	__forceinline       float& operator [](const size_t i)       { assert(i < 4); return f[i]; }
};


////////////////////////////////////////////////////////////////////////////////
/// Unary Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const ssef cast     (const __m128i& a) { return _mm_castsi128_ps(a); }
__forceinline const ssef operator +(const ssef& a) { return a; }
__forceinline const ssef operator -(const ssef& a) { return _mm_xor_ps(a.m128, _mm_castsi128_ps(_mm_set1_epi32(0x80000000))); }
__forceinline const ssef abs      (const ssef& a) { return _mm_and_ps(a.m128, _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff))); }
#if defined(__KERNEL_SSE41__)
__forceinline const ssef sign     (const ssef& a) { return _mm_blendv_ps(ssef(1.0f), -ssef(1.0f), _mm_cmplt_ps(a,ssef(0.0f))); }
#endif
__forceinline const ssef signmsk  (const ssef& a) { return _mm_and_ps(a.m128,_mm_castsi128_ps(_mm_set1_epi32(0x80000000))); }

__forceinline const ssef rcp (const ssef& a) {
	const ssef r = _mm_rcp_ps(a.m128);
	return _mm_sub_ps(_mm_add_ps(r, r), _mm_mul_ps(_mm_mul_ps(r, r), a));
}
__forceinline const ssef sqr (const ssef& a) { return _mm_mul_ps(a,a); }
__forceinline const ssef mm_sqrt(const ssef& a) { return _mm_sqrt_ps(a.m128); }
__forceinline const ssef rsqrt(const ssef& a) {
	const ssef r = _mm_rsqrt_ps(a.m128);
	return _mm_add_ps(_mm_mul_ps(_mm_set_ps(1.5f, 1.5f, 1.5f, 1.5f), r),
					  _mm_mul_ps(_mm_mul_ps(_mm_mul_ps(a, _mm_set_ps(-0.5f, -0.5f, -0.5f, -0.5f)), r), _mm_mul_ps(r, r)));
}

////////////////////////////////////////////////////////////////////////////////
/// Binary Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline const ssef operator +(const ssef& a, const ssef& b) { return _mm_add_ps(a.m128, b.m128); }
__forceinline const ssef operator +(const ssef& a, const float& b) { return a + ssef(b); }
__forceinline const ssef operator +(const float& a, const ssef& b) { return ssef(a) + b; }

__forceinline const ssef operator -(const ssef& a, const ssef& b) { return _mm_sub_ps(a.m128, b.m128); }
__forceinline const ssef operator -(const ssef& a, const float& b) { return a - ssef(b); }
__forceinline const ssef operator -(const float& a, const ssef& b) { return ssef(a) - b; }

__forceinline const ssef operator *(const ssef& a, const ssef& b) { return _mm_mul_ps(a.m128, b.m128); }
__forceinline const ssef operator *(const ssef& a, const float& b) { return a * ssef(b); }
__forceinline const ssef operator *(const float& a, const ssef& b) { return ssef(a) * b; }

__forceinline const ssef operator /(const ssef& a, const ssef& b) { return _mm_div_ps(a.m128,b.m128); }
__forceinline const ssef operator /(const ssef& a, const float& b) { return a/ssef(b); }
__forceinline const ssef operator /(const float& a, const ssef& b) { return ssef(a)/b; }

__forceinline const ssef operator^(const ssef& a, const ssef& b) { return _mm_xor_ps(a.m128,b.m128); }
__forceinline const ssef operator^(const ssef& a, const ssei& b) { return _mm_xor_ps(a.m128,_mm_castsi128_ps(b.m128)); }

__forceinline const ssef operator&(const ssef& a, const ssef& b) { return _mm_and_ps(a.m128,b.m128); }
__forceinline const ssef operator&(const ssef& a, const ssei& b) { return _mm_and_ps(a.m128,_mm_castsi128_ps(b.m128)); }

__forceinline const ssef andnot(const ssef& a, const ssef& b) { return _mm_andnot_ps(a.m128,b.m128); }

__forceinline const ssef min(const ssef& a, const ssef& b) { return _mm_min_ps(a.m128,b.m128); }
__forceinline const ssef min(const ssef& a, const float& b) { return _mm_min_ps(a.m128,ssef(b)); }
__forceinline const ssef min(const float& a, const ssef& b) { return _mm_min_ps(ssef(a),b.m128); }

__forceinline const ssef max(const ssef& a, const ssef& b) { return _mm_max_ps(a.m128,b.m128); }
__forceinline const ssef max(const ssef& a, const float& b) { return _mm_max_ps(a.m128,ssef(b)); }
__forceinline const ssef max(const float& a, const ssef& b) { return _mm_max_ps(ssef(a),b.m128); }

#if defined(__KERNEL_SSE41__)
__forceinline ssef mini(const ssef& a, const ssef& b) {
	const ssei ai = _mm_castps_si128(a);
	const ssei bi = _mm_castps_si128(b);
	const ssei ci = _mm_min_epi32(ai,bi);
	return _mm_castsi128_ps(ci);
}
#endif
	
#if defined(__KERNEL_SSE41__)
__forceinline ssef maxi(const ssef& a, const ssef& b) {
	const ssei ai = _mm_castps_si128(a);
	const ssei bi = _mm_castps_si128(b);
	const ssei ci = _mm_max_epi32(ai,bi);
	return _mm_castsi128_ps(ci);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// Ternary Operators
////////////////////////////////////////////////////////////////////////////////

#if defined(__KERNEL_AVX2__)
__forceinline const ssef madd (const ssef& a, const ssef& b, const ssef& c) { return _mm_fmadd_ps(a,b,c); }
__forceinline const ssef msub (const ssef& a, const ssef& b, const ssef& c) { return _mm_fmsub_ps(a,b,c); }
__forceinline const ssef nmadd(const ssef& a, const ssef& b, const ssef& c) { return _mm_fnmadd_ps(a,b,c); }
__forceinline const ssef nmsub(const ssef& a, const ssef& b, const ssef& c) { return _mm_fnmsub_ps(a,b,c); }
#else
__forceinline const ssef madd (const ssef& a, const ssef& b, const ssef& c) { return a*b+c; }
__forceinline const ssef msub (const ssef& a, const ssef& b, const ssef& c) { return a*b-c; }
__forceinline const ssef nmadd(const ssef& a, const ssef& b, const ssef& c) { return -a*b-c;}
__forceinline const ssef nmsub(const ssef& a, const ssef& b, const ssef& c) { return c-a*b; }
#endif

////////////////////////////////////////////////////////////////////////////////
/// Assignment Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline ssef& operator +=(ssef& a, const ssef& b) { return a = a + b; }
__forceinline ssef& operator +=(ssef& a, const float& b) { return a = a + b; }

__forceinline ssef& operator -=(ssef& a, const ssef& b) { return a = a - b; }
__forceinline ssef& operator -=(ssef& a, const float& b) { return a = a - b; }

__forceinline ssef& operator *=(ssef& a, const ssef& b) { return a = a * b; }
__forceinline ssef& operator *=(ssef& a, const float& b) { return a = a * b; }

__forceinline ssef& operator /=(ssef& a, const ssef& b) { return a = a / b; }
__forceinline ssef& operator /=(ssef& a, const float& b) { return a = a / b; }

////////////////////////////////////////////////////////////////////////////////
/// Comparison Operators + Select
////////////////////////////////////////////////////////////////////////////////

__forceinline const sseb operator ==(const ssef& a, const ssef& b) { return _mm_cmpeq_ps(a.m128, b.m128); }
__forceinline const sseb operator ==(const ssef& a, const float& b) { return a == ssef(b); }
__forceinline const sseb operator ==(const float& a, const ssef& b) { return ssef(a) == b; }

__forceinline const sseb operator !=(const ssef& a, const ssef& b) { return _mm_cmpneq_ps(a.m128, b.m128); }
__forceinline const sseb operator !=(const ssef& a, const float& b) { return a != ssef(b); }
__forceinline const sseb operator !=(const float& a, const ssef& b) { return ssef(a) != b; }

__forceinline const sseb operator <(const ssef& a, const ssef& b) { return _mm_cmplt_ps(a.m128, b.m128); }
__forceinline const sseb operator <(const ssef& a, const float& b) { return a <  ssef(b); }
__forceinline const sseb operator <(const float& a, const ssef& b) { return ssef(a) <  b; }

__forceinline const sseb operator >=(const ssef& a, const ssef& b) { return _mm_cmpnlt_ps(a.m128, b.m128); }
__forceinline const sseb operator >=(const ssef& a, const float& b) { return a >= ssef(b); }
__forceinline const sseb operator >=(const float& a, const ssef& b) { return ssef(a) >= b; }

__forceinline const sseb operator >(const ssef& a, const ssef& b) { return _mm_cmpnle_ps(a.m128, b.m128); }
__forceinline const sseb operator >(const ssef& a, const float& b) { return a >  ssef(b); }
__forceinline const sseb operator >(const float& a, const ssef& b) { return ssef(a) >  b; }

__forceinline const sseb operator <=(const ssef& a, const ssef& b) { return _mm_cmple_ps(a.m128, b.m128); }
__forceinline const sseb operator <=(const ssef& a, const float& b) { return a <= ssef(b); }
__forceinline const sseb operator <=(const float& a, const ssef& b) { return ssef(a) <= b; }

__forceinline const ssef select(const sseb& m, const ssef& t, const ssef& f) {
#ifdef __KERNEL_SSE41__
	return _mm_blendv_ps(f, t, m);
#else
	return _mm_or_ps(_mm_and_ps(m, t), _mm_andnot_ps(m, f));
#endif
}

__forceinline const ssef select(const ssef& m, const ssef& t, const ssef& f) {
#ifdef __KERNEL_SSE41__
	return _mm_blendv_ps(f, t, m);
#else
	return _mm_or_ps(_mm_and_ps(m, t), _mm_andnot_ps(m, f));
#endif
}

__forceinline const ssef select(const int mask, const ssef& t, const ssef& f) { 
#if defined(__KERNEL_SSE41__) && ((!defined(__clang__) && !defined(_MSC_VER)) || defined(__INTEL_COMPILER))
	return _mm_blend_ps(f, t, mask);
#else
	return select(sseb(mask),t,f);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// Rounding Functions
////////////////////////////////////////////////////////////////////////////////

#if defined(__KERNEL_SSE41__)
__forceinline const ssef round_even(const ssef& a) { return _mm_round_ps(a, _MM_FROUND_TO_NEAREST_INT); }
__forceinline const ssef round_down(const ssef& a) { return _mm_round_ps(a, _MM_FROUND_TO_NEG_INF   ); }
__forceinline const ssef round_up (const ssef& a) { return _mm_round_ps(a, _MM_FROUND_TO_POS_INF   ); }
__forceinline const ssef round_zero(const ssef& a) { return _mm_round_ps(a, _MM_FROUND_TO_ZERO      ); }
__forceinline const ssef floor    (const ssef& a) { return _mm_round_ps(a, _MM_FROUND_TO_NEG_INF   ); }
__forceinline const ssef ceil     (const ssef& a) { return _mm_round_ps(a, _MM_FROUND_TO_POS_INF   ); }
#endif

__forceinline ssei truncatei(const ssef& a) {
	return _mm_cvttps_epi32(a.m128);
}

__forceinline ssei floori(const ssef& a) {
#if defined(__KERNEL_SSE41__)
	return ssei(floor(a));
#else
	return ssei(a-ssef(0.5f));
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// Movement/Shifting/Shuffling Functions
////////////////////////////////////////////////////////////////////////////////

__forceinline ssef unpacklo(const ssef& a, const ssef& b) { return _mm_unpacklo_ps(a.m128, b.m128); }
__forceinline ssef unpackhi(const ssef& a, const ssef& b) { return _mm_unpackhi_ps(a.m128, b.m128); }

template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const ssef shuffle(const ssef& b) {
	return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(b), _MM_SHUFFLE(i3, i2, i1, i0)));
}

template<size_t i0, size_t i1, size_t i2, size_t i3> __forceinline const ssef shuffle(const ssef& a, const ssef& b) {
	return _mm_shuffle_ps(a, b, _MM_SHUFFLE(i3, i2, i1, i0));
}

#if defined(__KERNEL_SSSE3__)
__forceinline const ssef shuffle8(const ssef& a, const ssei& shuf) { 
	return _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(a), shuf)); 
}
#endif

#if defined(__KERNEL_SSE3__)
template<> __forceinline const ssef shuffle<0, 0, 2, 2>(const ssef& b) { return _mm_moveldup_ps(b); }
template<> __forceinline const ssef shuffle<1, 1, 3, 3>(const ssef& b) { return _mm_movehdup_ps(b); }
template<> __forceinline const ssef shuffle<0, 1, 0, 1>(const ssef& b) { return _mm_castpd_ps(_mm_movedup_pd(_mm_castps_pd(b))); }
#endif

template<size_t i0> __forceinline const ssef shuffle(const ssef& b) {
	return shuffle<i0,i0,i0,i0>(b);
}

#if defined(__KERNEL_SSE41__) && !defined(__GNUC__)
template<size_t i> __forceinline float extract  (const ssef& a) { return _mm_cvtss_f32(_mm_extract_ps(a,i)); }
#else
template<size_t i> __forceinline float extract  (const ssef& a) { return _mm_cvtss_f32(shuffle<i,i,i,i>(a)); }
#endif
template<>         __forceinline float extract<0>(const ssef& a) { return _mm_cvtss_f32(a); }

#if defined(__KERNEL_SSE41__)
template<size_t dst, size_t src, size_t clr> __forceinline const ssef insert(const ssef& a, const ssef& b) { return _mm_insert_ps(a, b,(dst << 4) |(src << 6) | clr); }
template<size_t dst, size_t src> __forceinline const ssef insert(const ssef& a, const ssef& b) { return insert<dst, src, 0>(a, b); }
template<size_t dst>             __forceinline const ssef insert(const ssef& a, const float b) { return insert<dst,      0>(a, _mm_set_ss(b)); }
#else
template<size_t dst>             __forceinline const ssef insert(const ssef& a, const float b) { ssef c = a; c[dst] = b; return c; }
#endif

////////////////////////////////////////////////////////////////////////////////
/// Transpose
////////////////////////////////////////////////////////////////////////////////

__forceinline void transpose(const ssef& r0, const ssef& r1, const ssef& r2, const ssef& r3, ssef& c0, ssef& c1, ssef& c2, ssef& c3) 
{
	ssef l02 = unpacklo(r0,r2);
	ssef h02 = unpackhi(r0,r2);
	ssef l13 = unpacklo(r1,r3);
	ssef h13 = unpackhi(r1,r3);
	c0 = unpacklo(l02,l13);
	c1 = unpackhi(l02,l13);
	c2 = unpacklo(h02,h13);
	c3 = unpackhi(h02,h13);
}

__forceinline void transpose(const ssef& r0, const ssef& r1, const ssef& r2, const ssef& r3, ssef& c0, ssef& c1, ssef& c2) 
{
	ssef l02 = unpacklo(r0,r2);
	ssef h02 = unpackhi(r0,r2);
	ssef l13 = unpacklo(r1,r3);
	ssef h13 = unpackhi(r1,r3);
	c0 = unpacklo(l02,l13);
	c1 = unpackhi(l02,l13);
	c2 = unpacklo(h02,h13);
}

////////////////////////////////////////////////////////////////////////////////
/// Reductions
////////////////////////////////////////////////////////////////////////////////

__forceinline const ssef vreduce_min(const ssef& v) { ssef h = min(shuffle<1,0,3,2>(v),v); return min(shuffle<2,3,0,1>(h),h); }
__forceinline const ssef vreduce_max(const ssef& v) { ssef h = max(shuffle<1,0,3,2>(v),v); return max(shuffle<2,3,0,1>(h),h); }
__forceinline const ssef vreduce_add(const ssef& v) { ssef h = shuffle<1,0,3,2>(v)   + v ; return shuffle<2,3,0,1>(h)   + h ; }

__forceinline float reduce_min(const ssef& v) { return _mm_cvtss_f32(vreduce_min(v)); }
__forceinline float reduce_max(const ssef& v) { return _mm_cvtss_f32(vreduce_max(v)); }
__forceinline float reduce_add(const ssef& v) { return _mm_cvtss_f32(vreduce_add(v)); }

__forceinline size_t select_min(const ssef& v) { return __bsf(movemask(v == vreduce_min(v))); }
__forceinline size_t select_max(const ssef& v) { return __bsf(movemask(v == vreduce_max(v))); }

__forceinline size_t select_min(const sseb& valid, const ssef& v) { const ssef a = select(valid,v,ssef(pos_inf)); return __bsf(movemask(valid &(a == vreduce_min(a)))); }
__forceinline size_t select_max(const sseb& valid, const ssef& v) { const ssef a = select(valid,v,ssef(neg_inf)); return __bsf(movemask(valid &(a == vreduce_max(a)))); }

////////////////////////////////////////////////////////////////////////////////
/// Memory load and store operations
////////////////////////////////////////////////////////////////////////////////

__forceinline ssef load4f(const float4& a) {
#ifdef __KERNEL_WITH_SSE_ALIGN__
	return _mm_load_ps(&a.x); 
#else
	return _mm_loadu_ps(&a.x); 
#endif
}

__forceinline ssef load4f(const float3& a) {
#ifdef __KERNEL_WITH_SSE_ALIGN__
	return _mm_load_ps(&a.x); 
#else
	return _mm_loadu_ps(&a.x); 
#endif
}

__forceinline ssef load4f(const void* const a) {
	return _mm_load_ps((float*)a); 
}

__forceinline ssef load1f_first(const float a) {
	return _mm_set_ss(a);
}

__forceinline void store4f(void* ptr, const ssef& v) {
	_mm_store_ps((float*)ptr,v);
}

__forceinline ssef loadu4f(const void* const a) {
	return _mm_loadu_ps((float*)a); 
}

__forceinline void storeu4f(void* ptr, const ssef& v) {
	_mm_storeu_ps((float*)ptr,v);
}

__forceinline void store4f(const sseb& mask, void* ptr, const ssef& f) { 
#if defined(__KERNEL_AVX__)
	_mm_maskstore_ps((float*)ptr,(__m128i)mask,f);
#else
	*(ssef*)ptr = select(mask,f,*(ssef*)ptr);
#endif
}

__forceinline ssef load4f_nt(void* ptr) {
#if defined(__KERNEL_SSE41__)
	return _mm_castsi128_ps(_mm_stream_load_si128((__m128i*)ptr));
#else
	return _mm_load_ps((float*)ptr); 
#endif
}

__forceinline void store4f_nt(void* ptr, const ssef& v) {
#if defined(__KERNEL_SSE41__)
	_mm_stream_ps((float*)ptr,v);
#else
	_mm_store_ps((float*)ptr,v);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// Euclidian Space Operators
////////////////////////////////////////////////////////////////////////////////

__forceinline float dot(const ssef& a, const ssef& b) {
	return reduce_add(a*b);
}

/* calculate shuffled cross product, useful when order of components does not matter */
__forceinline ssef cross_zxy(const ssef& a, const ssef& b) 
{
	const ssef a0 = a;
	const ssef b0 = shuffle<1,2,0,3>(b);
	const ssef a1 = shuffle<1,2,0,3>(a);
	const ssef b1 = b;
	return msub(a0,b0,a1*b1);
}

__forceinline ssef cross(const ssef& a, const ssef& b) 
{
	return shuffle<1,2,0,3>(cross_zxy(a, b));
}

ccl_device_inline const ssef dot3_splat(const ssef& a, const ssef& b)
{
#ifdef __KERNEL_SSE41__
	return _mm_dp_ps(a.m128, b.m128, 0x7f);
#else
	ssef t = a * b;
	return ssef(((float*)&t)[0] + ((float*)&t)[1] + ((float*)&t)[2]);
#endif
}

/* squared length taking only specified axes into account */
template<size_t X, size_t Y, size_t Z, size_t W>
ccl_device_inline float len_squared(const ssef& a)
{
#ifndef __KERNEL_SSE41__
	float4& t = (float4 &)a;
	return (X ? t.x * t.x : 0.0f) + (Y ? t.y * t.y : 0.0f) + (Z ? t.z * t.z : 0.0f) + (W ? t.w * t.w : 0.0f);
#else
	return extract<0>(ssef(_mm_dp_ps(a.m128, a.m128, (X << 4) | (Y << 5) | (Z << 6) | (W << 7) | 0xf)));
#endif
}

ccl_device_inline float dot3(const ssef& a, const ssef& b)
{
#ifdef __KERNEL_SSE41__
	return extract<0>(ssef(_mm_dp_ps(a.m128, b.m128, 0x7f)));
#else
	ssef t = a * b;
	return ((float*)&t)[0] + ((float*)&t)[1] + ((float*)&t)[2];
#endif
}

ccl_device_inline const ssef len3_squared_splat(const ssef& a)
{
	return dot3_splat(a, a);
}

ccl_device_inline float len3_squared(const ssef& a)
{
	return dot3(a, a);
}

ccl_device_inline float len3(const ssef& a)
{
	return extract<0>(mm_sqrt(dot3_splat(a, a)));
}

/* SSE shuffle utility functions */

#ifdef __KERNEL_SSSE3__

/* faster version for SSSE3 */
typedef ssei shuffle_swap_t;

ccl_device_inline const shuffle_swap_t shuffle_swap_identity(void)
{
	return _mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0);
}

ccl_device_inline const shuffle_swap_t shuffle_swap_swap(void)
{
	return _mm_set_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8);
}

ccl_device_inline const ssef shuffle_swap(const ssef& a, const shuffle_swap_t& shuf)
{
	return cast(_mm_shuffle_epi8(cast(a), shuf));
}

#else

/* somewhat slower version for SSE2 */
typedef int shuffle_swap_t;

ccl_device_inline const shuffle_swap_t shuffle_swap_identity(void)
{
	return 0;
}

ccl_device_inline const shuffle_swap_t shuffle_swap_swap(void)
{
	return 1;
}

ccl_device_inline const ssef shuffle_swap(const ssef& a, shuffle_swap_t shuf)
{
	/* shuffle value must be a constant, so we need to branch */
	if(shuf)
		return ssef(_mm_shuffle_ps(a.m128, a.m128, _MM_SHUFFLE(1, 0, 3, 2)));
	else
		return ssef(_mm_shuffle_ps(a.m128, a.m128, _MM_SHUFFLE(3, 2, 1, 0)));
}

#endif

#ifdef __KERNEL_SSE41__

ccl_device_inline void gen_idirsplat_swap(const ssef &pn, const shuffle_swap_t &shuf_identity, const shuffle_swap_t &shuf_swap,
										  const float3& idir, ssef idirsplat[3], shuffle_swap_t shufflexyz[3])
{
	const __m128 idirsplat_raw[] = { _mm_set_ps1(idir.x), _mm_set_ps1(idir.y), _mm_set_ps1(idir.z) };
	idirsplat[0] = _mm_xor_ps(idirsplat_raw[0], pn);
	idirsplat[1] = _mm_xor_ps(idirsplat_raw[1], pn);
	idirsplat[2] = _mm_xor_ps(idirsplat_raw[2], pn);

	const ssef signmask = cast(ssei(0x80000000));
	const ssef shuf_identity_f = cast(shuf_identity);
	const ssef shuf_swap_f = cast(shuf_swap);

	shufflexyz[0] = _mm_castps_si128(_mm_blendv_ps(shuf_identity_f, shuf_swap_f, _mm_and_ps(idirsplat_raw[0], signmask)));
	shufflexyz[1] = _mm_castps_si128(_mm_blendv_ps(shuf_identity_f, shuf_swap_f, _mm_and_ps(idirsplat_raw[1], signmask)));
	shufflexyz[2] = _mm_castps_si128(_mm_blendv_ps(shuf_identity_f, shuf_swap_f, _mm_and_ps(idirsplat_raw[2], signmask)));
}

#else

ccl_device_inline void gen_idirsplat_swap(const ssef &pn, const shuffle_swap_t &shuf_identity, const shuffle_swap_t &shuf_swap,
										  const float3& idir, ssef idirsplat[3], shuffle_swap_t shufflexyz[3])
{
	idirsplat[0] = ssef(idir.x) ^ pn;
	idirsplat[1] = ssef(idir.y) ^ pn;
	idirsplat[2] = ssef(idir.z) ^ pn;

	shufflexyz[0] = (idir.x >= 0)? shuf_identity: shuf_swap;
	shufflexyz[1] = (idir.y >= 0)? shuf_identity: shuf_swap;
	shufflexyz[2] = (idir.z >= 0)? shuf_identity: shuf_swap;
}

#endif

ccl_device_inline const ssef uint32_to_float(const ssei &in)
{
	ssei a = _mm_srli_epi32(in, 16);
	ssei b = _mm_and_si128(in, _mm_set1_epi32(0x0000ffff));
	ssei c = _mm_or_si128(a, _mm_set1_epi32(0x53000000));
	ssef d = _mm_cvtepi32_ps(b);
	ssef e = _mm_sub_ps(_mm_castsi128_ps(c), _mm_castsi128_ps(_mm_set1_epi32(0x53000000)));
	return _mm_add_ps(e, d);
}

template<size_t S1, size_t S2, size_t S3, size_t S4>
ccl_device_inline const ssef set_sign_bit(const ssef &a)
{
	return a ^ cast(ssei(S1 << 31, S2 << 31, S3 << 31, S4 << 31));
}

#endif

CCL_NAMESPACE_END

#endif

