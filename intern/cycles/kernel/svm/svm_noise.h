/*
 * Adapted from Open Shading Language with this license:
 *
 * Copyright (c) 2009-2010 Sony Pictures Imageworks Inc., et al.
 * All Rights Reserved.
 *
 * Modifications Copyright 2011, Blender Foundation.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * * Neither the name of Sony Pictures Imageworks nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_SSE2__
ccl_device int quick_floor(float x)
{
	return float_to_int(x) - ((x < 0) ? 1 : 0);
}
#else
ccl_device_inline __m128i quick_floor_sse(const __m128& x)
{
	__m128i b = _mm_cvttps_epi32(x);
	__m128i isneg = _mm_castps_si128(_mm_cmplt_ps(x, _mm_set1_ps(0.0f)));
	return _mm_add_epi32(b, isneg); // unsaturated add 0xffffffff is the same as subtract -1
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float bits_to_01(uint bits)
{
	return bits * (1.0f/(float)0xFFFFFFFF);
}
#else
ccl_device_inline __m128 bits_to_01_sse(const __m128i& bits)
{
	return _mm_mul_ps(uint32_to_float(bits), _mm_set1_ps(1.0f/(float)0xFFFFFFFF));
}
#endif

ccl_device uint hash(uint kx, uint ky, uint kz)
{
	// define some handy macros
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
#define final(a,b,c) \
{ \
	c ^= b; c -= rot(b,14); \
	a ^= c; a -= rot(c,11); \
	b ^= a; b -= rot(a,25); \
	c ^= b; c -= rot(b,16); \
	a ^= c; a -= rot(c,4);  \
	b ^= a; b -= rot(a,14); \
	c ^= b; c -= rot(b,24); \
}
	// now hash the data!
	uint a, b, c, len = 3;
	a = b = c = 0xdeadbeef + (len << 2) + 13;

	c += kz;
	b += ky;
	a += kx;
	final(a, b, c);

	return c;
	// macros not needed anymore
#undef rot
#undef final
}

#ifdef __KERNEL_SSE2__
ccl_device_inline __m128i hash_sse(const __m128i& kx, const __m128i& ky, const __m128i& kz)
{
#define rot(x,k) _mm_or_si128(_mm_slli_epi32((x), (k)), _mm_srli_epi32((x), 32-(k)))
#define xor_rot(a, b, c) do {a = _mm_xor_si128(a, b); a = _mm_sub_epi32(a, rot(b, c));} while(0)

	uint len = 3;
	__m128i magic = _mm_set1_epi32(0xdeadbeef + (len << 2) + 13);
	__m128i a = _mm_add_epi32(magic, kx);
	__m128i b = _mm_add_epi32(magic, ky);
	__m128i c = _mm_add_epi32(magic, kz);

	xor_rot(c, b, 14);
	xor_rot(a, c, 11);
	xor_rot(b, a, 25);
	xor_rot(c, b, 16);
	xor_rot(a, c, 4);
	xor_rot(b, a, 14);
	xor_rot(c, b, 24);

	return c;
#undef rot
#undef xor_rot
}
#endif

#if 0 // unused
ccl_device int imod(int a, int b)
{
	a %= b;
	return a < 0 ? a + b : a;
}

ccl_device uint phash(int kx, int ky, int kz, int3 p) 
{
	return hash(imod(kx, p.x), imod(ky, p.y), imod(kz, p.z));
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float floorfrac(float x, int* i)
{
	*i = quick_floor(x);
	return x - *i;
}
#else
ccl_device_inline __m128 floorfrac_sse(const __m128& x, __m128i *i)
{
	*i = quick_floor_sse(x);
	return _mm_sub_ps(x, _mm_cvtepi32_ps(*i));
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float fade(float t)
{
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}
#else
ccl_device_inline __m128 fade_sse(const __m128 *t)
{
	__m128 a = fma(*t, _mm_set1_ps(6.0f), _mm_set1_ps(-15.0f));
	__m128 b = fma(*t, a, _mm_set1_ps(10.0f));
	return _mm_mul_ps(_mm_mul_ps(*t, *t), _mm_mul_ps(*t, b));
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float nerp(float t, float a, float b)
{
	return (1.0f - t) * a + t * b;
}
#else
ccl_device_inline __m128 nerp_sse(const __m128& t, const __m128& a, const __m128& b)
{
	__m128 x1 = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(1.0f), t), a);
	return fma(t, b, x1);
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float grad(int hash, float x, float y, float z)
{
	// use vectors pointing to the edges of the cube
	int h = hash & 15;
	float u = h<8 ? x : y;
	float vt = ((h == 12) | (h == 14)) ? x : z;
	float v = h < 4 ? y : vt;
	return ((h&1) ? -u : u) + ((h&2) ? -v : v);
}
#else
ccl_device_inline __m128 grad_sse(const __m128i& hash, const __m128& x, const __m128& y, const __m128& z)
{
	__m128i c1 = _mm_set1_epi32(1);
	__m128i c2 = _mm_set1_epi32(2);

	__m128i h = _mm_and_si128(hash, _mm_set1_epi32(15));          // h = hash & 15

	__m128i case_ux = _mm_cmplt_epi32(h, _mm_set1_epi32(8));       // 0xffffffff if h < 8 else 0

	__m128 u = blend(_mm_castsi128_ps(case_ux), x, y);             // u = h<8 ? x : y

	__m128i case_vy = _mm_cmplt_epi32(h, _mm_set1_epi32(4));       // 0xffffffff if h < 4 else 0

	__m128i case_h12 = _mm_cmpeq_epi32(h, _mm_set1_epi32(12));     // 0xffffffff if h == 12 else 0
	__m128i case_h14 = _mm_cmpeq_epi32(h, _mm_set1_epi32(14));     // 0xffffffff if h == 14 else 0

	__m128i case_vx = _mm_or_si128(case_h12, case_h14);            // 0xffffffff if h == 12 or h == 14 else 0

	__m128 v = blend(_mm_castsi128_ps(case_vy), y, blend(_mm_castsi128_ps(case_vx), x, z)); // v = h<4 ? y : h == 12 || h == 14 ? x : z

	__m128i case_uneg = _mm_slli_epi32(_mm_and_si128(h, c1), 31);  // 1<<31 if h&1 else 0
	__m128 case_uneg_mask = _mm_castsi128_ps(case_uneg);           // -0.0 if h&1 else +0.0
	__m128 ru = _mm_xor_ps(u, case_uneg_mask);                     // -u if h&1 else u (copy float sign)

	__m128i case_vneg = _mm_slli_epi32(_mm_and_si128(h, c2), 30);  // 2<<30 if h&2 else 0
	__m128 case_vneg_mask = _mm_castsi128_ps(case_vneg);           // -0.0 if h&2 else +0.0
	__m128 rv = _mm_xor_ps(v, case_vneg_mask);                     // -v if h&2 else v (copy float sign)

	__m128 r = _mm_add_ps(ru, rv);                                 // ((h&1) ? -u : u) + ((h&2) ? -v : v)
	return r;
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device float scale3(float result)
{
	return 0.9820f * result;
}
#else
ccl_device_inline __m128 scale3_sse(const __m128& result)
{
	return _mm_mul_ps(_mm_set1_ps(0.9820f), result);
}
#endif

#ifndef __KERNEL_SSE2__
ccl_device_noinline float perlin(float x, float y, float z)
{
	int X; float fx = floorfrac(x, &X);
	int Y; float fy = floorfrac(y, &Y);
	int Z; float fz = floorfrac(z, &Z);

	float u = fade(fx);
	float v = fade(fy);
	float w = fade(fz);

	float result;

	result = nerp (w, nerp (v, nerp (u, grad (hash (X  , Y  , Z  ), fx	 , fy	 , fz	  ),
										grad (hash (X+1, Y  , Z  ), fx-1.0f, fy	 , fz	  )),
							   nerp (u, grad (hash (X  , Y+1, Z  ), fx	 , fy-1.0f, fz	  ),
										grad (hash (X+1, Y+1, Z  ), fx-1.0f, fy-1.0f, fz	  ))),
					  nerp (v, nerp (u, grad (hash (X  , Y  , Z+1), fx	 , fy	 , fz-1.0f ),
										grad (hash (X+1, Y  , Z+1), fx-1.0f, fy	 , fz-1.0f )),
							   nerp (u, grad (hash (X  , Y+1, Z+1), fx	 , fy-1.0f, fz-1.0f ),
										grad (hash (X+1, Y+1, Z+1), fx-1.0f, fy-1.0f, fz-1.0f ))));
	float r = scale3(result);

	/* can happen for big coordinates, things even out to 0.0 then anyway */
	return (isfinite(r))? r: 0.0f;
}
#else
ccl_device_noinline float perlin(float x, float y, float z)
{
	__m128 xyz = _mm_setr_ps(x, y, z, 0.0f);
	__m128i XYZ;

	__m128 fxyz = floorfrac_sse(xyz, &XYZ);

	__m128 uvw = fade_sse(&fxyz);
	__m128 u = broadcast<0>(uvw), v = broadcast<1>(uvw), w = broadcast<2>(uvw);

	__m128i XYZ_ofc = _mm_add_epi32(XYZ, _mm_set1_epi32(1));
	__m128i vdy = shuffle<1, 1, 1, 1>(XYZ, XYZ_ofc);                      // +0, +0, +1, +1
	__m128i vdz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(XYZ, XYZ_ofc)); // +0, +1, +0, +1

	__m128i h1 = hash_sse(broadcast<0>(XYZ),     vdy, vdz);               // hash directions 000, 001, 010, 011
	__m128i h2 = hash_sse(broadcast<0>(XYZ_ofc), vdy, vdz);               // hash directions 100, 101, 110, 111

	__m128 fxyz_ofc = _mm_sub_ps(fxyz, _mm_set1_ps(1.0f));
	__m128 vfy = shuffle<1, 1, 1, 1>(fxyz, fxyz_ofc);
	__m128 vfz = shuffle<0, 2, 0, 2>(shuffle<2, 2, 2, 2>(fxyz, fxyz_ofc));

	__m128 g1 = grad_sse(h1, broadcast<0>(fxyz),     vfy, vfz);
	__m128 g2 = grad_sse(h2, broadcast<0>(fxyz_ofc), vfy, vfz);
	__m128 n1 = nerp_sse(u, g1, g2);

	__m128 n1_half = shuffle<2, 3, 2, 3>(n1);      // extract 2 floats to a separate vector
	__m128 n2 = nerp_sse(v, n1, n1_half);          // process nerp([a, b, _, _], [c, d, _, _]) -> [a', b', _, _]

	__m128 n2_second = broadcast<1>(n2);           // extract b to a separate vector
	__m128 result = nerp_sse(w, n2, n2_second);    // process nerp([a', _, _, _], [b', _, _, _]) -> [a'', _, _, _]

	__m128 r = scale3_sse(result);

	__m128 infmask = _mm_castsi128_ps(_mm_set1_epi32(0x7f800000));
	__m128 rinfmask = _mm_cmpeq_ps(_mm_and_ps(r, infmask), infmask); // 0xffffffff if r is inf/-inf/nan else 0
	__m128 rfinite = _mm_andnot_ps(rinfmask, r);   // 0 if r is inf/-inf/nan else r
	return _mm_cvtss_f32(rfinite);
}
#endif

