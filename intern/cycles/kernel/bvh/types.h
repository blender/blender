/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

CCL_NAMESPACE_BEGIN

/* Don't inline intersect functions on GPU, this is faster */
#ifdef __KERNEL_GPU__
#  define ccl_device_intersect ccl_device_forceinline
#else
#  define ccl_device_intersect ccl_device_inline
#endif

/* bottom-most stack entry, indicating the end of traversal */
#define ENTRYPOINT_SENTINEL 0x76543210

/* 64 object BVH + 64 mesh BVH + 64 object node splitting */
#define BVH_STACK_SIZE 192
/* BVH intersection function variations */

#define BVH_MOTION 1
#define BVH_HAIR 2
#define BVH_POINTCLOUD 4

#define BVH_NAME_JOIN(x, y) x##_##y
#define BVH_NAME_EVAL(x, y) BVH_NAME_JOIN(x, y)
#define BVH_FUNCTION_FULL_NAME(prefix) BVH_NAME_EVAL(prefix, BVH_FUNCTION_NAME)

#define BVH_FEATURE(f) (((BVH_FUNCTION_FEATURES) & (f)) != 0)

CCL_NAMESPACE_END
