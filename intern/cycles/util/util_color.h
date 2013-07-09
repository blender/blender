/*
 * Copyright 2011, Blender Foundation.
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

#ifndef __UTIL_COLOR_H__
#define __UTIL_COLOR_H__

#include "util_math.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

__device float color_srgb_to_scene_linear(float c)
{
	if(c < 0.04045f)
		return (c < 0.0f)? 0.0f: c * (1.0f/12.92f);
	else
		return powf((c + 0.055f) * (1.0f / 1.055f), 2.4f);
}

__device float color_scene_linear_to_srgb(float c)
{
	if(c < 0.0031308f)
		return (c < 0.0f)? 0.0f: c * 12.92f;
	else
		return 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

__device float3 rgb_to_hsv(float3 rgb)
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

__device float3 hsv_to_rgb(float3 hsv)
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

__device float3 xyY_to_xyz(float x, float y, float Y)
{
	float X, Z;

	if(y != 0.0f) X = (x / y) * Y;
	else X = 0.0f;

	if(y != 0.0f && Y != 0.0f) Z = (1.0f - x - y) / y * Y;
	else Z = 0.0f;

	return make_float3(X, Y, Z);
}

__device float3 xyz_to_rgb(float x, float y, float z)
{
	return make_float3(3.240479f * x + -1.537150f * y + -0.498535f * z,
					  -0.969256f * x +  1.875991f * y +  0.041556f * z,
					   0.055648f * x + -0.204043f * y +  1.057311f * z);
}

#ifndef __KERNEL_OPENCL__

__device float3 color_srgb_to_scene_linear(float3 c)
{
	return make_float3(
		color_srgb_to_scene_linear(c.x),
		color_srgb_to_scene_linear(c.y),
		color_srgb_to_scene_linear(c.z));
}

__device float3 color_scene_linear_to_srgb(float3 c)
{
	return make_float3(
		color_scene_linear_to_srgb(c.x),
		color_scene_linear_to_srgb(c.y),
		color_scene_linear_to_srgb(c.z));
}

#endif

__device float linear_rgb_to_gray(float3 c)
{
	return c.x*0.2126f + c.y*0.7152f + c.z*0.0722f;
}

CCL_NAMESPACE_END

#endif /* __UTIL_COLOR_H__ */

