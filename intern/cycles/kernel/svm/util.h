/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/svm/types.h"

CCL_NAMESPACE_BEGIN

/* Stack */

ccl_device_inline float3 stack_load_float3(const ccl_private float *stack, const uint a)
{
  kernel_assert(a + 2 < SVM_STACK_SIZE);

  const ccl_private float *stack_a = stack + a;
  return make_float3(stack_a[0], stack_a[1], stack_a[2]);
}

ccl_device_inline void stack_store_float3(ccl_private float *stack, const uint a, const float3 f)
{
  kernel_assert(a + 2 < SVM_STACK_SIZE);

  ccl_private float *stack_a = stack + a;
  stack_a[0] = f.x;
  stack_a[1] = f.y;
  stack_a[2] = f.z;
}

ccl_device_inline float stack_load_float(const ccl_private float *stack, const uint a)
{
  kernel_assert(a < SVM_STACK_SIZE);

  return stack[a];
}

ccl_device_inline float stack_load_float_default(const ccl_private float *stack,
                                                 const uint a,
                                                 const uint value)
{
  return (a == (uint)SVM_STACK_INVALID) ? __uint_as_float(value) : stack_load_float(stack, a);
}

ccl_device_inline void stack_store_float(ccl_private float *stack, const uint a, const float f)
{
  kernel_assert(a < SVM_STACK_SIZE);

  stack[a] = f;
}

ccl_device_inline int stack_load_int(const ccl_private float *stack, const uint a)
{
  kernel_assert(a < SVM_STACK_SIZE);

  return __float_as_int(stack[a]);
}

ccl_device_inline int stack_load_int_default(ccl_private float *stack,
                                             const uint a,
                                             const uint value)
{
  return (a == (uint)SVM_STACK_INVALID) ? (int)value : stack_load_int(stack, a);
}

ccl_device_inline void stack_store_int(ccl_private float *stack, const uint a, const int i)
{
  kernel_assert(a < SVM_STACK_SIZE);

  stack[a] = __int_as_float(i);
}

ccl_device_inline bool stack_valid(const uint a)
{
  return a != (uint)SVM_STACK_INVALID;
}

/* Reading Nodes */

ccl_device_inline uint4 read_node(KernelGlobals kg, ccl_private int *const offset)
{
  uint4 node = kernel_data_fetch(svm_nodes, *offset);
  (*offset)++;
  return node;
}

ccl_device_inline float4 read_node_float(KernelGlobals kg, ccl_private int *const offset)
{
  const uint4 node = kernel_data_fetch(svm_nodes, *offset);
  const float4 f = make_float4(__uint_as_float(node.x),
                               __uint_as_float(node.y),
                               __uint_as_float(node.z),
                               __uint_as_float(node.w));
  (*offset)++;
  return f;
}

ccl_device_inline float4 fetch_node_float(KernelGlobals kg, const int offset)
{
  const uint4 node = kernel_data_fetch(svm_nodes, offset);
  return make_float4(__uint_as_float(node.x),
                     __uint_as_float(node.y),
                     __uint_as_float(node.z),
                     __uint_as_float(node.w));
}

ccl_device_forceinline void svm_unpack_node_uchar2(const uint i,
                                                   ccl_private uint *x,
                                                   ccl_private uint *y)
{
  *x = (i & 0xFF);
  *y = ((i >> 8) & 0xFF);
}

ccl_device_forceinline void svm_unpack_node_uchar3(const uint i,
                                                   ccl_private uint *x,
                                                   ccl_private uint *y,
                                                   ccl_private uint *z)
{
  *x = (i & 0xFF);
  *y = ((i >> 8) & 0xFF);
  *z = ((i >> 16) & 0xFF);
}

ccl_device_forceinline void svm_unpack_node_uchar4(const uint i,
                                                   ccl_private uint *x,
                                                   ccl_private uint *y,
                                                   ccl_private uint *z,
                                                   ccl_private uint *w)
{
  *x = (i & 0xFF);
  *y = ((i >> 8) & 0xFF);
  *z = ((i >> 16) & 0xFF);
  *w = ((i >> 24) & 0xFF);
}

CCL_NAMESPACE_END
