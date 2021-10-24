/*
 * Copyright 2011-2021 Blender Foundation
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

#pragma once

CCL_NAMESPACE_BEGIN

/* Data structures */

/* Integrator State
 *
 * CPU rendering path state with AoS layout. */
typedef struct IntegratorShadowStateCPU {
#define KERNEL_STRUCT_BEGIN(name) struct {
#define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) type name;
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
#undef KERNEL_STRUCT_MEMBER
#undef KERNEL_STRUCT_ARRAY_MEMBER
#undef KERNEL_STRUCT_END
#undef KERNEL_STRUCT_END_ARRAY
} IntegratorShadowStateCPU;

typedef struct IntegratorStateCPU {
#define KERNEL_STRUCT_BEGIN(name) struct {
#define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) type name;
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
#undef KERNEL_STRUCT_MEMBER
#undef KERNEL_STRUCT_ARRAY_MEMBER
#undef KERNEL_STRUCT_END
#undef KERNEL_STRUCT_END_ARRAY
#undef KERNEL_STRUCT_VOLUME_STACK_SIZE

  IntegratorShadowStateCPU shadow;
  IntegratorShadowStateCPU ao;
} IntegratorStateCPU;

/* Path Queue
 *
 * Keep track of which kernels are queued to be executed next in the path
 * for GPU rendering. */
typedef struct IntegratorQueueCounter {
  int num_queued[DEVICE_KERNEL_INTEGRATOR_NUM];
} IntegratorQueueCounter;

/* Integrator State GPU
 *
 * GPU rendering path state with SoA layout. */
typedef struct IntegratorStateGPU {
#define KERNEL_STRUCT_BEGIN(name) struct {
#define KERNEL_STRUCT_MEMBER(parent_struct, type, name, feature) ccl_global type *name;
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
#undef KERNEL_STRUCT_MEMBER
#undef KERNEL_STRUCT_ARRAY_MEMBER
#undef KERNEL_STRUCT_END
#undef KERNEL_STRUCT_END_ARRAY
#undef KERNEL_STRUCT_VOLUME_STACK_SIZE

  /* Count number of queued kernels. */
  ccl_global IntegratorQueueCounter *queue_counter;

  /* Count number of kernels queued for specific shaders. */
  ccl_global int *sort_key_counter[DEVICE_KERNEL_INTEGRATOR_NUM];

  /* Index of shadow path which will be used by a next shadow path.  */
  ccl_global int *next_shadow_path_index;

  /* Index of main path which will be used by a next shadow catcher split.  */
  ccl_global int *next_main_path_index;
} IntegratorStateGPU;

/* Abstraction
 *
 * Macros to access data structures on different devices.
 *
 * Note that there is a special access function for the shadow catcher state. This access is to
 * happen from a kernel which operates on a "main" path. Attempt to use shadow catcher accessors
 * from a kernel which operates on a shadow catcher state will cause bad memory access. */

#ifdef __KERNEL_CPU__

/* Scalar access on CPU. */

typedef IntegratorStateCPU *ccl_restrict IntegratorState;
typedef const IntegratorStateCPU *ccl_restrict ConstIntegratorState;
typedef IntegratorShadowStateCPU *ccl_restrict IntegratorShadowState;
typedef const IntegratorShadowStateCPU *ccl_restrict ConstIntegratorShadowState;

#  define INTEGRATOR_STATE_NULL nullptr

#  define INTEGRATOR_STATE(state, nested_struct, member) ((state)->nested_struct.member)
#  define INTEGRATOR_STATE_WRITE(state, nested_struct, member) ((state)->nested_struct.member)

#  define INTEGRATOR_STATE_ARRAY(state, nested_struct, array_index, member) \
    ((state)->nested_struct[array_index].member)
#  define INTEGRATOR_STATE_ARRAY_WRITE(state, nested_struct, array_index, member) \
    ((state)->nested_struct[array_index].member)

#else /* __KERNEL_CPU__ */

/* Array access on GPU with Structure-of-Arrays. */

typedef const int IntegratorState;
typedef const int ConstIntegratorState;
typedef const int IntegratorShadowState;
typedef const int ConstIntegratorShadowState;

#  define INTEGRATOR_STATE_NULL -1

#  define INTEGRATOR_STATE(state, nested_struct, member) \
    kernel_integrator_state.nested_struct.member[state]
#  define INTEGRATOR_STATE_WRITE(state, nested_struct, member) \
    INTEGRATOR_STATE(state, nested_struct, member)

#  define INTEGRATOR_STATE_ARRAY(state, nested_struct, array_index, member) \
    kernel_integrator_state.nested_struct[array_index].member[state]
#  define INTEGRATOR_STATE_ARRAY_WRITE(state, nested_struct, array_index, member) \
    INTEGRATOR_STATE_ARRAY(state, nested_struct, array_index, member)

#endif /* __KERNEL_CPU__ */

CCL_NAMESPACE_END
