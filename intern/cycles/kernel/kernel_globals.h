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
 * limitations under the License.
 */

/* Constant Globals */

CCL_NAMESPACE_BEGIN

/* On the CPU, we pass along the struct KernelGlobals to nearly everywhere in
 * the kernel, to access constant data. These are all stored as "textures", but
 * these are really just standard arrays. We can't use actually globals because
 * multiple renders may be running inside the same process. */

#ifdef __KERNEL_CPU__

#ifdef __OSL__
struct OSLGlobals;
struct OSLThreadData;
struct OSLShadingSystem;
#endif

#define MAX_BYTE_IMAGES   1024
#define MAX_FLOAT_IMAGES  1024

typedef struct KernelGlobals {
	texture_image_uchar4 texture_byte_images[MAX_BYTE_IMAGES];
	texture_image_float4 texture_float_images[MAX_FLOAT_IMAGES];

#define KERNEL_TEX(type, ttype, name) ttype name;
#define KERNEL_IMAGE_TEX(type, ttype, name)
#include "kernel_textures.h"

	KernelData __data;

#ifdef __OSL__
	/* On the CPU, we also have the OSL globals here. Most data structures are shared
	 * with SVM, the difference is in the shaders and object/mesh attributes. */
	OSLGlobals *osl;
	OSLShadingSystem *osl_ss;
	OSLThreadData *osl_tdata;
#endif

} KernelGlobals;

#endif

/* For CUDA, constant memory textures must be globals, so we can't put them
 * into a struct. As a result we don't actually use this struct and use actual
 * globals and simply pass along a NULL pointer everywhere, which we hope gets
 * optimized out. */

#ifdef __KERNEL_CUDA__

__constant__ KernelData __data;
typedef struct KernelGlobals {} KernelGlobals;

#ifdef __KERNEL_CUDA_TEX_STORAGE__
#define KERNEL_TEX(type, ttype, name) ttype name;
#else
#define KERNEL_TEX(type, ttype, name) const __constant__ __device__ type *name;
#endif
#define KERNEL_IMAGE_TEX(type, ttype, name) ttype name;
#include "kernel_textures.h"

#endif

/* OpenCL */

#ifdef __KERNEL_OPENCL__

typedef struct KernelGlobals {
	ccl_constant KernelData *data;

#define KERNEL_TEX(type, ttype, name) \
	ccl_global type *name;
#include "kernel_textures.h"
} KernelGlobals;

#endif

/* Interpolated lookup table access */

ccl_device float lookup_table_read(KernelGlobals *kg, float x, int offset, int size)
{
	x = clamp(x, 0.0f, 1.0f)*(size-1);

	int index = min(float_to_int(x), size-1);
	int nindex = min(index+1, size-1);
	float t = x - index;

	float data0 = kernel_tex_fetch(__lookup_table, index + offset);
	if(t == 0.0f)
		return data0;

	float data1 = kernel_tex_fetch(__lookup_table, nindex + offset);
	return (1.0f - t)*data0 + t*data1;
}

ccl_device float lookup_table_read_2D(KernelGlobals *kg, float x, float y, int offset, int xsize, int ysize)
{
	y = clamp(y, 0.0f, 1.0f)*(ysize-1);

	int index = min(float_to_int(y), ysize-1);
	int nindex = min(index+1, ysize-1);
	float t = y - index;

	float data0 = lookup_table_read(kg, x, offset + xsize*index, xsize);
	if(t == 0.0f)
		return data0;

	float data1 = lookup_table_read(kg, x, offset + xsize*nindex, xsize);
	return (1.0f - t)*data0 + t*data1;
}

CCL_NAMESPACE_END

