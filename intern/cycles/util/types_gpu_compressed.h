/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/half.h"
#include "util/types_spectrum.h"

CCL_NAMESPACE_BEGIN

/* Compact storage of data types on GPU, stored in full precision on CPU.
 * Under the assumption that reducing state size is important on GPU and
 * math is relatively cheap. */

/* A float stored in half precision. */
struct FloatCompressedOnGPU {
  using value_type = float;
  using compressed_type = half;

#ifdef __KERNEL_GPU__
  half v;
#else
  float v;
#endif

  FloatCompressedOnGPU() = default;

  ccl_device_inline_method FloatCompressedOnGPU(const float a)
  {
#ifdef __KERNEL_GPU__
    v = float_to_half(a);
#else
    v = a;
#endif
  }

  ccl_device_inline_method operator float() const ccl_global
  {
#ifdef __KERNEL_GPU__
    return half_to_float(v);
#else
    return v;
#endif
  }

  ccl_device_inline_method ccl_global FloatCompressedOnGPU &operator=(const float a) ccl_global
  {
#ifdef __KERNEL_GPU__
    v = float_to_half(a);
#else
    v = a;
#endif
    return *this;
  }

  ccl_device_inline_method ccl_global FloatCompressedOnGPU &operator=(FloatCompressedOnGPU a)
      ccl_global
  {
    v = a.v;
    return *this;
  }

#ifdef __KERNEL_METAL__
  ccl_device_inline_method operator float() const ccl_private
  {
#  ifdef __KERNEL_GPU__
    return half_to_float(v);
#  else
    return v;
#  endif
  }

  ccl_device_inline_method ccl_private FloatCompressedOnGPU &operator=(const float a) ccl_private
  {
#  ifdef __KERNEL_GPU__
    v = float_to_half(a);
#  else
    v = a;
#  endif
    return *this;
  }

  ccl_device_inline_method ccl_private FloatCompressedOnGPU &operator=(FloatCompressedOnGPU a)
      ccl_private
  {
    v = a.v;
    return *this;
  }
#endif
};

/* Spectrum stored in half precision. */
struct SpectrumCompressedOnGPU {
  using value_type = Spectrum;
  using compressed_type = packed_half3;

#ifdef __KERNEL_GPU__
  packed_half3 v;
#else
  Spectrum v;
#endif

  SpectrumCompressedOnGPU() = default;

  ccl_device_inline_method SpectrumCompressedOnGPU(const Spectrum a)
  {
#ifdef __KERNEL_GPU__
    v = float3_to_packed_half3(a);
#else
    v = a;
#endif
  }

  ccl_device_inline_method operator Spectrum() const ccl_global
  {
#ifdef __KERNEL_GPU__
    return packed_half3_to_float3(v);
#else
    return v;
#endif
  }

  ccl_device_inline_method ccl_global SpectrumCompressedOnGPU &operator=(const Spectrum a)
      ccl_global
  {
#ifdef __KERNEL_GPU__
    v = float3_to_packed_half3(a);
#else
    v = a;
#endif
    return *this;
  }

  ccl_device_inline_method ccl_global SpectrumCompressedOnGPU &operator=(SpectrumCompressedOnGPU a)
      ccl_global
  {
    v = a.v;
    return *this;
  }

#ifdef __KERNEL_METAL__
  ccl_device_inline_method operator Spectrum() const ccl_private
  {
#  ifdef __KERNEL_GPU__
    return packed_half3_to_float3(v);
#  else
    return v;
#  endif
  }

  ccl_device_inline_method ccl_private SpectrumCompressedOnGPU &operator=(const Spectrum a)
      ccl_private
  {
#  ifdef __KERNEL_GPU__
    v = float3_to_packed_half3(a);
#  else
    v = a;
#  endif
    return *this;
  }

  ccl_device_inline_method ccl_private SpectrumCompressedOnGPU &operator=(
      SpectrumCompressedOnGPU a) ccl_private
  {
    v = a.v;
    return *this;
  }
#endif
};

/* Templates for determining GPU storage type and size from the host. */
template<typename...> using gpu_void_t = void;
template<typename T, typename = void> struct gpu_state_storage {
  using gpu_type = T;
};
template<typename T> struct gpu_state_storage<T, gpu_void_t<typename T::compressed_type>> {
  using gpu_type = typename T::compressed_type;
};

CCL_NAMESPACE_END
