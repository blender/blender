/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Integrator State
 *
 * This file defines the data structures that define the state of a path. Any state that is
 * preserved and passed between kernel executions is part of this.
 *
 * The size of this state must be kept as small as possible, to reduce cache misses and keep memory
 * usage under control on GPUs that may execute millions of kernels.
 *
 * Memory may be allocated and passed along in different ways depending on the device. There may
 * be a scalar layout, or AoS or SoA layout for batches. The state may be passed along as a pointer
 * to every kernel, or the pointer may exist at program scope or in constant memory. To abstract
 * these differences between devices and experiment with different layouts, macros are used.
 *
 * Use IntegratorState to pass a reference to the integrator state for the current path. These are
 * defined differently on the CPU and GPU. Use ConstIntegratorState instead of const
 * IntegratorState for passing state as read-only, to avoid oddities in typedef behavior.
 *
 * INTEGRATOR_STATE(state, x, y): read nested struct member x.y of IntegratorState
 * INTEGRATOR_STATE_WRITE(state, x, y): write to nested struct member x.y of IntegratorState
 *
 * INTEGRATOR_STATE_ARRAY(state, x, index, y): read x[index].y
 * INTEGRATOR_STATE_ARRAY_WRITE(state, x, index, y): write x[index].y
 *
 * INTEGRATOR_STATE_NULL: use to pass empty state to other functions.
 */

#include "kernel/types.h"

#include "util/types.h"

#ifdef __PATH_GUIDING__
#  include "util/guiding.h"  // IWYU pragma: keep
#endif

#pragma once

CCL_NAMESPACE_BEGIN

/* Data structures */

/* Integrator State
 *
 * CPU rendering path state with AoS layout. */
struct IntegratorShadowStateCPU {
#define KERNEL_STRUCT_BEGIN(name) struct {
#define KERNEL_STRUCT_BEGIN_PACKED(parent_struct, feature) struct {
#define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) type name;
#define KERNEL_STRUCT_MEMBER_PACKED KERNEL_STRUCT_MEMBER
#define KERNEL_STRUCT_ARRAY_MEMBER KERNEL_STRUCT_MEMBER
#define KERNEL_STRUCT_END(name) \
  } \
  name;
#define KERNEL_STRUCT_END_ARRAY(name, cpu_size, gpu_size) \
  } \
  name[cpu_size];
#define KERNEL_STRUCT_VOLUME_STACK_SIZE MAX_VOLUME_STACK_SIZE
#include "kernel/integrator/shadow_state_template.h"
#undef KERNEL_STRUCT_BEGIN
#undef KERNEL_STRUCT_BEGIN_PACKED
#undef KERNEL_STRUCT_MEMBER
#undef KERNEL_STRUCT_MEMBER_PACKED
#undef KERNEL_STRUCT_ARRAY_MEMBER
#undef KERNEL_STRUCT_END
#undef KERNEL_STRUCT_END_ARRAY
};

struct IntegratorStateCPU {
#define KERNEL_STRUCT_BEGIN(name) struct {
#define KERNEL_STRUCT_BEGIN_PACKED(parent_struct, feature) struct {
#define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) type name;
#define KERNEL_STRUCT_MEMBER_PACKED KERNEL_STRUCT_MEMBER
#define KERNEL_STRUCT_ARRAY_MEMBER KERNEL_STRUCT_MEMBER
#define KERNEL_STRUCT_END(name) \
  } \
  name;
#define KERNEL_STRUCT_END_ARRAY(name, cpu_size, gpu_size) \
  } \
  name[cpu_size];
#define KERNEL_STRUCT_VOLUME_STACK_SIZE MAX_VOLUME_STACK_SIZE
#include "kernel/integrator/state_template.h"
#undef KERNEL_STRUCT_BEGIN
#undef KERNEL_STRUCT_BEGIN_PACKED
#undef KERNEL_STRUCT_MEMBER
#undef KERNEL_STRUCT_MEMBER_PACKED
#undef KERNEL_STRUCT_ARRAY_MEMBER
#undef KERNEL_STRUCT_END
#undef KERNEL_STRUCT_END_ARRAY
#undef KERNEL_STRUCT_VOLUME_STACK_SIZE

  IntegratorShadowStateCPU shadow;
  IntegratorShadowStateCPU ao;
};

