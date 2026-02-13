/* SPDX-FileCopyrightText: 2025 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/types_float2.h"
#include "util/types_float3.h"
#include "util/types_float4.h"

CCL_NAMESPACE_BEGIN

template<class T> struct dual {
  T val = T(), dx = T(), dy = T();
  dual() = default;
  ccl_device_inline_method explicit dual(const T val) : val(val) {}
  ccl_device_inline_method dual(const T val, const T dx, const T dy) : val(val), dx(dx), dy(dy) {}
};

template<> struct dual<float> {
  float val = 0.0f;
  float dx = 0.0f;
  float dy = 0.0f;
  dual() = default;
  ccl_device_inline_method explicit dual(const float val) : val(val) {}
  ccl_device_inline_method dual(const float val, const float dx, const float dy)
      : val(val), dx(dx), dy(dy)
  {
  }
};

template<> struct dual<float2> {
  float2 val = make_float2(0.0f);
  float2 dx = make_float2(0.0f);
  float2 dy = make_float2(0.0f);
  dual() = default;
  ccl_device_inline_method explicit dual(const float2 val) : val(val) {}
  ccl_device_inline_method dual(const float2 val, const float2 dx, const float2 dy)
      : val(val), dx(dx), dy(dy)
  {
  }
  ccl_device_inline_method dual<float> x() const
  {
    return {val.x, dx.x, dy.x};
  }
  ccl_device_inline_method dual<float> y() const
  {
    return {val.y, dx.y, dy.y};
  }
};

template<> struct dual<float3> {
  float3 val = make_float3(0.0f);
  float3 dx = make_float3(0.0f);
  float3 dy = make_float3(0.0f);
  dual() = default;
  ccl_device_inline_method explicit dual(const float3 val) : val(val) {}
  ccl_device_inline_method dual(const float3 val, const float3 dx, const float3 dy)
      : val(val), dx(dx), dy(dy)
  {
  }
  ccl_device_inline_method dual<float> x() const
  {
    return {val.x, dx.x, dy.x};
  }
  ccl_device_inline_method dual<float> y() const
  {
    return {val.y, dx.y, dy.y};
  }
  ccl_device_inline_method dual<float> z() const
  {
    return {val.z, dx.z, dy.z};
  }
};

template<> struct dual<float4> {
  float4 val = make_float4(0.0f);
  float4 dx = make_float4(0.0f);
  float4 dy = make_float4(0.0f);
  dual() = default;
  ccl_device_inline_method explicit dual(const float4 val) : val(val) {}
  ccl_device_inline_method dual(const float4 val, const float4 dx, const float4 dy)
      : val(val), dx(dx), dy(dy)
  {
  }
  ccl_device_inline_method dual<float> x() const
  {
    return {val.x, dx.x, dy.x};
  }
  ccl_device_inline_method dual<float> y() const
  {
    return {val.y, dx.y, dy.y};
  }
  ccl_device_inline_method dual<float> z() const
  {
    return {val.z, dx.z, dy.z};
  }
  ccl_device_inline_method dual<float> w() const
  {
    return {val.w, dx.w, dy.w};
  }
};

using dual1 = dual<float>;
using dual2 = dual<float2>;
using dual3 = dual<float3>;
using dual4 = dual<float4>;

ccl_device_inline dual2 make_float2(const dual3 a)
{
  return {make_float2(a.val), make_float2(a.dx), make_float2(a.dy)};
}

ccl_device_inline dual2 make_float2(const dual1 a, const dual1 b)
{
  return {make_float2(a.val, b.val), make_float2(a.dx, b.dx), make_float2(a.dy, b.dy)};
}

ccl_device_inline dual3 make_float3(const dual1 a, const dual1 b, const dual1 c)
{
  return {make_float3(a.val, b.val, c.val),
          make_float3(a.dx, b.dx, c.dx),
          make_float3(a.dy, b.dy, c.dy)};
}

template<class T> ccl_device_inline dual3 make_float3(const ccl_private dual<T> &a)
{
  return {make_float3(a.val), make_float3(a.dx), make_float3(a.dy)};
}

ccl_device_inline dual4 make_float4(const dual1 a, const dual1 b, const dual1 c, const dual1 d)
{
  return {make_float4(a.val, b.val, c.val, d.val),
          make_float4(a.dx, b.dx, c.dx, d.dx),
          make_float4(a.dy, b.dy, c.dy, d.dy)};
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