#if 0 // unused
ccl_device_noinline float perlin_periodic(float x, float y, float z, float3 pperiod)
{
	int X; float fx = floorfrac(x, &X);
	int Y; float fy = floorfrac(y, &Y);
	int Z; float fz = floorfrac(z, &Z);

	int3 p;

	p.x = max(quick_floor(pperiod.x), 1);
	p.y = max(quick_floor(pperiod.y), 1);
	p.z = max(quick_floor(pperiod.z), 1);

	float u = fade(fx);
	float v = fade(fy);
	float w = fade(fz);

	float result;

	result = nerp (w, nerp (v, nerp (u, grad (phash (X  , Y  , Z  , p), fx	 , fy	 , fz	  ),
										grad (phash (X+1, Y  , Z  , p), fx-1.0f, fy	 , fz	  )),
							   nerp (u, grad (phash (X  , Y+1, Z  , p), fx	 , fy-1.0f, fz	  ),
										grad (phash (X+1, Y+1, Z  , p), fx-1.0f, fy-1.0f, fz	  ))),
					  nerp (v, nerp (u, grad (phash (X  , Y  , Z+1, p), fx	 , fy	 , fz-1.0f ),
										grad (phash (X+1, Y  , Z+1, p), fx-1.0f, fy	 , fz-1.0f )),
							   nerp (u, grad (phash (X  , Y+1, Z+1, p), fx	 , fy-1.0f, fz-1.0f ),
										grad (phash (X+1, Y+1, Z+1, p), fx-1.0f, fy-1.0f, fz-1.0f ))));
	float r = scale3(result);

	/* can happen for big coordinates, things even out to 0.0 then anyway */
	return (isfinite(r))? r: 0.0f;
}
#endif

