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

#ifndef __BVH_TYPES__
#define __BVH_TYPES__

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

/* Debugging heleprs */
#ifdef __KERNEL_DEBUG__
#  define BVH_DEBUG_INIT() \
    do { \
      isect->num_traversed_nodes = 0; \
      isect->num_traversed_instances = 0; \
      isect->num_intersections = 0; \
    } while (0)
#  define BVH_DEBUG_NEXT_NODE() \
    do { \
      ++isect->num_traversed_nodes; \
    } while (0)
#  define BVH_DEBUG_NEXT_INTERSECTION() \
    do { \
      ++isect->num_intersections; \
    } while (0)
#  define BVH_DEBUG_NEXT_INSTANCE() \
    do { \
      ++isect->num_traversed_instances; \
    } while (0)
#else /* __KERNEL_DEBUG__ */
#  define BVH_DEBUG_INIT()
#  define BVH_DEBUG_NEXT_NODE()
#  define BVH_DEBUG_NEXT_INTERSECTION()
#  define BVH_DEBUG_NEXT_INSTANCE()
#endif /* __KERNEL_DEBUG__ */

CCL_NAMESPACE_END

#endif /* __BVH_TYPES__ */
