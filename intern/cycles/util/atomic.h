/* SPDX-FileCopyrightText: 2014-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __KERNEL_GPU__

/* Using atomic ops header from Blender. */
#  include "atomic_ops.h"  // IWYU pragma: export

#  define atomic_add_and_fetch_float(p, x) atomic_add_and_fetch_fl((p), (x))
#  define atomic_compare_and_swap_float(p, old_val, new_val) \
    atomic_cas_float((p), (old_val), (new_val))

#  define atomic_fetch_and_inc_uint32(p) atomic_fetch_and_add_uint32((p), 1)
#  define atomic_fetch_and_dec_uint32(p) atomic_fetch_and_add_uint32((p), -1)

#  define CCL_LOCAL_MEM_FENCE 0
#  define ccl_barrier(flags) ((void)0)

#else /* __KERNEL_GPU__ */

#  ifndef __KERNEL_ONEAPI__
#    define atomic_fetch_and_add_uint32_shared atomic_fetch_and_add_uint32
#  endif

#  if defined(__KERNEL_CUDA__) || defined(__KERNEL_HIP__)

#    define atomic_add_and_fetch_float(p, x) (atomicAdd((float *)(p), (float)(x)) + (float)(x))

#    define atomic_fetch_and_add_uint32(p, x) atomicAdd((unsigned int *)(p), (unsigned int)(x))
#    define atomic_fetch_and_sub_uint32(p, x) atomicSub((unsigned int *)(p), (unsigned int)(x))
#    define atomic_fetch_and_inc_uint32(p) atomic_fetch_and_add_uint32((p), 1)
#    define atomic_fetch_and_dec_uint32(p) atomic_fetch_and_sub_uint32((p), 1)
#    define atomic_fetch_and_or_uint32(p, x) atomicOr((unsigned int *)(p), (unsigned int)(x))

ccl_device_inline float atomic_compare_and_swap_float(volatile float *dest,
                                                      const float old_val,
                                                      const float new_val)
{
  union {
    unsigned int int_value;
    float float_value;
  } new_value, prev_value, result;
  prev_value.float_value = old_val;
  new_value.float_value = new_val;
  result.int_value = atomicCAS((unsigned int *)dest, prev_value.int_value, new_value.int_value);
  return result.float_value;
}

#    define CCL_LOCAL_MEM_FENCE
#    define ccl_barrier(flags) __syncthreads()

#  endif /* __KERNEL_CUDA__ */

#  ifdef __KERNEL_METAL__

// global address space versions
ccl_device_inline float atomic_add_and_fetch_float(volatile ccl_global float *_source,
                                                   const float operand)
{
#    if __METAL_VERSION__ >= 300
  return atomic_fetch_add_explicit(
      (ccl_global atomic_float *)_source, operand, memory_order_relaxed);
#    else
  volatile ccl_global atomic_int *source = (ccl_global atomic_int *)_source;
  union {
    int int_value;
    float float_value;
  } new_value, prev_value;
  prev_value.int_value = atomic_load_explicit(source, memory_order_relaxed);
  do {
    new_value.float_value = prev_value.float_value + operand;
  } while (!atomic_compare_exchange_weak_explicit(source,
                                                  &prev_value.int_value,
                                                  new_value.int_value,
                                                  memory_order_relaxed,
                                                  memory_order_relaxed));

  return new_value.float_value;
#    endif
}

template<class T> ccl_device_inline uint32_t atomic_fetch_and_add_uint32(device T *p, const int x)
{
  return atomic_fetch_add_explicit((device atomic_uint *)p, x, memory_order_relaxed);
}

template<class T> ccl_device_inline uint32_t atomic_fetch_and_sub_uint32(device T *p, const int x)
{
  return atomic_fetch_sub_explicit((device atomic_uint *)p, x, memory_order_relaxed);
}

template<class T> ccl_device_inline uint32_t atomic_fetch_and_inc_uint32(device T *p)
{
  return atomic_fetch_add_explicit((device atomic_uint *)p, 1, memory_order_relaxed);
}

template<class T> ccl_device_inline uint32_t atomic_fetch_and_dec_uint32(device T *p)
{
  return atomic_fetch_sub_explicit((device atomic_uint *)p, 1, memory_order_relaxed);
}

template<class T> ccl_device_inline uint32_t atomic_fetch_and_or_uint32(device T *p, const int x)
{
  return atomic_fetch_or_explicit((device atomic_uint *)p, x, memory_order_relaxed);
}

template<class T>
ccl_device_inline uint32_t atomic_fetch_and_add_uint32(threadgroup T *p, const int x)
{
  return atomic_fetch_add_explicit((threadgroup atomic_uint *)p, x, memory_order_relaxed);
}

template<class T>
ccl_device_inline uint32_t atomic_fetch_and_sub_uint32(threadgroup T *p, const int x)
{
  return atomic_fetch_sub_explicit((threadgroup atomic_uint *)p, x, memory_order_relaxed);
}

template<class T> ccl_device_inline uint32_t atomic_fetch_and_inc_uint32(threadgroup T *p)
{
  return atomic_fetch_add_explicit((threadgroup atomic_uint *)p, 1, memory_order_relaxed);
}

template<class T> ccl_device_inline uint32_t atomic_fetch_and_dec_uint32(threadgroup T *p)
{
  return atomic_fetch_sub_explicit((threadgroup atomic_uint *)p, 1, memory_order_relaxed);
}

