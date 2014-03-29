/*
 * Adapted from code Copyright 2009-2010 NVIDIA Corporation
 * Modifications Copyright 2011, Blender Foundation.
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

/* bottom-most stack entry, indicating the end of traversal */
#define ENTRYPOINT_SENTINEL 0x76543210

/* 64 object BVH + 64 mesh BVH + 64 object node splitting */
#define BVH_STACK_SIZE 192
#define BVH_NODE_SIZE 4
#define TRI_NODE_SIZE 3

/* silly workaround for float extended precision that happens when compiling
 * without sse support on x86, it results in different results for float ops
 * that you would otherwise expect to compare correctly */
#if !defined(__i386__) || defined(__SSE__)
#define NO_EXTENDED_PRECISION
#else
#define NO_EXTENDED_PRECISION volatile
#endif

#include "geom_attribute.h"
#include "geom_object.h"
#include "geom_triangle.h"
#include "geom_motion_triangle.h"
#include "geom_motion_curve.h"
#include "geom_curve.h"
#include "geom_volume.h"
#include "geom_primitive.h"
#include "geom_bvh.h"

