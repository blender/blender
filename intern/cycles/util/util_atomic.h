/*
 * Copyright 2014 Blender Foundation
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

#ifndef __UTIL_ATOMIC_H__
#define __UTIL_ATOMIC_H__

#ifndef __KERNEL_GPU__

/* Using atomic ops header from Blender. */
#include "atomic_ops.h"

#define atomic_add_and_fetch_float(p, x) atomic_add_and_fetch_fl((p), (x))
#define atomic_compare_and_swap_float(p, old_val, new_val) atomic_cas_float((p), (old_val), (new_val))

#define atomic_fetch_and_inc_uint32(p) atomic_fetch_and_add_uint32((p), 1)
#define atomic_fetch_and_dec_uint32(p) atomic_fetch_and_add_uint32((p), -1)

#define CCL_LOCAL_MEM_FENCE 0
#define ccl_barrier(flags) (void)0

#else  /* __KERNEL_GPU__ */

#ifdef __KERNEL_OPENCL__

/* Float atomics implementation credits:
 *   http://suhorukov.blogspot.in/2011/12/opencl-11-atomic-operations-on-floating.html
 */
ccl_device_inline float atomic_add_and_fetch_float(volatile ccl_global float *source,
                                        const float operand)
{
	union {
		unsigned int int_value;
		float float_value;
	} new_value;
	union {
		unsigned int int_value;
		float float_value;
	} prev_value;
	do {
		prev_value.float_value = *source;
		new_value.float_value = prev_value.float_value + operand;
	} while(atomic_cmpxchg((volatile ccl_global unsigned int *)source,
	                       prev_value.int_value,
	                       new_value.int_value) != prev_value.int_value);
	return new_value.float_value;
}

ccl_device_inline float atomic_compare_and_swap_float(volatile ccl_global float *dest,
                                                      const float old_val, const float new_val)
{
	union {
		unsigned int int_value;
		float float_value;
	} new_value, prev_value, result;
	prev_value.float_value = old_val;
	new_value.float_value = new_val;
	result.int_value = atomic_cmpxchg((volatile ccl_global unsigned int *)dest,
                                       prev_value.int_value, new_value.int_value);
	return result.float_value;
}

#define atomic_fetch_and_add_uint32(p, x) atomic_add((p), (x))
#define atomic_fetch_and_inc_uint32(p) atomic_inc((p))
#define atomic_fetch_and_dec_uint32(p) atomic_dec((p))

#define CCL_LOCAL_MEM_FENCE CLK_LOCAL_MEM_FENCE
#define ccl_barrier(flags) barrier(flags)

#endif  /* __KERNEL_OPENCL__ */

#ifdef __KERNEL_CUDA__

#define atomic_add_and_fetch_float(p, x) (atomicAdd((float*)(p), (float)(x)) + (float)(x))

#define atomic_fetch_and_add_uint32(p, x) atomicAdd((unsigned int*)(p), (unsigned int)(x))
#define atomic_fetch_and_sub_uint32(p, x) atomicSub((unsigned int*)(p), (unsigned int)(x))
#define atomic_fetch_and_inc_uint32(p) atomic_fetch_and_add_uint32((p), 1)
#define atomic_fetch_and_dec_uint32(p) atomic_fetch_and_sub_uint32((p), 1)

ccl_device_inline float atomic_compare_and_swap_float(volatile float *dest,
                                                      const float old_val, const float new_val)
{
	union {
		unsigned int int_value;
		float float_value;
	} new_value, prev_value, result;
	prev_value.float_value = old_val;
	new_value.float_value = new_val;
	result.int_value = atomicCAS((unsigned int *)dest, prev_value.int_value,new_value.int_value);
	return result.float_value;
}

#define CCL_LOCAL_MEM_FENCE
#define ccl_barrier(flags) __syncthreads()

#endif  /* __KERNEL_CUDA__ */

#endif  /* __KERNEL_GPU__ */

#endif /* __UTIL_ATOMIC_H__ */
