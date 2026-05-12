/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/types.h"

#include "kernel/svm/types.h"

CCL_NAMESPACE_BEGIN

/* Stack Load */

ccl_device_inline float stack_load_float(const ccl_private float *stack, const uint a)
{
  kernel_assert(a < SVM_STACK_SIZE);

  return stack[a];
}

ccl_device_inline float stack_load_float_default(const ccl_private float *stack,
                                                 const uint a,
                                                 const float value)
{
  return (a == (uint)SVM_STACK_INVALID) ? value : stack_load_float(stack, a);
}

ccl_device_inline float3 stack_load_float3(const ccl_private float *stack, const uint a)
{
  kernel_assert(a + 2 < SVM_STACK_SIZE);

  const ccl_private float *stack_a = stack + a;
  return make_float3(stack_a[0], stack_a[1], stack_a[2]);
}

ccl_device_inline float3 stack_load_float3_default(const ccl_private float *stack,
                                                   const uint a,
                                                   const float3 value)
{
  return (a == (uint)SVM_STACK_INVALID) ? value : stack_load_float3(stack, a);
}

ccl_device_inline int stack_load_int(const ccl_private float *stack, const uint a)
{
  kernel_assert(a < SVM_STACK_SIZE);

  return __float_as_int(stack[a]);
}

/* Type-based stack load. T can be float, float3, dual1, or dual3.
 * When T is a dual type, derivatives are loaded from adjacent stack slots. */

template<typename T> ccl_device_inline T stack_load(const ccl_private float *stack, const uint a);

ccl_device_template_spec float stack_load(const ccl_private float *stack, const uint a)
{
  return stack_load_float(stack, a);
}

ccl_device_template_spec float3 stack_load(const ccl_private float *stack, const uint a)
{
  return stack_load_float3(stack, a);
}

ccl_device_template_spec dual1 stack_load(const ccl_private float *stack, const uint a)
{
  return {
      stack_load_float(stack, a), stack_load_float(stack, a + 1), stack_load_float(stack, a + 2)};
}

ccl_device_template_spec dual3 stack_load(const ccl_private float *stack, const uint a)
{
  return {stack_load_float3(stack, a),
          stack_load_float3(stack, a + 3),
          stack_load_float3(stack, a + 6)};
}

/* Load from SVMInputFloat and SVMInputFloat3. With template versions to support duals
 * for loading derivatives from adjacent stack slots. */

ccl_device_inline float stack_load(const ccl_private float *ccl_restrict stack,
                                   const SVMInputFloat v)
{
  if ((v.bits >> 8) == (SVM_INPUT_STACK_OFFSET_MASK >> 8)) {
    return stack_load_float(stack, v.bits & 0xFFu);
  }
  return __uint_as_float(v.bits);
}

ccl_device_inline float3 stack_load(const ccl_private float *ccl_restrict stack,
                                    const SVMInputFloat3 v)
{
  if ((v.x.bits >> 8) == (SVM_INPUT_STACK_OFFSET_MASK >> 8)) {
    return stack_load_float3(stack, v.x.bits & 0xFFu);
  }
  return make_float3(
      __uint_as_float(v.x.bits), __uint_as_float(v.y.bits), __uint_as_float(v.z.bits));
}

template<typename T>
ccl_device_inline T stack_load(const ccl_private float *stack, const SVMInputFloat v);

ccl_device_template_spec float stack_load(const ccl_private float *stack, const SVMInputFloat v)
{
  return stack_load(stack, v);
}

ccl_device_template_spec dual1 stack_load(const ccl_private float *stack, const SVMInputFloat v)
{
  if ((v.bits >> 8) == (SVM_INPUT_STACK_OFFSET_MASK >> 8)) {
    return stack_load<dual1>(stack, v.bits & 0xFFu);
  }
  return dual1(__uint_as_float(v.bits));
}

template<typename T>
ccl_device_inline T stack_load(const ccl_private float *stack, const SVMInputFloat3 v);

ccl_device_template_spec float3 stack_load(const ccl_private float *stack, const SVMInputFloat3 v)
{
  return stack_load(stack, v);
}

ccl_device_template_spec dual3 stack_load(const ccl_private float *stack, const SVMInputFloat3 v)
{
  if ((v.x.bits >> 8) == (SVM_INPUT_STACK_OFFSET_MASK >> 8)) {
    return stack_load<dual3>(stack, v.x.bits & 0xFFu);
  }
  return dual3(make_float3(
      __uint_as_float(v.x.bits), __uint_as_float(v.y.bits), __uint_as_float(v.z.bits)));
}

