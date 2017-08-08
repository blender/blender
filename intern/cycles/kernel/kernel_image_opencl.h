/*
 * Copyright 2016 Blender Foundation
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
 * limitations under the License.
 */


/* For OpenCL we do manual lookup and interpolation. */

ccl_device_inline ccl_global tex_info_t* kernel_tex_info(KernelGlobals *kg, uint id) {
	const uint tex_offset = id
#define KERNEL_TEX(type, ttype, name) + 1
#include "kernel/kernel_textures.h"
	;

	return &((ccl_global tex_info_t*)kg->buffers[0])[tex_offset];
}

#define tex_fetch(type, info, index) ((ccl_global type*)(kg->buffers[info->buffer] + info->offset))[(index)]

ccl_device_inline float4 svm_image_texture_read(KernelGlobals *kg, int id, int offset)
{
	const ccl_global tex_info_t *info = kernel_tex_info(kg, id);
	const int texture_type = kernel_tex_type(id);

	/* Float4 */
	if(texture_type == IMAGE_DATA_TYPE_FLOAT4) {
		return tex_fetch(float4, info, offset);
	}
	/* Byte4 */
	else if(texture_type == IMAGE_DATA_TYPE_BYTE4) {
		uchar4 r = tex_fetch(uchar4, info, offset);
		float f = 1.0f/255.0f;
		return make_float4(r.x*f, r.y*f, r.z*f, r.w*f);
	}
	/* Float */
	else if(texture_type == IMAGE_DATA_TYPE_FLOAT) {
		float f = tex_fetch(float, info, offset);
		return make_float4(f, f, f, 1.0f);
	}
	/* Byte */
	else {
		uchar r = tex_fetch(uchar, info, offset);
		float f = r * (1.0f/255.0f);
		return make_float4(f, f, f, 1.0f);
	}
}

ccl_device_inline int svm_image_texture_wrap_periodic(int x, int width)
{
	x %= width;
	if(x < 0)
		x += width;
	return x;
}

ccl_device_inline int svm_image_texture_wrap_clamp(int x, int width)
{
	return clamp(x, 0, width-1);
}

ccl_device_inline float svm_image_texture_frac(float x, int *ix)
{
	int i = float_to_int(x) - ((x < 0.0f)? 1: 0);
	*ix = i;
	return x - (float)i;
}

ccl_device_inline uint kernel_decode_image_interpolation(uint info)
{
	return (info & (1 << 0)) ? INTERPOLATION_CLOSEST : INTERPOLATION_LINEAR;
}

ccl_device_inline uint kernel_decode_image_extension(uint info)
{
	if(info & (1 << 1)) {
		return EXTENSION_REPEAT;
	}
	else if(info & (1 << 2)) {
		return EXTENSION_EXTEND;
	}
	else {
		return EXTENSION_CLIP;
	}
}

ccl_device float4 kernel_tex_image_interp(KernelGlobals *kg, int id, float x, float y)
{
	const ccl_global tex_info_t *info = kernel_tex_info(kg, id);

	uint width = info->width;
	uint height = info->height;
	uint offset = 0;

	/* Decode image options. */
	uint interpolation = kernel_decode_image_interpolation(info->options);
	uint extension = kernel_decode_image_extension(info->options);

	/* Actual sampling. */
	float4 r;
	int ix, iy, nix, niy;
	if(interpolation == INTERPOLATION_CLOSEST) {
		svm_image_texture_frac(x*width, &ix);
		svm_image_texture_frac(y*height, &iy);

		if(extension == EXTENSION_REPEAT) {
			ix = svm_image_texture_wrap_periodic(ix, width);
			iy = svm_image_texture_wrap_periodic(iy, height);
		}
		else {
			if(extension == EXTENSION_CLIP) {
				if(x < 0.0f || y < 0.0f || x > 1.0f || y > 1.0f) {
					return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
				}
			}
			/* Fall through. */
			/* EXTENSION_EXTEND */
			ix = svm_image_texture_wrap_clamp(ix, width);
			iy = svm_image_texture_wrap_clamp(iy, height);
		}

		r = svm_image_texture_read(kg, id, offset + ix + iy*width);
	}
	else { /* INTERPOLATION_LINEAR */
		float tx = svm_image_texture_frac(x*width - 0.5f, &ix);
		float ty = svm_image_texture_frac(y*height - 0.5f, &iy);

		if(extension == EXTENSION_REPEAT) {
			ix = svm_image_texture_wrap_periodic(ix, width);
			iy = svm_image_texture_wrap_periodic(iy, height);

			nix = svm_image_texture_wrap_periodic(ix+1, width);
			niy = svm_image_texture_wrap_periodic(iy+1, height);
		}
		else {
			if(extension == EXTENSION_CLIP) {
				if(x < 0.0f || y < 0.0f || x > 1.0f || y > 1.0f) {
					return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
				}
			}
			nix = svm_image_texture_wrap_clamp(ix+1, width);
			niy = svm_image_texture_wrap_clamp(iy+1, height);
			ix = svm_image_texture_wrap_clamp(ix, width);
			iy = svm_image_texture_wrap_clamp(iy, height);
		}

		r = (1.0f - ty)*(1.0f - tx)*svm_image_texture_read(kg, id, offset + ix + iy*width);
		r += (1.0f - ty)*tx*svm_image_texture_read(kg, id, offset + nix + iy*width);
		r += ty*(1.0f - tx)*svm_image_texture_read(kg, id, offset + ix + niy*width);
		r += ty*tx*svm_image_texture_read(kg, id, offset + nix + niy*width);
	}
	return r;
}


