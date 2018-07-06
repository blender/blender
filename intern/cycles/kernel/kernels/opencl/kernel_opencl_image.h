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

ccl_device_inline ccl_global TextureInfo* kernel_tex_info(KernelGlobals *kg, uint id) {
	const uint tex_offset = id
#define KERNEL_TEX(type, name) + 1
#include "kernel/kernel_textures.h"
	;

	return &((ccl_global TextureInfo*)kg->buffers[0])[tex_offset];
}

#define tex_fetch(type, info, index) ((ccl_global type*)(kg->buffers[info->cl_buffer] + info->data))[(index)]

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

ccl_device_inline float4 svm_image_texture_read(KernelGlobals *kg, const ccl_global TextureInfo *info, int id, int offset)
{
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
	/* Ushort4 */
	else if(texture_type == IMAGE_DATA_TYPE_USHORT4) {
		ushort4 r = tex_fetch(ushort4, info, offset);
		float f = 1.0f/65535.f;
		return make_float4(r.x*f, r.y*f, r.z*f, r.w*f);
	}
	/* Float */
	else if(texture_type == IMAGE_DATA_TYPE_FLOAT) {
		float f = tex_fetch(float, info, offset);
		return make_float4(f, f, f, 1.0f);
	}
	/* UShort */
	else if(texture_type == IMAGE_DATA_TYPE_USHORT) {
		ushort r = tex_fetch(ushort, info, offset);
		float f = r * (1.0f / 65535.0f);
		return make_float4(f, f, f, 1.0f);
	}
	/* Byte */
#ifdef cl_khr_fp16
	/* half and half4 are optional in OpenCL */
	else if(texture_type == IMAGE_DATA_TYPE_HALF) {
		float f = tex_fetch(half, info, offset);
		return make_float4(f, f, f, 1.0f);
	}
	else if(texture_type == IMAGE_DATA_TYPE_HALF4) {
		half4 r = tex_fetch(half4, info, offset);
		return make_float4(r.x, r.y, r.z, r.w);
	}
#endif
	else {
		uchar r = tex_fetch(uchar, info, offset);
		float f = r * (1.0f/255.0f);
		return make_float4(f, f, f, 1.0f);
	}
}

ccl_device_inline float4 svm_image_texture_read_2d(KernelGlobals *kg, int id, int x, int y)
{
	const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

	/* Wrap */
	if(info->extension == EXTENSION_REPEAT) {
		x = svm_image_texture_wrap_periodic(x, info->width);
		y = svm_image_texture_wrap_periodic(y, info->height);
	}
	else {
		x = svm_image_texture_wrap_clamp(x, info->width);
		y = svm_image_texture_wrap_clamp(y, info->height);
	}

	int offset = x + info->width * y;
	return svm_image_texture_read(kg, info, id, offset);
}

ccl_device_inline float4 svm_image_texture_read_3d(KernelGlobals *kg, int id, int x, int y, int z)
{
	const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

	/* Wrap */
	if(info->extension == EXTENSION_REPEAT) {
		x = svm_image_texture_wrap_periodic(x, info->width);
		y = svm_image_texture_wrap_periodic(y, info->height);
		z = svm_image_texture_wrap_periodic(z, info->depth);
	}
	else {
		x = svm_image_texture_wrap_clamp(x, info->width);
		y = svm_image_texture_wrap_clamp(y, info->height);
		z = svm_image_texture_wrap_clamp(z, info->depth);
	}

	int offset = x + info->width * y + info->width * info->height * z;
	return svm_image_texture_read(kg, info, id, offset);
}


ccl_device_inline float svm_image_texture_frac(float x, int *ix)
{
	int i = float_to_int(x) - ((x < 0.0f)? 1: 0);
	*ix = i;
	return x - (float)i;
}

#define SET_CUBIC_SPLINE_WEIGHTS(u, t) \
	{ \
		u[0] = (((-1.0f/6.0f)* t + 0.5f) * t - 0.5f) * t + (1.0f/6.0f); \
		u[1] =  ((      0.5f * t - 1.0f) * t       ) * t + (2.0f/3.0f); \
		u[2] =  ((     -0.5f * t + 0.5f) * t + 0.5f) * t + (1.0f/6.0f); \
		u[3] = (1.0f / 6.0f) * t * t * t; \
	} (void)0

