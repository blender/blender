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
 * limitations under the License
 */

#ifndef __UTIL_COLOR_H__
#define __UTIL_COLOR_H__

#include "util_math.h"
#include "util_types.h"

#ifdef __KERNEL_SSE2__
#include "util_simd.h"
#endif

CCL_NAMESPACE_BEGIN

ccl_device float color_srgb_to_scene_linear(float c)
{
	if(c < 0.04045f)
		return (c < 0.0f)? 0.0f: c * (1.0f/12.92f);
	else
		return powf((c + 0.055f) * (1.0f / 1.055f), 2.4f);
}

ccl_device float color_scene_linear_to_srgb(float c)
{
	if(c < 0.0031308f)
		return (c < 0.0f)? 0.0f: c * 12.92f;
	else
		return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

ccl_device float3 rgb_to_hsv(float3 rgb)
{
	float cmax, cmin, h, s, v, cdelta;
	float3 c;

	cmax = fmaxf(rgb.x, fmaxf(rgb.y, rgb.z));
	cmin = min(rgb.x, min(rgb.y, rgb.z));
	cdelta = cmax - cmin;

	v = cmax;

	if(cmax != 0.0f) {
		s = cdelta/cmax;
	}
	else {
		s = 0.0f;
		h = 0.0f;
	}

	if(s != 0.0f) {
		float3 cmax3 = make_float3(cmax, cmax, cmax);
		c = (cmax3 - rgb)/cdelta;

		if     (rgb.x == cmax) h =        c.z - c.y;
		else if(rgb.y == cmax) h = 2.0f + c.x - c.z;
		else                   h = 4.0f + c.y - c.x;

		h /= 6.0f;

		if(h < 0.0f)
			h += 1.0f;
	}
	else {
		h = 0.0f;
	}

	return make_float3(h, s, v);
}

ccl_device float3 hsv_to_rgb(float3 hsv)
{
	float i, f, p, q, t, h, s, v;
	float3 rgb;

	h = hsv.x;
	s = hsv.y;
	v = hsv.z;

	if(s != 0.0f) {
		if(h == 1.0f)
			h = 0.0f;

		h *= 6.0f;
		i = floorf(h);
		f = h - i;
		rgb = make_float3(f, f, f);
		p = v*(1.0f-s);
		q = v*(1.0f-(s*f));
		t = v*(1.0f-(s*(1.0f-f)));

		if     (i == 0.0f) rgb = make_float3(v, t, p);
		else if(i == 1.0f) rgb = make_float3(q, v, p);
		else if(i == 2.0f) rgb = make_float3(p, v, t);
		else if(i == 3.0f) rgb = make_float3(p, q, v);
		else if(i == 4.0f) rgb = make_float3(t, p, v);
		else               rgb = make_float3(v, p, q);
	}
	else {
		rgb = make_float3(v, v, v);
	}

	return rgb;
}

ccl_device float3 xyY_to_xyz(float x, float y, float Y)
{
	float X, Z;

	if(y != 0.0f) X = (x / y) * Y;
	else X = 0.0f;

	if(y != 0.0f && Y != 0.0f) Z = (1.0f - x - y) / y * Y;
	else Z = 0.0f;

	return make_float3(X, Y, Z);
}

ccl_device float3 xyz_to_rgb(float x, float y, float z)
{
	return make_float3(3.240479f * x + -1.537150f * y + -0.498535f * z,
	                  -0.969256f * x +  1.875991f * y +  0.041556f * z,
	                   0.055648f * x + -0.204043f * y +  1.057311f * z);
}

#ifndef __KERNEL_OPENCL__

ccl_device float3 color_srgb_to_scene_linear(float3 c)
{
	return make_float3(
		color_srgb_to_scene_linear(c.x),
		color_srgb_to_scene_linear(c.y),
		color_srgb_to_scene_linear(c.z));
}

#ifdef __KERNEL_SSE2__
/*
 * Calculate initial guess for arg^exp based on float representation
 * This method gives a constant bias, which can be easily compensated by multiplicating with bias_coeff.
 * Gives better results for exponents near 1 (e. g. 4/5).
 * exp = exponent, encoded as uint32_t
 * e2coeff = 2^(127/exponent - 127) * bias_coeff^(1/exponent), encoded as uint32_t
 */
template<unsigned exp, unsigned e2coeff>
ccl_device_inline __m128 fastpow(const __m128 &arg)
{
	__m128 ret;
	ret = _mm_mul_ps(arg, _mm_castsi128_ps(_mm_set1_epi32(e2coeff)));
	ret = _mm_cvtepi32_ps(_mm_castps_si128(ret));
	ret = _mm_mul_ps(ret, _mm_castsi128_ps(_mm_set1_epi32(exp)));
	ret = _mm_castsi128_ps(_mm_cvtps_epi32(ret));
	return ret;
}

/* Improve x ^ 1.0f/5.0f solution with Newton-Raphson method */
ccl_device_inline __m128 improve_5throot_solution(const __m128 &old_result, const __m128 &x)
{
	__m128 approx2 = _mm_mul_ps(old_result, old_result);
	__m128 approx4 = _mm_mul_ps(approx2, approx2);
	__m128 t = _mm_div_ps(x, approx4);
	__m128 summ = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(4.0f), old_result), t); /* fma */
	return _mm_mul_ps(summ, _mm_set1_ps(1.0f/5.0f));
}