/* perlin noise in range 0..1 */
ccl_device float noise(float3 p)
{
	float r = perlin(p.x, p.y, p.z);
	return 0.5f*r + 0.5f;
}

/* perlin noise in range -1..1 */
ccl_device float snoise(float3 p)
{
	return perlin(p.x, p.y, p.z);
}

/* cell noise */
#ifndef __KERNEL_SSE2__
ccl_device_noinline float cellnoise(float3 p)
{
	uint ix = quick_floor(p.x);
	uint iy = quick_floor(p.y);
	uint iz = quick_floor(p.z);

	return bits_to_01(hash(ix, iy, iz));
}

ccl_device float3 cellnoise_color(float3 p)
{
	float r = cellnoise(p);
	float g = cellnoise(make_float3(p.y, p.x, p.z));
	float b = cellnoise(make_float3(p.y, p.z, p.x));

	return make_float3(r, g, b);
}
#else
ccl_device __m128 cellnoise_color(const __m128& p)
{
	__m128i ip = quick_floor_sse(p);
	__m128i ip_yxz = shuffle<1, 0, 2, 3>(ip);
	__m128i ip_xyy = shuffle<0, 1, 1, 3>(ip);
	__m128i ip_zzx = shuffle<2, 2, 0, 3>(ip);
	return bits_to_01_sse(hash_sse(ip_xyy, ip_yxz, ip_zzx));
}
#endif

#if 0 // unused
/* periodic perlin noise in range 0..1 */
ccl_device float pnoise(float3 p, float3 pperiod)
{
	float r = perlin_periodic(p.x, p.y, p.z, pperiod);
	return 0.5f*r + 0.5f;
}

/* periodic perlin noise in range -1..1 */
ccl_device float psnoise(float3 p, float3 pperiod)
{
	return perlin_periodic(p.x, p.y, p.z, pperiod);
}
#endif

CCL_NAMESPACE_END

