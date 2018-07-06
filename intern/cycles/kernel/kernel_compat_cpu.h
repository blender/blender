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

#ifndef __KERNEL_COMPAT_CPU_H__
#define __KERNEL_COMPAT_CPU_H__

#define __KERNEL_CPU__

/* Release kernel has too much false-positive maybe-uninitialized warnings,
 * which makes it possible to miss actual warnings.
 */
#if (defined(__GNUC__) && !defined(__clang__)) && defined(NDEBUG)
#  pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#  pragma GCC diagnostic ignored "-Wuninitialized"
#endif

/* Selective nodes compilation. */
#ifndef __NODES_MAX_GROUP__
#  define __NODES_MAX_GROUP__ NODE_GROUP_LEVEL_MAX
#endif
#ifndef __NODES_FEATURES__
#  define __NODES_FEATURES__ NODE_FEATURE_ALL
#endif

#include "util/util_math.h"
#include "util/util_simd.h"
#include "util/util_half.h"
#include "util/util_types.h"
#include "util/util_texture.h"

#define ccl_addr_space

#define ccl_local_id(d) 0
#define ccl_global_id(d) (kg->global_id[d])

#define ccl_local_size(d) 1
#define ccl_global_size(d) (kg->global_size[d])

#define ccl_group_id(d) ccl_global_id(d)
#define ccl_num_groups(d) ccl_global_size(d)

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
	ccl_always_inline const T& fetch(int index)
	{
		kernel_assert(index >= 0 && index < width);
		return data[index];
	}

#ifdef __KERNEL_AVX__
	/* Reads 256 bytes but indexes in blocks of 128 bytes to maintain
	 * compatibility with existing indicies and data structures.
	 */
	ccl_always_inline avxf fetch_avxf(const int index)
	{
		kernel_assert(index >= 0 && (index+1) < width);
		ssef *ssef_data = (ssef*)data;
		ssef *ssef_node_data = &ssef_data[index];
		return _mm256_loadu_ps((float *)ssef_node_data);
	}

#endif

#ifdef __KERNEL_SSE2__
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
#endif

	T *data;
	int width;
};

/* Macros to handle different memory storage on different devices */

#define kernel_tex_fetch(tex, index) (kg->tex.fetch(index))
#define kernel_tex_fetch_avxf(tex, index) (kg->tex.fetch_avxf(index))
#define kernel_tex_fetch_ssef(tex, index) (kg->tex.fetch_ssef(index))
#define kernel_tex_fetch_ssei(tex, index) (kg->tex.fetch_ssei(index))
#define kernel_tex_lookup(tex, t, offset, size) (kg->tex.lookup(t, offset, size))
#define kernel_tex_array(tex) (kg->tex.data)

#define kernel_data (kg->__data)

#ifdef __KERNEL_SSE2__
typedef vector3<sseb> sse3b;
typedef vector3<ssef> sse3f;
typedef vector3<ssei> sse3i;

ccl_device_inline void print_sse3b(const char *label, sse3b& a)
{
	print_sseb(label, a.x);
	print_sseb(label, a.y);
	print_sseb(label, a.z);
}

ccl_device_inline void print_sse3f(const char *label, sse3f& a)
{
	print_ssef(label, a.x);
	print_ssef(label, a.y);
	print_ssef(label, a.z);
}

ccl_device_inline void print_sse3i(const char *label, sse3i& a)
{
	print_ssei(label, a.x);
	print_ssei(label, a.y);
	print_ssei(label, a.z);
}

#endif

CCL_NAMESPACE_END

#endif /* __KERNEL_COMPAT_CPU_H__ */