/* Stack Store */

ccl_device_inline void stack_store_float(ccl_private float *stack, const uint a, const float f)
{
  kernel_assert(a < SVM_STACK_SIZE);

  stack[a] = f;
}

ccl_device_inline void stack_store_float3(ccl_private float *stack, const uint a, const float3 f)
{
  kernel_assert(a + 2 < SVM_STACK_SIZE);
  copy_v3_v3(stack + a, f);
}

ccl_device_inline void stack_store_int(ccl_private float *stack, const uint a, const int i)
{
  kernel_assert(a < SVM_STACK_SIZE);

  stack[a] = __int_as_float(i);
}

/* Type-based stack store. Overloaded for plain and dual types.
 * For dual types, derivatives are stored in adjacent stack slots. */

ccl_device_inline void stack_store(ccl_private float *stack, const uint a, const float f)
{
  stack_store_float(stack, a, f);
}

ccl_device_inline void stack_store(ccl_private float *stack, const uint a, const float3 f)
{
  stack_store_float3(stack, a, f);
}

ccl_device_inline void stack_store(ccl_private float *stack, const uint a, const dual1 f)
{
  stack_store_float(stack, a, f.val);
  stack_store_float(stack, a + 1, f.dx);
  stack_store_float(stack, a + 2, f.dy);
}

ccl_device_inline void stack_store(ccl_private float *stack, const uint a, const dual3 f)
{
  stack_store_float3(stack, a, f.val);
  stack_store_float3(stack, a + 3, f.dx);
  stack_store_float3(stack, a + 6, f.dy);
}

/* Stack Utility */

ccl_device_inline bool stack_valid(const uint a)
{
  return a != (uint)SVM_STACK_INVALID;
}

/* Reading Nodes */

/* Read a typed node struct directly from the SVM byte-code stream. The struct T must be a
 * multiple of sizeof(uint) and its memory layout must match the byte-code encoding. Returns
 * a const reference into the byte-code array and advances the offset past the struct. */
template<typename T>
ccl_device_inline const ccl_global T &svm_node_get(KernelGlobals kg, ccl_private int *const offset)
{
  static_assert(alignof(T) <= alignof(uint));
  static_assert(sizeof(T) % sizeof(uint) == 0);
  const ccl_global T &node = *reinterpret_cast<const ccl_global T *>(
      &kernel_data_fetch(svm_nodes, *offset));
  *offset += sizeof(T) / sizeof(uint);
  return node;
}

ccl_device_inline float4 svm_node_get_data_float4(KernelGlobals kg, const int offset)
{
  return make_float4(__uint_as_float(kernel_data_fetch(svm_nodes, offset)),
                     __uint_as_float(kernel_data_fetch(svm_nodes, offset + 1)),
                     __uint_as_float(kernel_data_fetch(svm_nodes, offset + 2)),
                     __uint_as_float(kernel_data_fetch(svm_nodes, offset + 3)));
}

/* Shading Helpers */

ccl_device_forceinline float3 dPdx(const ccl_private ShaderData *sd)
{
  return sd->dPdu * sd->du.dx + sd->dPdv * sd->dv.dx;
}

ccl_device_forceinline float3 dPdy(const ccl_private ShaderData *sd)
{
  return sd->dPdu * sd->du.dy + sd->dPdv * sd->dv.dy;
}

/* Shading position, returns Float3Type = float3 (no derivatives) or dual3 (with derivatives). */

template<typename Float3Type>
ccl_device_inline Float3Type shading_position(const ccl_private ShaderData *sd)
{
  if constexpr (is_dual_v<Float3Type>) {
    dual3 P(sd->P);
    P.dx = dPdx(sd);
    P.dy = dPdy(sd);
    return P;
  }
  else {
    return sd->P;
  }
}

/* Shading incoming direction, returns Float3Type = float3 or dual3. */

template<typename Float3Type>
ccl_device_inline Float3Type shading_incoming(const ccl_private ShaderData *sd)
{
  if constexpr (is_dual_v<Float3Type>) {
    dual3 I(sd->wi);
    float3 dIdx, dIdy;
    make_orthonormals(sd->wi, &dIdx, &dIdy);
    I.dx = sd->dI * dIdx;
    I.dy = sd->dI * dIdy;
    return I;
  }
  else {
    return sd->wi;
  }
}

CCL_NAMESPACE_END