ccl_device float4 kernel_tex_image_interp(KernelGlobals *kg, int id, float x, float y)
{
	const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

	if(info->extension == EXTENSION_CLIP) {
		if(x < 0.0f || y < 0.0f || x > 1.0f || y > 1.0f) {
			return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}

	if(info->interpolation == INTERPOLATION_CLOSEST) {
		/* Closest interpolation. */
		int ix, iy;
		svm_image_texture_frac(x*info->width, &ix);
		svm_image_texture_frac(y*info->height, &iy);

		return svm_image_texture_read_2d(kg, id, ix, iy);
	}
	else if(info->interpolation == INTERPOLATION_LINEAR) {
		/* Bilinear interpolation. */
		int ix, iy;
		float tx = svm_image_texture_frac(x*info->width - 0.5f, &ix);
		float ty = svm_image_texture_frac(y*info->height - 0.5f, &iy);

		float4 r;
		r =  (1.0f - ty)*(1.0f - tx)*svm_image_texture_read_2d(kg, id, ix, iy);
		r += (1.0f - ty)*tx*svm_image_texture_read_2d(kg, id, ix+1, iy);
		r += ty*(1.0f - tx)*svm_image_texture_read_2d(kg, id, ix, iy+1);
		r += ty*tx*svm_image_texture_read_2d(kg, id, ix+1, iy+1);
		return r;
	}
	else {
		/* Bicubic interpolation. */
		int ix, iy;
		float tx = svm_image_texture_frac(x*info->width - 0.5f, &ix);
		float ty = svm_image_texture_frac(y*info->height - 0.5f, &iy);

		float u[4], v[4];
		SET_CUBIC_SPLINE_WEIGHTS(u, tx);
		SET_CUBIC_SPLINE_WEIGHTS(v, ty);

		float4 r = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

		for(int y = 0; y < 4; y++) {
			for(int x = 0; x < 4; x++) {
				float weight = u[x]*v[y];
				r += weight*svm_image_texture_read_2d(kg, id, ix+x-1, iy+y-1);
			}
		}
		return r;
	}
}


ccl_device float4 kernel_tex_image_interp_3d(KernelGlobals *kg, int id, float x, float y, float z, int interp)
{
	const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

	if(info->extension == EXTENSION_CLIP) {
		if(x < 0.0f || y < 0.0f || z < 0.0f ||
		   x > 1.0f || y > 1.0f || z > 1.0f)
		{
			return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}

	uint interpolation = (interp == INTERPOLATION_NONE)? info->interpolation: interp;

	if(interpolation == INTERPOLATION_CLOSEST) {
		/* Closest interpolation. */
		int ix, iy, iz;
		svm_image_texture_frac(x*info->width, &ix);
		svm_image_texture_frac(y*info->height, &iy);
		svm_image_texture_frac(z*info->depth, &iz);

		return svm_image_texture_read_3d(kg, id, ix, iy, iz);
	}
	else if(interpolation == INTERPOLATION_LINEAR) {
		/* Bilinear interpolation. */
		int ix, iy, iz;
		float tx = svm_image_texture_frac(x*info->width - 0.5f, &ix);
		float ty = svm_image_texture_frac(y*info->height - 0.5f, &iy);
		float tz = svm_image_texture_frac(z*info->depth - 0.5f, &iz);

		float4 r;
		r  = (1.0f - tz)*(1.0f - ty)*(1.0f - tx)*svm_image_texture_read_3d(kg, id, ix, iy, iz);
		r += (1.0f - tz)*(1.0f - ty)*tx*svm_image_texture_read_3d(kg, id, ix+1, iy, iz);
		r += (1.0f - tz)*ty*(1.0f - tx)*svm_image_texture_read_3d(kg, id, ix, iy+1, iz);
		r += (1.0f - tz)*ty*tx*svm_image_texture_read_3d(kg, id, ix+1, iy+1, iz);

		r += tz*(1.0f - ty)*(1.0f - tx)*svm_image_texture_read_3d(kg, id, ix, iy, iz+1);
		r += tz*(1.0f - ty)*tx*svm_image_texture_read_3d(kg, id, ix+1, iy, iz+1);
		r += tz*ty*(1.0f - tx)*svm_image_texture_read_3d(kg, id, ix, iy+1, iz+1);
		r += tz*ty*tx*svm_image_texture_read_3d(kg, id, ix+1, iy+1, iz+1);
		return r;
	}
	else {
		/* Bicubic interpolation. */
		int ix, iy, iz;
		float tx = svm_image_texture_frac(x*info->width - 0.5f, &ix);
		float ty = svm_image_texture_frac(y*info->height - 0.5f, &iy);
		float tz = svm_image_texture_frac(z*info->depth - 0.5f, &iz);

		float u[4], v[4], w[4];
		SET_CUBIC_SPLINE_WEIGHTS(u, tx);
		SET_CUBIC_SPLINE_WEIGHTS(v, ty);
		SET_CUBIC_SPLINE_WEIGHTS(w, tz);

		float4 r = make_float4(0.0f, 0.0f, 0.0f, 0.0f);

		for(int z = 0; z < 4; z++) {
			for(int y = 0; y < 4; y++) {
				for(int x = 0; x < 4; x++) {
					float weight = u[x]*v[y]*w[z];
					r += weight*svm_image_texture_read_3d(kg, id, ix+x-1, iy+y-1, iz+z-1);
				}
			}
		}
		return r;
	}
}

#undef SET_CUBIC_SPLINE_WEIGHTS
