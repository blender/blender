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


/* For OpenCL all images are packed in a single array, and we do manual lookup
 * and interpolation. */

ccl_device_inline float4 svm_image_texture_read(KernelGlobals *kg, int id, int offset)
{
	/* Float4 */
	if(id < TEX_START_BYTE4_OPENCL) {
		return kernel_tex_fetch(__tex_image_float4_packed, offset);
	}
	/* Byte4 */
	else if(id < TEX_START_FLOAT_OPENCL) {
		uchar4 r = kernel_tex_fetch(__tex_image_byte4_packed, offset);
		float f = 1.0f/255.0f;
		return make_float4(r.x*f, r.y*f, r.z*f, r.w*f);
	}
	/* Float */
	else if(id < TEX_START_BYTE_OPENCL) {
		float f = kernel_tex_fetch(__tex_image_float_packed, offset);
		return make_float4(f, f, f, 1.0f);
	}
	/* Byte */
	else {
		uchar r = kernel_tex_fetch(__tex_image_byte_packed, offset);
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

ccl_device float4 kernel_tex_image_interp(KernelGlobals *kg, int id, float x, float y)
{
	uint4 info = kernel_tex_fetch(__tex_image_packed_info, id*2);
	uint width = info.x;
	uint height = info.y;
	uint offset = info.z;

	/* Image Options */
	uint interpolation = (info.w & (1 << 0)) ? INTERPOLATION_CLOSEST : INTERPOLATION_LINEAR;
	uint extension;
	if(info.w & (1 << 1))
		extension = EXTENSION_REPEAT;
	else if(info.w & (1 << 2))
		extension = EXTENSION_EXTEND;
	else
		extension = EXTENSION_CLIP;

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
	uint4 info = kernel_tex_fetch(__tex_image_packed_info, id*2);
	uint width = info.x;
	uint height = info.y;
	uint offset = info.z;
	uint depth = kernel_tex_fetch(__tex_image_packed_info, id*2+1).x;

	/* Image Options */
	uint interpolation = (info.w & (1 << 0)) ? INTERPOLATION_CLOSEST : INTERPOLATION_LINEAR;
	uint extension;
	if(info.w & (1 << 1))
		extension = EXTENSION_REPEAT;
	else if(info.w & (1 << 2))
		extension = EXTENSION_EXTEND;
	else
		extension = EXTENSION_CLIP;

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
			if(extension == EXTENSION_CLIP)
				if(x < 0.0f || y < 0.0f || z < 0.0f ||
				   x > 1.0f || y > 1.0f || z > 1.0f)
				{
					return make_float4(0.0f, 0.0f, 0.0f, 0.0f);
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
