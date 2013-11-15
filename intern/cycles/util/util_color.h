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

	if(s == 0.0f) {
		h = 0.0f;
	}
	else {
		float3 cmax3 = make_float3(cmax, cmax, cmax);
		c = (cmax3 - rgb)/cdelta;

		if(rgb.x == cmax) h = c.z - c.y;
		else if(rgb.y == cmax) h = 2.0f + c.x -  c.z;
		else h = 4.0f + c.y - c.x;

		h /= 6.0f;

		if(h < 0.0f)
			h += 1.0f;
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

	if(s == 0.0f) {
		rgb = make_float3(v, v, v);
	}
	else {
		if(h == 1.0f)
			h = 0.0f;
		
		h *= 6.0f;
		i = floorf(h);
		f = h - i;
		rgb = make_float3(f, f, f);
		p = v*(1.0f-s);
		q = v*(1.0f-(s*f));
		t = v*(1.0f-(s*(1.0f-f)));
		
		if(i == 0.0f) rgb = make_float3(v, t, p);
		else if(i == 1.0f) rgb = make_float3(q, v, p);
		else if(i == 2.0f) rgb = make_float3(p, v, t);
		else if(i == 3.0f) rgb = make_float3(p, q, v);
		else if(i == 4.0f) rgb = make_float3(t, p, v);
		else rgb = make_float3(v, p, q);
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

