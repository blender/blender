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

/* Constant Globals */

#ifdef __KERNEL_CPU__

#ifdef __OSL__
#include "osl_globals.h"
#endif

#endif

CCL_NAMESPACE_BEGIN

/* On the CPU, we pass along the struct KernelGlobals to nearly everywhere in
 * the kernel, to access constant data. These are all stored as "textures", but
 * these are really just standard arrays. We can't use actually globals because
 * multiple renders may be running inside the same process. */

#ifdef __KERNEL_CPU__

typedef struct KernelGlobals {

#define KERNEL_TEX(type, ttype, name) ttype name;
#define KERNEL_IMAGE_TEX(type, ttype, name) ttype name;
#include "kernel_textures.h"

	KernelData __data;

#ifdef __OSL__
	/* On the CPU, we also have the OSL globals here. Most data structures are shared
	 * with SVM, the difference is in the shaders and object/mesh attributes. */
	OSLGlobals osl;
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

#define KERNEL_TEX(type, ttype, name) ttype name;
#define KERNEL_IMAGE_TEX(type, ttype, name) ttype name;
#include "kernel_textures.h"

#endif

/* OpenCL */

#ifdef __KERNEL_OPENCL__

typedef struct KernelGlobals {
	__constant KernelData *data;

#define KERNEL_TEX(type, ttype, name) \
	__global type *name;
#include "kernel_textures.h"
} KernelGlobals;

#endif

CCL_NAMESPACE_END

