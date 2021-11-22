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

#define BVH_NAME_JOIN(x, y) x##_##y
#define BVH_NAME_EVAL(x, y) BVH_NAME_JOIN(x, y)
#define BVH_FUNCTION_FULL_NAME(prefix) BVH_NAME_EVAL(prefix, BVH_FUNCTION_NAME)

#define BVH_FEATURE(f) (((BVH_FUNCTION_FEATURES) & (f)) != 0)

CCL_NAMESPACE_END