ccl_device float4 kernel_tex_image_interp_3d(KernelGlobals *kg, int id, float x, float y, float z)
{
	const ccl_global tex_info_t *info = kernel_tex_info(kg, id);

	uint width = info->width;
	uint height = info->height;
	uint offset = 0;
	uint depth = info->depth;

	/* Decode image options. */
	uint interpolation = kernel_decode_image_interpolation(info->options);
	uint extension = kernel_decode_image_extension(info->options);

	/* Actual sampling. */
	float4 r;
	int ix, iy, iz, nix, niy, niz;
	if(interpolation == INTERPOLATION_CLOSEST) {
		svm_image_texture_frac(x*width, &ix);
		svm_image_texture_frac(y*height, &iy);
		svm_image_texture_frac(z*depth, &iz);

		if(extension == EXTENSION_REPEAT) {
			ix = svm_image_texture_wrap_periodic(ix, width);
			iy = svm_image_texture_wrap_periodic(iy, height);
			iz = svm_image_texture_wrap_periodic(iz, depth);
		}
		else {
			if(extension == EXTENSION_CLIP) {
				if(x < 0.0f || y < 0.0f || z < 0.0f ||
				   x > 1.0f || y > 1.0f || z > 1.0f)
				{
					return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
				}
			}
			/* Fall through. */
			/* EXTENSION_EXTEND */
			ix = svm_image_texture_wrap_clamp(ix, width);
			iy = svm_image_texture_wrap_clamp(iy, height);
			iz = svm_image_texture_wrap_clamp(iz, depth);
		}
		r = svm_image_texture_read(kg, id, offset + ix + iy*width + iz*width*height);
	}
	else { /* INTERPOLATION_LINEAR */
		float tx = svm_image_texture_frac(x*(float)width - 0.5f, &ix);
		float ty = svm_image_texture_frac(y*(float)height - 0.5f, &iy);
		float tz = svm_image_texture_frac(z*(float)depth - 0.5f, &iz);

		if(extension == EXTENSION_REPEAT) {
			ix = svm_image_texture_wrap_periodic(ix, width);
			iy = svm_image_texture_wrap_periodic(iy, height);
			iz = svm_image_texture_wrap_periodic(iz, depth);

			nix = svm_image_texture_wrap_periodic(ix+1, width);
			niy = svm_image_texture_wrap_periodic(iy+1, height);
			niz = svm_image_texture_wrap_periodic(iz+1, depth);
		}
		else {
			if(extension == EXTENSION_CLIP) {
				if(x < 0.0f || y < 0.0f || z < 0.0f ||
				   x > 1.0f || y > 1.0f || z > 1.0f)
				{
					return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
				}
			}
			/* Fall through. */
			/*  EXTENSION_EXTEND */
			nix = svm_image_texture_wrap_clamp(ix+1, width);
			niy = svm_image_texture_wrap_clamp(iy+1, height);
			niz = svm_image_texture_wrap_clamp(iz+1, depth);

			ix = svm_image_texture_wrap_clamp(ix, width);
			iy = svm_image_texture_wrap_clamp(iy, height);
			iz = svm_image_texture_wrap_clamp(iz, depth);
		}

		r  = (1.0f - tz)*(1.0f - ty)*(1.0f - tx)*svm_image_texture_read(kg, id, offset + ix + iy*width + iz*width*height);
		r += (1.0f - tz)*(1.0f - ty)*tx*svm_image_texture_read(kg, id, offset + nix + iy*width + iz*width*height);
		r += (1.0f - tz)*ty*(1.0f - tx)*svm_image_texture_read(kg, id, offset + ix + niy*width + iz*width*height);
		r += (1.0f - tz)*ty*tx*svm_image_texture_read(kg, id, offset + nix + niy*width + iz*width*height);

		r += tz*(1.0f - ty)*(1.0f - tx)*svm_image_texture_read(kg, id, offset + ix + iy*width + niz*width*height);
		r += tz*(1.0f - ty)*tx*svm_image_texture_read(kg, id, offset + nix + iy*width + niz*width*height);
		r += tz*ty*(1.0f - tx)*svm_image_texture_read(kg, id, offset + ix + niy*width + niz*width*height);
		r += tz*ty*tx*svm_image_texture_read(kg, id, offset + nix + niy*width + niz*width*height);
	}
	return r;
}
