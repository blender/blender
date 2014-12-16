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

#ifndef __KERNEL_COMPAT_CPU_H__
#define __KERNEL_COMPAT_CPU_H__

#define __KERNEL_CPU__

#include "util_debug.h"
#include "util_math.h"
#include "util_simd.h"
#include "util_half.h"
#include "util_types.h"

/* On x86_64, versions of glibc < 2.16 have an issue where expf is
 * much slower than the double version.  This was fixed in glibc 2.16.
 */
#if !defined(__KERNEL_GPU__)  && defined(__x86_64__) && defined(__x86_64__) && \
     defined(__GNU_LIBRARY__) && defined(__GLIBC__ ) && defined(__GLIBC_MINOR__) && \
     (__GLIBC__ <= 2 && __GLIBC_MINOR__ < 16)
#  define expf(x) ((float)exp((double)(x)))
#endif

CCL_NAMESPACE_BEGIN

/* Assertions inside the kernel only work for the CPU device, so we wrap it in
 * a macro which is empty for other devices */

#define kernel_assert(cond) assert(cond)

/* Texture types to be compatible with CUDA textures. These are really just
 * simple arrays and after inlining fetch hopefully revert to being a simple
 * pointer lookup. */

template<typename T> struct texture  {
	ccl_always_inline T fetch(int index)
	{
		kernel_assert(index >= 0 && index < width);
		return data[index];
	}

	ccl_always_inline ssef fetch_ssef(int index)
	{
		kernel_assert(index >= 0 && index < width);
		return ((ssef*)data)[index];
	}

	ccl_always_inline ssei fetch_ssei(int index)
	{
		kernel_assert(index >= 0 && index < width);
		return ((ssei*)data)[index];
	}

	T *data;
	int width;
};