/* Calculate powf(x, 2.4). Working domain: 1e-10 < x < 1e+10 */
ccl_device_inline __m128 fastpow24(const __m128 &arg)
{
	/* max, avg and |avg| errors were calculated in gcc without FMA instructions
	 * The final precision should be better than powf in glibc */

	/* Calculate x^4/5, coefficient 0.994 was constructed manually to minimize avg error */
	/* 0x3F4CCCCD = 4/5 */
	/* 0x4F55A7FB = 2^(127/(4/5) - 127) * 0.994^(1/(4/5)) */
	__m128 x = fastpow<0x3F4CCCCD, 0x4F55A7FB>(arg); // error max = 0.17	avg = 0.0018	|avg| = 0.05
	__m128 arg2 = _mm_mul_ps(arg, arg);
	__m128 arg4 = _mm_mul_ps(arg2, arg2);
	x = improve_5throot_solution(x, arg4); /* error max = 0.018		avg = 0.0031	|avg| = 0.0031  */
	x = improve_5throot_solution(x, arg4); /* error max = 0.00021	avg = 1.6e-05	|avg| = 1.6e-05 */
	x = improve_5throot_solution(x, arg4); /* error max = 6.1e-07	avg = 5.2e-08	|avg| = 1.1e-07 */
	return _mm_mul_ps(x, _mm_mul_ps(x, x));
}

ccl_device __m128 color_srgb_to_scene_linear(const __m128 &c)
{
	__m128 cmp = _mm_cmplt_ps(c, _mm_set1_ps(0.04045f));
	__m128 lt = _mm_max_ps(_mm_mul_ps(c, _mm_set1_ps(1.0f/12.92f)), _mm_set1_ps(0.0f));
	__m128 gtebase = _mm_mul_ps(_mm_add_ps(c, _mm_set1_ps(0.055f)), _mm_set1_ps(1.0f/1.055f)); /* fma */
	__m128 gte = fastpow24(gtebase);
	return blend(cmp, lt, gte);
}
#endif

ccl_device float3 color_scene_linear_to_srgb(float3 c)
{
	return make_float3(
		color_scene_linear_to_srgb(c.x),
		color_scene_linear_to_srgb(c.y),
		color_scene_linear_to_srgb(c.z));
}

#endif

ccl_device float linear_rgb_to_gray(float3 c)
{
	return c.x*0.2126f + c.y*0.7152f + c.z*0.0722f;
}

CCL_NAMESPACE_END

#endif /* __UTIL_COLOR_H__ */

