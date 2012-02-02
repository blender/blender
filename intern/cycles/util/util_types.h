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

#ifndef __UTIL_TYPES_H__
#define __UTIL_TYPES_H__

#ifndef __KERNEL_OPENCL__

#include <stdlib.h>

#endif

/* Qualifiers for kernel code shared by CPU and GPU */

#ifndef __KERNEL_GPU__

#define __device static inline
#define __device_noinline static
#define __global
#define __local
#define __shared
#define __constant

#ifdef __GNUC__
#define __device_inline static inline __attribute__((always_inline))
#else
#define __device_inline static __forceinline
#endif

#endif

/* SIMD Types */

/* not needed yet, will be for qbvh
#ifndef __KERNEL_GPU__

#include <emmintrin.h>
#include <xmmintrin.h>

#endif*/

#ifndef _WIN32
#ifndef __KERNEL_GPU__

#include <stdint.h>

#endif
#endif

CCL_NAMESPACE_BEGIN

/* Types
 *
 * Define simpler unsigned type names, and integer with defined number of bits.
 * Also vector types, named to be compatible with OpenCL builtin types, while
 * working for CUDA and C++ too. */

/* Shorter Unsigned Names */

#ifndef __KERNEL_OPENCL__

typedef unsigned char uchar;
typedef unsigned int uint;

#endif

#ifndef __KERNEL_GPU__

/* Fixed Bits Types */

#ifdef _WIN32

typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef signed short int16_t;
typedef unsigned short uint16_t;

typedef signed int int32_t;
typedef unsigned int uint32_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;

#endif

/* Generic Memory Pointer */

typedef uint64_t device_ptr;

/* Vector Types */

struct uchar2 {
	uchar x, y;

	uchar operator[](int i) const { return *(&x + i); }
	uchar& operator[](int i) { return *(&x + i); }
};

struct uchar3 {
	uchar x, y, z;

	uchar operator[](int i) const { return *(&x + i); }
	uchar& operator[](int i) { return *(&x + i); }
};

struct uchar4 {
	uchar x, y, z, w;

	uchar operator[](int i) const { return *(&x + i); }
	uchar& operator[](int i) { return *(&x + i); }
};

struct int2 {
	int x, y;

	int operator[](int i) const { return *(&x + i); }
	int& operator[](int i) { return *(&x + i); }
};

struct int3 {
	int x, y, z;

	int operator[](int i) const { return *(&x + i); }
	int& operator[](int i) { return *(&x + i); }
};

struct int4 {
	int x, y, z, w;

	int operator[](int i) const { return *(&x + i); }
	int& operator[](int i) { return *(&x + i); }
};

struct uint2 {
	uint x, y;

	uint operator[](int i) const { return *(&x + i); }
	uint& operator[](int i) { return *(&x + i); }
};

struct uint3 {
	uint x, y, z;

	uint operator[](int i) const { return *(&x + i); }
	uint& operator[](int i) { return *(&x + i); }
};

struct uint4 {
	uint x, y, z, w;

	uint operator[](int i) const { return *(&x + i); }
	uint& operator[](int i) { return *(&x + i); }
};

struct float2 {
	float x, y;

	float operator[](int i) const { return *(&x + i); }
	float& operator[](int i) { return *(&x + i); }
};

struct float3 {
	float x, y, z;

#ifdef WITH_OPENCL
	float w;
#endif

	float operator[](int i) const { return *(&x + i); }
	float& operator[](int i) { return *(&x + i); }
};

struct float4 {
	float x, y, z, w;

	float operator[](int i) const { return *(&x + i); }
	float& operator[](int i) { return *(&x + i); }
};

#endif

#ifndef __KERNEL_GPU__

/* Vector Type Constructors
 * 
 * OpenCL does not support C++ class, so we use these instead. */

__device uchar2 make_uchar2(uchar x, uchar y)
{
	uchar2 a = {x, y};
	return a;
}

__device uchar3 make_uchar3(uchar x, uchar y, uchar z)
{
	uchar3 a = {x, y, z};
	return a;
}

__device uchar4 make_uchar4(uchar x, uchar y, uchar z, uchar w)
{
	uchar4 a = {x, y, z, w};
	return a;
}

__device int2 make_int2(int x, int y)
{
	int2 a = {x, y};
	return a;
}

__device int3 make_int3(int x, int y, int z)
{
	int3 a = {x, y, z};
	return a;
}

__device int4 make_int4(int x, int y, int z, int w)
{
	int4 a = {x, y, z, w};
	return a;
}

__device uint2 make_uint2(uint x, uint y)
{
	uint2 a = {x, y};
	return a;
}

__device uint3 make_uint3(uint x, uint y, uint z)
{
	uint3 a = {x, y, z};
	return a;
}

__device uint4 make_uint4(uint x, uint y, uint z, uint w)
{
	uint4 a = {x, y, z, w};
	return a;
}

__device float2 make_float2(float x, float y)
{
	float2 a = {x, y};
	return a;
}

__device float3 make_float3(float x, float y, float z)
{
#ifdef WITH_OPENCL
	float3 a = {x, y, z, 0.0f};
#else
	float3 a = {x, y, z};
#endif
	return a;
}

__device float4 make_float4(float x, float y, float z, float w)
{
	float4 a = {x, y, z, w};
	return a;
}

__device int align_up(int offset, int alignment)
{
	return (offset + alignment - 1) & ~(alignment - 1);
}

#endif

CCL_NAMESPACE_END

#endif /* __UTIL_TYPES_H__ */