template<typename T> struct texture_image  {
	ccl_always_inline float4 read(float4 r)
	{
		return r;
	}

	ccl_always_inline float4 read(uchar4 r)
	{
		float f = 1.0f/255.0f;
		return make_float4(r.x*f, r.y*f, r.z*f, r.w*f);
	}

	ccl_always_inline int wrap_periodic(int x, int width)
	{
		x %= width;
		if(x < 0)
			x += width;
		return x;
	}

	ccl_always_inline int wrap_clamp(int x, int width)
	{
		return clamp(x, 0, width-1);
	}

	ccl_always_inline float frac(float x, int *ix)
	{
		int i = float_to_int(x) - ((x < 0.0f)? 1: 0);
		*ix = i;
		return x - (float)i;
	}

	ccl_always_inline float4 interp(float x, float y, bool periodic = true)
	{
		if(UNLIKELY(!data))
			return make_float4(0.0f, 0.0f, 0.0f, 0.0f);

		int ix, iy, nix, niy;

		if(interpolation == INTERPOLATION_CLOSEST) {
			frac(x*(float)width, &ix);
			frac(y*(float)height, &iy);
			if(periodic) {
				ix = wrap_periodic(ix, width);
				iy = wrap_periodic(iy, height);

			}
			else {
				ix = wrap_clamp(ix, width);
				iy = wrap_clamp(iy, height);
			}
			return read(data[ix + iy*width]);
		}
		else {
			float tx = frac(x*(float)width - 0.5f, &ix);
			float ty = frac(y*(float)height - 0.5f, &iy);

			if(periodic) {
				ix = wrap_periodic(ix, width);
				iy = wrap_periodic(iy, height);

				nix = wrap_periodic(ix+1, width);
				niy = wrap_periodic(iy+1, height);
			}
			else {
				ix = wrap_clamp(ix, width);
				iy = wrap_clamp(iy, height);

				nix = wrap_clamp(ix+1, width);
				niy = wrap_clamp(iy+1, height);
			}

			float4 r = (1.0f - ty)*(1.0f - tx)*read(data[ix + iy*width]);
			r += (1.0f - ty)*tx*read(data[nix + iy*width]);
			r += ty*(1.0f - tx)*read(data[ix + niy*width]);
			r += ty*tx*read(data[nix + niy*width]);

			return r;
		}
	}

	ccl_always_inline float4 interp_3d(float x, float y, float z, bool periodic = false)
	{
		return interp_3d_ex(x, y, z, interpolation, periodic);
	}

	ccl_always_inline float4 interp_3d_ex(float x, float y, float z,
	                                      int interpolation = INTERPOLATION_LINEAR,
	                                      bool periodic = false)
	{
		if(UNLIKELY(!data))
			return make_float4(0.0f, 0.0f, 0.0f, 0.0f);

		int ix, iy, iz, nix, niy, niz;

		if(interpolation == INTERPOLATION_CLOSEST) {
			frac(x*(float)width, &ix);
			frac(y*(float)height, &iy);
			frac(z*(float)depth, &iz);

			if(periodic) {
				ix = wrap_periodic(ix, width);
				iy = wrap_periodic(iy, height);
				iz = wrap_periodic(iz, depth);
			}
			else {
				ix = wrap_clamp(ix, width);
				iy = wrap_clamp(iy, height);
				iz = wrap_clamp(iz, depth);
			}

			return read(data[ix + iy*width + iz*width*height]);
		}
		else if(interpolation == INTERPOLATION_LINEAR) {
			float tx = frac(x*(float)width - 0.5f, &ix);
			float ty = frac(y*(float)height - 0.5f, &iy);
			float tz = frac(z*(float)depth - 0.5f, &iz);

			if(periodic) {
				ix = wrap_periodic(ix, width);
				iy = wrap_periodic(iy, height);
				iz = wrap_periodic(iz, depth);

				nix = wrap_periodic(ix+1, width);
				niy = wrap_periodic(iy+1, height);
				niz = wrap_periodic(iz+1, depth);
			}
			else {
				ix = wrap_clamp(ix, width);
				iy = wrap_clamp(iy, height);
				iz = wrap_clamp(iz, depth);

				nix = wrap_clamp(ix+1, width);
				niy = wrap_clamp(iy+1, height);
				niz = wrap_clamp(iz+1, depth);
			}

			float4 r;

			r  = (1.0f - tz)*(1.0f - ty)*(1.0f - tx)*read(data[ix + iy*width + iz*width*height]);
			r += (1.0f - tz)*(1.0f - ty)*tx*read(data[nix + iy*width + iz*width*height]);
			r += (1.0f - tz)*ty*(1.0f - tx)*read(data[ix + niy*width + iz*width*height]);
			r += (1.0f - tz)*ty*tx*read(data[nix + niy*width + iz*width*height]);

			r += tz*(1.0f - ty)*(1.0f - tx)*read(data[ix + iy*width + niz*width*height]);
			r += tz*(1.0f - ty)*tx*read(data[nix + iy*width + niz*width*height]);
			r += tz*ty*(1.0f - tx)*read(data[ix + niy*width + niz*width*height]);
			r += tz*ty*tx*read(data[nix + niy*width + niz*width*height]);

			return r;
		}
		else {
			/* Tricubic b-spline interpolation. */
			const float tx = frac(x*(float)width - 0.5f, &ix);
			const float ty = frac(y*(float)height - 0.5f, &iy);
			const float tz = frac(z*(float)depth - 0.5f, &iz);
			int pix, piy, piz, nnix, nniy, nniz;

			if(periodic) {
				ix = wrap_periodic(ix, width);
				iy = wrap_periodic(iy, height);
				iz = wrap_periodic(iz, depth);

				pix = wrap_periodic(ix-1, width);
				piy = wrap_periodic(iy-1, height);
				piz = wrap_periodic(iz-1, depth);

				nix = wrap_periodic(ix+1, width);
				niy = wrap_periodic(iy+1, height);
				niz = wrap_periodic(iz+1, depth);

				nnix = wrap_periodic(ix+2, width);
				nniy = wrap_periodic(iy+2, height);
				nniz = wrap_periodic(iz+2, depth);
			}
			else {
				ix = wrap_clamp(ix, width);
				iy = wrap_clamp(iy, height);
				iz = wrap_clamp(iz, depth);

				pix = wrap_clamp(ix-1, width);
				piy = wrap_clamp(iy-1, height);
				piz = wrap_clamp(iz-1, depth);

				nix = wrap_clamp(ix+1, width);
				niy = wrap_clamp(iy+1, height);
				niz = wrap_clamp(iz+1, depth);

				nnix = wrap_clamp(ix+2, width);
				nniy = wrap_clamp(iy+2, height);
				nniz = wrap_clamp(iz+2, depth);
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
#define SET_SPLINE_WEIGHTS(u, t) \
			{ \
				u[0] = (((-1.0f/6.0f)* t + 0.5f) * t - 0.5f) * t + (1.0f/6.0f); \
				u[1] =  ((      0.5f * t - 1.0f) * t       ) * t + (2.0f/3.0f); \
				u[2] =  ((     -0.5f * t + 0.5f) * t + 0.5f) * t + (1.0f/6.0f); \
				u[3] = (1.0f / 6.0f) * t * t * t; \
			} (void)0
#define DATA(x, y, z) (read(data[xc[x] + yc[y] + zc[z]]))
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

			SET_SPLINE_WEIGHTS(u, tx);
			SET_SPLINE_WEIGHTS(v, ty);
			SET_SPLINE_WEIGHTS(w, tz);

			/* Actual interpolation. */
			return ROW_TERM(0) + ROW_TERM(1) + ROW_TERM(2) + ROW_TERM(3);

#undef COL_TERM
#undef ROW_TERM
#undef DATA
#undef SET_SPLINE_WEIGHTS
		}
	}

	ccl_always_inline void dimensions_set(int width_, int height_, int depth_)
	{
		width = width_;
		height = height_;
		depth = depth_;
	}

	T *data;
	int interpolation;
	int width, height, depth;
};

typedef texture<float4> texture_float4;
typedef texture<float2> texture_float2;
typedef texture<float> texture_float;
typedef texture<uint> texture_uint;
typedef texture<int> texture_int;
typedef texture<uint4> texture_uint4;
typedef texture<uchar4> texture_uchar4;
typedef texture_image<float4> texture_image_float4;
typedef texture_image<uchar4> texture_image_uchar4;

/* Macros to handle different memory storage on different devices */

#define kernel_tex_fetch(tex, index) (kg->tex.fetch(index))
#define kernel_tex_fetch_ssef(tex, index) (kg->tex.fetch_ssef(index))
#define kernel_tex_fetch_ssei(tex, index) (kg->tex.fetch_ssei(index))
#define kernel_tex_lookup(tex, t, offset, size) (kg->tex.lookup(t, offset, size))
#define kernel_tex_image_interp(tex, x, y) ((tex < MAX_FLOAT_IMAGES) ? kg->texture_float_images[tex].interp(x, y) : kg->texture_byte_images[tex - MAX_FLOAT_IMAGES].interp(x, y))
#define kernel_tex_image_interp_3d(tex, x, y, z) ((tex < MAX_FLOAT_IMAGES) ? kg->texture_float_images[tex].interp_3d(x, y, z) : kg->texture_byte_images[tex - MAX_FLOAT_IMAGES].interp_3d(x, y, z))
#define kernel_tex_image_interp_3d_ex(tex, x, y, z, interpolation) ((tex < MAX_FLOAT_IMAGES) ? kg->texture_float_images[tex].interp_3d_ex(x, y, z, interpolation) : kg->texture_byte_images[tex - MAX_FLOAT_IMAGES].interp_3d_ex(x, y, z, interpolation))

#define kernel_data (kg->__data)

#ifdef __KERNEL_SSE2__
typedef vector3<sseb> sse3b;
typedef vector3<ssef> sse3f;
typedef vector3<ssei> sse3i;
#endif

CCL_NAMESPACE_END

#endif /* __KERNEL_COMPAT_CPU_H__ */

