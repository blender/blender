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

ccl_device_inline float4 svm_image_texture_read(KernelGlobals *kg, int id, int offset)
{
	const ccl_global TextureInfo *info = kernel_tex_info(kg, id);
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

	uint width = info->width;
	uint height = info->height;
	uint interpolation = info->interpolation;
	uint extension = info->extension;

	/* Actual sampling. */
	if(interpolation == INTERPOLATION_CLOSEST) {
		int ix, iy;
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

		return svm_image_texture_read(kg, id, ix + iy*width);
	}
	else {
		/* Bilinear or bicubic interpolation. */
		int ix, iy, nix, niy;
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
			ix = svm_image_texture_wrap_clamp(ix, width);
			iy = svm_image_texture_wrap_clamp(iy, height);
			nix = svm_image_texture_wrap_clamp(ix+1, width);
			niy = svm_image_texture_wrap_clamp(iy+1, height);
		}

		if(interpolation == INTERPOLATION_LINEAR) {
			/* Bilinear interpolation. */
			float4 r;
			r = (1.0f - ty)*(1.0f - tx)*svm_image_texture_read(kg, id, ix + iy*width);
			r += (1.0f - ty)*tx*svm_image_texture_read(kg, id, nix + iy*width);
			r += ty*(1.0f - tx)*svm_image_texture_read(kg, id, ix + niy*width);
			r += ty*tx*svm_image_texture_read(kg, id, nix + niy*width);
			return r;
		}

		/* Bicubic interpolation. */
		int pix, piy, nnix, nniy;
		if(extension == EXTENSION_REPEAT) {
			pix = svm_image_texture_wrap_periodic(ix-1, width);
			piy = svm_image_texture_wrap_periodic(iy-1, height);
			nnix = svm_image_texture_wrap_periodic(ix+2, width);
			nniy = svm_image_texture_wrap_periodic(iy+2, height);
		}
		else {
			pix = svm_image_texture_wrap_clamp(ix-1, width);
			piy = svm_image_texture_wrap_clamp(iy-1, height);
			nnix = svm_image_texture_wrap_clamp(ix+2, width);
			nniy = svm_image_texture_wrap_clamp(iy+2, height);
		}

		const int xc[4] = {pix, ix, nix, nnix};
		const int yc[4] = {width * piy,
		                   width * iy,
		                   width * niy,
		                   width * nniy};
		float u[4], v[4];
		/* Some helper macro to keep code reasonable size,
		 * let compiler to inline all the matrix multiplications.
		 */
#define DATA(x, y) (svm_image_texture_read(kg, id, xc[x] + yc[y]))
#define TERM(col) \
		(v[col] * (u[0] * DATA(0, col) + \
		           u[1] * DATA(1, col) + \
		           u[2] * DATA(2, col) + \
		           u[3] * DATA(3, col)))

		SET_CUBIC_SPLINE_WEIGHTS(u, tx);
		SET_CUBIC_SPLINE_WEIGHTS(v, ty);

		/* Actual interpolation. */
		return TERM(0) + TERM(1) + TERM(2) + TERM(3);
#undef TERM
#undef DATA
	}
}


ccl_device float4 kernel_tex_image_interp_3d(KernelGlobals *kg, int id, float x, float y, float z, int interp)
{
	const ccl_global TextureInfo *info = kernel_tex_info(kg, id);

	uint width = info->width;
	uint height = info->height;
	uint depth = info->depth;
	uint interpolation = (interp == INTERPOLATION_NONE)? info->interpolation: interp;
	uint extension = info->extension;

	/* Actual sampling. */
	if(interpolation == INTERPOLATION_CLOSEST) {
		int ix, iy, iz;
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
		return svm_image_texture_read(kg, id, ix + iy*width + iz*width*height);
	}
	else {
		/* Bilinear or bicubic interpolation. */
		int ix, iy, iz, nix, niy, niz;
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

		if(interpolation == INTERPOLATION_LINEAR) {
			/* Bilinear interpolation. */
			float4 r;
			r  = (1.0f - tz)*(1.0f - ty)*(1.0f - tx)*svm_image_texture_read(kg, id, ix + iy*width + iz*width*height);
			r += (1.0f - tz)*(1.0f - ty)*tx*svm_image_texture_read(kg, id, nix + iy*width + iz*width*height);
			r += (1.0f - tz)*ty*(1.0f - tx)*svm_image_texture_read(kg, id, ix + niy*width + iz*width*height);
			r += (1.0f - tz)*ty*tx*svm_image_texture_read(kg, id, nix + niy*width + iz*width*height);

			r += tz*(1.0f - ty)*(1.0f - tx)*svm_image_texture_read(kg, id, ix + iy*width + niz*width*height);
			r += tz*(1.0f - ty)*tx*svm_image_texture_read(kg, id, nix + iy*width + niz*width*height);
			r += tz*ty*(1.0f - tx)*svm_image_texture_read(kg, id, ix + niy*width + niz*width*height);
			r += tz*ty*tx*svm_image_texture_read(kg, id, nix + niy*width + niz*width*height);
			return r;
		}

		/* Bicubic interpolation. */
		int pix, piy, piz, nnix, nniy, nniz;
		if(extension == EXTENSION_REPEAT) {
			pix = svm_image_texture_wrap_periodic(ix-1, width);
			piy = svm_image_texture_wrap_periodic(iy-1, height);
			piz = svm_image_texture_wrap_periodic(iz-1, depth);
			nnix = svm_image_texture_wrap_periodic(ix+2, width);
			nniy = svm_image_texture_wrap_periodic(iy+2, height);
			nniz = svm_image_texture_wrap_periodic(iz+2, depth);
		}
		else {
			pix = svm_image_texture_wrap_clamp(ix-1, width);
			piy = svm_image_texture_wrap_clamp(iy-1, height);
			piz = svm_image_texture_wrap_clamp(iz-1, depth);
			nnix = svm_image_texture_wrap_clamp(ix+2, width);
			nniy = svm_image_texture_wrap_clamp(iy+2, height);
			nniz = svm_image_texture_wrap_clamp(iz+2, depth);
		}

		const int xc[4] = {pix, ix, nix, nnix};
		const int yc[4] = {width * piy,
		                   width * iy,
		                   width * niy,
		                   width * nniy};
		const int zc[4] = {width * height * piz,
		                   width * height * iz,
		                   width * height * niz,
		                   width * height * nniz};
		float u[4], v[4], w[4];

		/* Some helper macro to keep code reasonable size,
		 * let compiler to inline all the matrix multiplications.
		 */
#define DATA(x, y, z) (svm_image_texture_read(kg, id, xc[x] + yc[y] + zc[z]))
#define COL_TERM(col, row) \
		(v[col] * (u[0] * DATA(0, col, row) + \
		           u[1] * DATA(1, col, row) + \
		           u[2] * DATA(2, col, row) + \
		           u[3] * DATA(3, col, row)))
#define ROW_TERM(row) \
		(w[row] * (COL_TERM(0, row) + \
		           COL_TERM(1, row) + \
		           COL_TERM(2, row) + \
		           COL_TERM(3, row)))

		SET_CUBIC_SPLINE_WEIGHTS(u, tx);
		SET_CUBIC_SPLINE_WEIGHTS(v, ty);
		SET_CUBIC_SPLINE_WEIGHTS(w, tz);

		/* Actual interpolation. */
		return ROW_TERM(0) + ROW_TERM(1) + ROW_TERM(2) + ROW_TERM(3);

#undef COL_TERM
#undef ROW_TERM
#undef DATA
	}
}

#undef SET_CUBIC_SPLINE_WEIGHTS