template<class T>
ccl_device_inline uint32_t atomic_fetch_and_or_uint32(threadgroup T *p, const int x)
{
  return atomic_fetch_or_explicit((threadgroup atomic_uint *)p, x, memory_order_relaxed);
}

ccl_device_inline float atomic_compare_and_swap_float(volatile ccl_global float *dest,
                                                      const float old_val,
                                                      const float new_val)
{
#    if __METAL_VERSION__ >= 300
  float prev_value = old_val;
  atomic_compare_exchange_weak_explicit((ccl_global atomic_float *)dest,
                                        &prev_value,
                                        new_val,
                                        memory_order_relaxed,
                                        memory_order_relaxed);
  return prev_value;
#    else
  int prev_value;
  prev_value = __float_as_int(old_val);
  atomic_compare_exchange_weak_explicit((ccl_global atomic_int *)dest,
                                        &prev_value,
                                        __float_as_int(new_val),
                                        memory_order_relaxed,
                                        memory_order_relaxed);
  return __int_as_float(prev_value);
#    endif
}

#    define atomic_store(p, x) atomic_store_explicit(p, x, memory_order_relaxed)
#    define atomic_fetch(p) atomic_load_explicit(p, memory_order_relaxed)

#    define atomic_store_local(p, x) \
      atomic_store_explicit((ccl_gpu_shared atomic_int *)p, x, memory_order_relaxed)
#    define atomic_load_local(p) \
      atomic_load_explicit((ccl_gpu_shared atomic_int *)p, memory_order_relaxed)

#    define CCL_LOCAL_MEM_FENCE mem_flags::mem_threadgroup
#    define ccl_barrier(flags) threadgroup_barrier(flags)

#  endif /* __KERNEL_METAL__ */

#  ifdef __KERNEL_ONEAPI__

ccl_device_inline float atomic_add_and_fetch_float(ccl_global float *p, const float x)
{
  sycl::atomic_ref<float,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::ext_intel_global_device_space>
      atomic(*p);
  return atomic.fetch_add(x);
}

ccl_device_inline float atomic_compare_and_swap_float(ccl_global float *source,
                                                      float old_val,
                                                      float new_val)
{
  sycl::atomic_ref<float,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::ext_intel_global_device_space>
      atomic(*source);
  atomic.compare_exchange_weak(old_val, new_val);
  return old_val;
}

ccl_device_inline unsigned int atomic_fetch_and_add_uint32(ccl_global unsigned int *p,
                                                           unsigned int x)
{
  sycl::atomic_ref<unsigned int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::ext_intel_global_device_space>
      atomic(*p);
  return atomic.fetch_add(x);
}

ccl_device_inline int atomic_fetch_and_add_uint32(ccl_global int *p, const int x)
{
  sycl::atomic_ref<int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::ext_intel_global_device_space>
      atomic(*p);
  return atomic.fetch_add(x);
}

ccl_device_inline int atomic_fetch_and_add_uint32_shared(int *p, const int x)
{
  sycl::atomic_ref<int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::local_space>
      atomic(*p);
  return atomic.fetch_add(x);
}

ccl_device_inline unsigned int atomic_fetch_and_sub_uint32(ccl_global unsigned int *p,
                                                           unsigned int x)
{
  sycl::atomic_ref<unsigned int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::ext_intel_global_device_space>
      atomic(*p);
  return atomic.fetch_sub(x);
}

ccl_device_inline int atomic_fetch_and_sub_uint32(ccl_global int *p, const int x)
{
  sycl::atomic_ref<int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::ext_intel_global_device_space>
      atomic(*p);
  return atomic.fetch_sub(x);
}

ccl_device_inline unsigned int atomic_fetch_and_inc_uint32(ccl_global unsigned int *p)
{
  return atomic_fetch_and_add_uint32(p, 1);
}

ccl_device_inline int atomic_fetch_and_inc_uint32(ccl_global int *p)
{
  return atomic_fetch_and_add_uint32(p, 1);
}

ccl_device_inline unsigned int atomic_fetch_and_dec_uint32(ccl_global unsigned int *p)
{
  return atomic_fetch_and_sub_uint32(p, 1);
}

ccl_device_inline int atomic_fetch_and_dec_uint32(ccl_global int *p)
{
  return atomic_fetch_and_sub_uint32(p, 1);
}

ccl_device_inline unsigned int atomic_fetch_and_or_uint32(ccl_global unsigned int *p,
                                                          unsigned int x)
{
  sycl::atomic_ref<unsigned int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::ext_intel_global_device_space>
      atomic(*p);
  return atomic.fetch_or(x);
}

ccl_device_inline int atomic_fetch_and_or_uint32(ccl_global int *p, const int x)
{
  sycl::atomic_ref<int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::ext_intel_global_device_space>
      atomic(*p);
  return atomic.fetch_or(x);
}

ccl_device_inline void atomic_store_local(int *p, const int x)
{
  sycl::atomic_ref<int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::local_space>
      atomic(*p);
  atomic.store(x);
}

ccl_device_inline int atomic_load_local(int *p)
{
  sycl::atomic_ref<int,
                   sycl::memory_order::relaxed,
                   sycl::memory_scope::device,
                   sycl::access::address_space::local_space>
      atomic(*p);
  return atomic.load();
}

#  endif /* __KERNEL_ONEAPI__ */

#endif /* __KERNEL_GPU__ */