/* Path Queue
 *
 * Keep track of which kernels are queued to be executed next in the path
 * for GPU rendering. */
struct IntegratorQueueCounter {
  int num_queued[DEVICE_KERNEL_INTEGRATOR_NUM];
};

#if defined(__INTEGRATOR_GPU_PACKED_STATE__) && defined(__KERNEL_GPU__)

/* Generate wrapper structs for all integrator state fields. This allows us to access state
 * uniformly, regardless of whether it stored in a packed struct or separate arrays. */
#  define KERNEL_STRUCT_BEGIN(name)
#  define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) \
    struct Wrapped_##parent_struct##_##name { \
      type name; \
    };
#  define KERNEL_STRUCT_MEMBER_PACKED KERNEL_STRUCT_MEMBER
#  define KERNEL_STRUCT_BEGIN_PACKED(parent_struct, feature) \
    KERNEL_STRUCT_BEGIN(parent_struct) \
    KERNEL_STRUCT_MEMBER(parent_struct, packed_##parent_struct, packed, feature)
#  define KERNEL_STRUCT_ARRAY_MEMBER KERNEL_STRUCT_MEMBER
#  define KERNEL_STRUCT_END(name)
#  define KERNEL_STRUCT_END_ARRAY(name, cpu_size, gpu_size)
#  define KERNEL_STRUCT_VOLUME_STACK_SIZE MAX_VOLUME_STACK_SIZE

#  include "kernel/integrator/shadow_state_template.h"
#  include "kernel/integrator/state_template.h"

#  undef KERNEL_STRUCT_BEGIN
#  undef KERNEL_STRUCT_BEGIN_PACKED
#  undef KERNEL_STRUCT_MEMBER
#  undef KERNEL_STRUCT_MEMBER_PACKED
#  undef KERNEL_STRUCT_ARRAY_MEMBER
#  undef KERNEL_STRUCT_END
#  undef KERNEL_STRUCT_END_ARRAY
#  undef KERNEL_STRUCT_VOLUME_STACK_SIZE

#endif

/* Integrator State GPU
 *
 * GPU rendering path state with SoA layout. */
struct IntegratorStateGPU {
#define KERNEL_STRUCT_BEGIN(name) struct {

#ifdef __INTEGRATOR_GPU_PACKED_STATE__

#  ifdef __KERNEL_GPU__

/* If we've opted in to packed layouts, generate member functions that return a pointer to a
 * wrapper type so we can access state using uniform syntax. */
#    define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) \
      ccl_global Wrapped_##parent_struct##_##name *name; \
      ccl_device_inline ccl_global Wrapped_##parent_struct##_##name *name##_fn() ccl_constant \
      { \
        return (ccl_global Wrapped_##parent_struct##_##name *)name; \
      }
#    define KERNEL_STRUCT_MEMBER_PACKED(parent_struct, type, name, feature) \
      ccl_device_inline ccl_global packed_##parent_struct *name##_fn() ccl_constant \
      { \
        return (ccl_global packed_##parent_struct *)packed; \
      }
#  else
#    define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) ccl_global type *name;
#    define KERNEL_STRUCT_MEMBER_PACKED(parent_struct, type, name, feature)
#  endif

#  define KERNEL_STRUCT_BEGIN_PACKED(parent_struct, feature) \
    KERNEL_STRUCT_BEGIN(parent_struct) \
    KERNEL_STRUCT_MEMBER(parent_struct, packed_##parent_struct, packed, feature)

#else
#  define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) ccl_global type *name;
#  define KERNEL_STRUCT_MEMBER_PACKED KERNEL_STRUCT_MEMBER
#  define KERNEL_STRUCT_BEGIN_PACKED(parent_struct, feature) KERNEL_STRUCT_BEGIN(parent_struct)
#endif
#define KERNEL_STRUCT_ARRAY_MEMBER KERNEL_STRUCT_MEMBER
#define KERNEL_STRUCT_END(name) \
  } \
  name;
#define KERNEL_STRUCT_END_ARRAY(name, cpu_size, gpu_size) \
  } \
  name[gpu_size];
#define KERNEL_STRUCT_VOLUME_STACK_SIZE MAX_VOLUME_STACK_SIZE

#include "kernel/integrator/state_template.h"

#include "kernel/integrator/shadow_state_template.h"

#undef KERNEL_STRUCT_BEGIN
#undef KERNEL_STRUCT_BEGIN_PACKED
#undef KERNEL_STRUCT_MEMBER
#undef KERNEL_STRUCT_MEMBER_PACKED
#undef KERNEL_STRUCT_ARRAY_MEMBER
#undef KERNEL_STRUCT_END
#undef KERNEL_STRUCT_END_ARRAY
#undef KERNEL_STRUCT_VOLUME_STACK_SIZE

  /* Count number of queued kernels. */
  ccl_global IntegratorQueueCounter *queue_counter;

  /* Count number of kernels queued for specific shaders. */
  ccl_global int *sort_key_counter[DEVICE_KERNEL_INTEGRATOR_NUM];

  /* Index of shadow path which will be used by a next shadow path. */
  ccl_global int *next_shadow_path_index;

  /* Index of main path which will be used by a next shadow catcher split. */
  ccl_global int *next_main_path_index;

  /* Partition/key offsets used when writing sorted active indices. */
  ccl_global int *sort_partition_key_offsets;

  /* Divisor used to partition active indices by locality when sorting by material. */
  uint sort_partition_divisor;
};

/* Abstraction
 *
 * Macros to access data structures on different devices.
 *
 * Note that there is a special access function for the shadow catcher state. This access is to
 * happen from a kernel which operates on a "main" path. Attempt to use shadow catcher accessors
 * from a kernel which operates on a shadow catcher state will cause bad memory access. */

#ifndef __KERNEL_GPU__

/* Scalar access on CPU. */

using IntegratorState = IntegratorStateCPU *;
using ConstIntegratorState = const IntegratorStateCPU *;
using IntegratorShadowState = IntegratorShadowStateCPU *;
using ConstIntegratorShadowState = const IntegratorShadowStateCPU *;

#  define INTEGRATOR_STATE_NULL nullptr

#  define INTEGRATOR_STATE(state, nested_struct, member) ((state)->nested_struct.member)
#  define INTEGRATOR_STATE_WRITE(state, nested_struct, member) ((state)->nested_struct.member)

#  define INTEGRATOR_STATE_ARRAY(state, nested_struct, array_index, member) \
    ((state)->nested_struct[array_index].member)
#  define INTEGRATOR_STATE_ARRAY_WRITE(state, nested_struct, array_index, member) \
    ((state)->nested_struct[array_index].member)

#else /* !__KERNEL_GPU__ */

/* Array access on GPU with Structure-of-Arrays. */

using IntegratorState = int;
using ConstIntegratorState = int;
using IntegratorShadowState = int;
using ConstIntegratorShadowState = int;

#  define INTEGRATOR_STATE_NULL -1

#  ifdef __INTEGRATOR_GPU_PACKED_STATE__

/* If we've opted in to packed layouts, we use the generated accessor functions (member##_fn) to
 * resolve different layouts (packed vs separate). */
#    define INTEGRATOR_STATE(state, nested_struct, member) \
      kernel_integrator_state.nested_struct.member##_fn()[state].member
#    define INTEGRATOR_STATE_ARRAY(state, nested_struct, array_index, member) \
      kernel_integrator_state.nested_struct[array_index].member##_fn()[state].member
#  else
#    define INTEGRATOR_STATE(state, nested_struct, member) \
      kernel_integrator_state.nested_struct.member[state]
#    define INTEGRATOR_STATE_ARRAY(state, nested_struct, array_index, member) \
      kernel_integrator_state.nested_struct[array_index].member[state]
#  endif

#  define INTEGRATOR_STATE_WRITE(state, nested_struct, member) \
    INTEGRATOR_STATE(state, nested_struct, member)
#  define INTEGRATOR_STATE_ARRAY_WRITE(state, nested_struct, array_index, member) \
    INTEGRATOR_STATE_ARRAY(state, nested_struct, array_index, member)

#endif /* !__KERNEL_GPU__ */

CCL_NAMESPACE_END
