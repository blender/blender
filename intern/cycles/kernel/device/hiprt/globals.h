/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "kernel/integrator/state.h"

#include "kernel/util/profiling.h"

#define HIPRT_SHARED_STACK

/* The size of global stack  available to each thread (memory reserved for each thread in
 * global_stack_buffer). */
#define HIPRT_THREAD_STACK_SIZE 64

/* LDS (Local Data Storage) allocation for each thread, the number is obtained empirically. */
#define HIPRT_SHARED_STACK_SIZE 24

/* HIPRT_THREAD_GROUP_SIZE is the number of threads per work group for intersection kernels
 * The default number of threads per work-group is 1024, however, since HIP RT intersection kernels
 * use local memory, and the local memory size in those kernels scales up with the number of
 * threads, the number of threads to is scaled down to 256 to avoid going over maximum local memory
 * and to strike a balance between memory access and the number of waves.
 *
 * Total local stack size would be number of threads * HIPRT_SHARED_STACK_SIZE. */
#define HIPRT_THREAD_GROUP_SIZE 256

CCL_NAMESPACE_BEGIN

struct KernelGlobalsGPU {
  int *global_stack_buffer;
#ifdef HIPRT_SHARED_STACK
  int *shared_stack;
#endif
};

typedef ccl_global KernelGlobalsGPU *ccl_restrict KernelGlobals;

#if defined(HIPRT_SHARED_STACK)

/* This macro allocates shared memory and to pass the shared memory down to intersection functions
 * KernelGlobals is used. */
#  define HIPRT_INIT_KERNEL_GLOBAL() \
    ccl_gpu_shared int shared_stack[HIPRT_SHARED_STACK_SIZE * HIPRT_THREAD_GROUP_SIZE]; \
    ccl_global KernelGlobalsGPU kg_gpu; \
    KernelGlobals kg = &kg_gpu; \
    kg->shared_stack = &shared_stack[0]; \
    kg->global_stack_buffer = stack_buffer;
#else
#  define HIPRT_INIT_KERNEL_GLOBAL() \
    KernelGlobals kg = NULL; \
    kg->global_stack_buffer = stack_buffer;
#endif

struct KernelParamsHIPRT {
  KernelData data;
#define KERNEL_DATA_ARRAY(type, name) const type *name;
  KERNEL_DATA_ARRAY(int, user_instance_id)
  KERNEL_DATA_ARRAY(uint64_t, blas_ptr)
  KERNEL_DATA_ARRAY(int2, custom_prim_info)
  KERNEL_DATA_ARRAY(int2, custom_prim_info_offset)
  KERNEL_DATA_ARRAY(float2, prims_time)
  KERNEL_DATA_ARRAY(int, prim_time_offset)
#include "kernel/data_arrays.h"

  /* Integrator state */
  IntegratorStateGPU integrator_state;

  hiprtFuncTable table_closest_intersect;
  hiprtFuncTable table_shadow_intersect;
  hiprtFuncTable table_local_intersect;
  hiprtFuncTable table_volume_intersect;
};

/* Intersection_Function_Table_Index defines index values to retrieve custom intersection
 * functions from function table. */

enum Intersection_Function_Table_Index {
  // Triangles use the intersection function provided by HIP RT and don't need custom intersection
  // functions
  // Custom intersection functions for closest intersect.
  Curve_Intersect_Function = 1,        // Custom intersection for curves
  Motion_Triangle_Intersect_Function,  // Custom intersection for triangles with vertex motion blur
                                       // attributes.
  Point_Intersect_Function,            // Custom intersection for point cloud.
  // Custom intersection functions for shadow rendering are the same as the function for closest
  // intersect.
  // However, the table indices are different
  Triangle_Intersect_Shadow_None,
  Curve_Intersect_Shadow,
  Motion_Triangle_Intersect_Shadow,
  Point_Intersect_Shadow,
  // Custom intersection functions for subsurface scattering.
  // Only motion triangles have valid custom intersection function
  Triangle_Intersect_Local_None,
  Curve_Intersect_Local_None,
  Motion_Triangle_Intersect_Local,
  Point_Intersect_Local_None,
  // Custom intersection functions for volume rendering.
  // Only motion triangles have valid custom intersection function
  Triangle_Intersect_Volume_None,
  Curve_Intersect_Volume_None,
  Motion_Triangle_Intersect_Volume,
  Point_Intersect_Volume_None,
};

// Filter functions, filter hits, i.e. test whether a hit should be accepted or not, and whether
// traversal should stop or continue.
enum Filter_Function_Table_Index {
  Triangle_Filter_Closest = 0,  // Filter function for triangles for closest intersect, no custom
                                // intersection function is needed.
  Curve_Filter_Opaque_None,     // No filter function is needed and everything is handled in the
                                // intersection function.
  Motion_Triangle_Filter_Opaque_None,  // No filter function is needed and everything is handled in
                                       // intersection function.
  Point_Filter_Opaque_Non,             // No filter function is needed.
  // Filter function for all primitives for shadow intersection.
  // All primitives use the same function but each has a different index in the table.
  Triangle_Filter_Shadow,
  Curve_Filter_Shadow,
  Motion_Triangle_Filter_Shadow,
  Point_Filter_Shadow,
  // Filter functions for subsurface scattering. Triangles and motion triangles need function
  // assignment. They indices for triangles and motion triangles point to the same function. Points
  // and curves dont need any function since subsurface scattering is not applied on either.
  Triangle_Filter_Local,    // Filter functions for triangles
  Curve_Filter_Local_None,  // Subsurface scattering is not applied on curves, no filter function
                            // is
                            // needed.
  Motion_Triangle_Filter_Local,
  Point_Filter_Local_None,
  // Filter functions for volume rendering.
  // Volume rendering only applies to triangles and motion triangles.
  // Triangles and motion triangles use the same filter functions for volume rendering
  Triangle_Filter_Volume,
  Curve_Filter_Volume_None,
  Motion_Triangle_Filter_Volume,
  Point_Filter_Volume_None,
};

#ifdef __KERNEL_GPU__
__constant__ KernelParamsHIPRT kernel_params;

#  ifdef HIPRT_SHARED_STACK
typedef hiprtGlobalStack Stack;
#  endif

#endif

/* Abstraction macros */
#define kernel_data kernel_params.data
#define kernel_data_fetch(name, index) kernel_params.name[(index)]
#define kernel_data_array(name) (kernel_params.name)
#define kernel_integrator_state kernel_params.integrator_state

CCL_NAMESPACE_END
