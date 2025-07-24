/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_float2.h"
#include "util/types_float3.h"
#include "util/types_float4.h"

CCL_NAMESPACE_BEGIN

template<class T> struct dual {
  T val, dx, dy;
  dual<T>() = default;
  ccl_device_inline_method explicit dual<T>(const T val) : val(val) {}
  ccl_device_inline_method dual<T>(const T val, const T dx, const T dy) : val(val), dx(dx), dy(dy)
  {
  }
};

template<> struct dual<float2> {
  float2 val = make_float2(0.0f);
  float2 dx = make_float2(0.0f);
  float2 dy = make_float2(0.0f);
  dual<float2>() = default;
  ccl_device_inline_method explicit dual<float2>(const float2 val) : val(val) {}
  ccl_device_inline_method dual<float2>(const float2 val, const float2 dx, const float2 dy)
      : val(val), dx(dx), dy(dy)
  {
  }
};

template<> struct dual<float3> {
  float3 val = make_float3(0.0f);
  float3 dx = make_float3(0.0f);
  float3 dy = make_float3(0.0f);
  dual<float3>() = default;
  ccl_device_inline_method explicit dual<float3>(const float3 val) : val(val) {}
  ccl_device_inline_method dual<float3>(const float3 val, const float3 dx, const float3 dy)
      : val(val), dx(dx), dy(dy)
  {
  }
};

template<> struct dual<float4> {
  float4 val = make_float4(0.0f);
  float4 dx = make_float4(0.0f);
  float4 dy = make_float4(0.0f);
  dual<float4>() = default;
  ccl_device_inline_method explicit dual<float4>(const float4 val) : val(val) {}
  ccl_device_inline_method dual<float4>(const float4 val, const float4 dx, const float4 dy)
      : val(val), dx(dx), dy(dy)
  {
  }
};

using dual1 = dual<float>;
using dual2 = dual<float2>;
using dual3 = dual<float3>;
using dual4 = dual<float4>;

template<class T> ccl_device_inline dual3 make_float3(const ccl_private dual<T> &a)
{
  return {make_float3(a.val), make_float3(a.dx), make_float3(a.dy)};
}

ccl_device_inline dual3 make_float3(const dual1 a, const dual1 b, const dual1 c)
{
  return {make_float3(a.val, b.val, c.val),
          make_float3(a.dx, b.dx, c.dx),
          make_float3(a.dy, b.dy, c.dy)};
}

ccl_device_inline dual4 make_float4(const dual3 a)
{
  return {make_float4(a.val), make_float4(a.dx, 0.0f), make_float4(a.dy, 0.0f)};
}

ccl_device_inline dual4 make_homogeneous(const dual3 a)
{
  return {make_float4(a.val, 1.0f), make_float4(a.dx, 0.0f), make_float4(a.dy, 0.0f)};
}

ccl_device_inline void print_dual1(const ccl_private char *label, const dual1 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: {\nval = %.8f\n dx = %.8f\n dy = %.8f\n}\n",
         label,
         (double)a.val,
         (double)a.dx,
         (double)a.dy);
#else
  (void)label;
  (void)a;
#endif
}

ccl_device_inline void print_dual2(const ccl_private char *label, const dual2 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: {\nval = %.8f %.8f\n dx = %.8f %.8f\n dy = %.8f %.8f\n}\n",
         label,
         (double)a.val.x,
         (double)a.val.y,
         (double)a.dx.x,
         (double)a.dx.y,
         (double)a.dy.x,
         (double)a.dy.y);
#else
  (void)label;
  (void)a;
#endif
}

ccl_device_inline void print_dual3(const ccl_private char *label, const dual3 a)
{
#ifdef __KERNEL_PRINTF__
  printf("%s: {\nval = %.8f %.8f %.8f\n dx = %.8f %.8f %.8f\n dy = %.8f %.8f %.8f\n}\n",
         label,
         (double)a.val.x,
         (double)a.val.y,
         (double)a.val.z,
         (double)a.dx.x,
         (double)a.dx.y,
         (double)a.dx.z,
         (double)a.dy.x,
         (double)a.dy.y,
         (double)a.dy.z);
#else
  (void)label;
  (void)a;
#endif
}

CCL_NAMESPACE_END
