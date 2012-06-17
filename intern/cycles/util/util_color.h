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

