/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"
#include "util/math_float3.h"
#include "util/types_base.h"
#include "util/types_float3.h"

#if !defined(__KERNEL_GPU__)
#  include <cstring> /* for memset(). */
#endif

CCL_NAMESPACE_BEGIN

/* A packed representation of spherical harmonics.
 * Supports spherical harmonics for degrees 1 to 3. The values are quantized to 8 bit.
 * Spherical harmonics for degree 0 is stored in the base radiance attribute.
 *
 * The order is: <L=1 M=-1>, <L=1 M=0>, <L=1 M=1>, <L=2 M=-2> ...
 * where L is the band index, M is the  basis function.
 *
 * NOTE: There is an utility spherical_harmonics_get<L, M> to access bands in a more semantic way.
 */
struct PackedSphericalHarmonics {
  /* Some formats (like SPZ) define the maximum degree as 4.
   * However, in practice it is hard to run across dataset that is actually trained using such
   * high degree, and the difference that the higher degree brings is probably barely perceivable.
   * To keep logic simple, assume the most commonly used degree of 3, and allocate coefficients for
   * it. */
  ccl_static_constexpr int MAX_DEGREES = 3;
  ccl_static_constexpr int MAX_COEFFICIENTS = 15;

  uint8_t coefficients[3 * MAX_COEFFICIENTS];

  /* NOTE: Can not have methods here, as the kernel references data from global memory, which makes
   * it tricky on Metal that either doesn't support it or requires some non-trivial way, as calling
   * methods from a `ccl_global PackedSphericalHarmonics *` conflicts with the address space for
   * `this`. */
};

/* Set all spherical harmonics values to 0. */
ccl_device_forceinline void spherical_harmonics_fill_zero(
    ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
#if !defined(__KERNEL_GPU__)
  memset(
      packed_spherical_harmonics.coefficients, 0, sizeof(packed_spherical_harmonics.coefficients));
#else
  /* Metal does not support memset() call. */
  for (int i = 0; i < 3 * PackedSphericalHarmonics::MAX_COEFFICIENTS; ++i) {
    packed_spherical_harmonics.coefficients[i] = 0;
  }
#endif
}

/* Quantize spherical harmonics coefficients from float to uint8_t.
 * Similar to the float_to_byte(), but deals with input values in the range [-1 .. 1]. */
ccl_device_forceinline uint8_t quantize_spherical_harmonics(const float value)
{
  const float rescaled = saturatef((value + 1.0f) / 2.0f);
  return ((rescaled <= 0.0f) ? 0 :
                               ((rescaled > (1.0f - 0.5f / 255.0f)) ?
                                    255 :
                                    (uint8_t)((255.0f * rescaled) + 0.5f)));  // NOLINT
}

ccl_device_forceinline float unquantize_spherical_harmonics(const uint8_t value)
{
  return float(value) * (1.0f / 255.0f) * 2.0f - 1.0f;
}

/* Set the spherical harmonic coefficient by its flat index. */
ccl_device_forceinline void spherical_harmonics_set_coefficient(
    ccl_global PackedSphericalHarmonics &packed_spherical_harmonics,
    const int index,
    const float3 value)
{
  util_assert(index >= 0);
  util_assert(index < PackedSphericalHarmonics::MAX_COEFFICIENTS);
  const int i = index * 3;
  packed_spherical_harmonics.coefficients[i + 0] = quantize_spherical_harmonics(value.x);
  packed_spherical_harmonics.coefficients[i + 1] = quantize_spherical_harmonics(value.y);
  packed_spherical_harmonics.coefficients[i + 2] = quantize_spherical_harmonics(value.z);
}

/* Get unquantized spherical harmonics coefficient by its flat index. */
ccl_device_forceinline float3 spherical_harmonics_get_coefficient(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics, const int index)
{
  util_assert(index >= 0);
  util_assert(index < PackedSphericalHarmonics::MAX_COEFFICIENTS);
  const int i = index * 3;
  return make_float3(
      unquantize_spherical_harmonics(packed_spherical_harmonics.coefficients[i + 0]),
      unquantize_spherical_harmonics(packed_spherical_harmonics.coefficients[i + 1]),
      unquantize_spherical_harmonics(packed_spherical_harmonics.coefficients[i + 2]));
}

/* Get unquantized spherical harmonics coefficient by the band L and the basis function M. */
template<int L, int M>
ccl_device_inline float3
spherical_harmonics_get(const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics);

/* Band L=1. */
ccl_device_template_spec float3 spherical_harmonics_get<1, -1>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 0);
}
ccl_device_template_spec float3 spherical_harmonics_get<1, 0>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 1);
}
ccl_device_template_spec float3 spherical_harmonics_get<1, 1>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 2);
}

/* Band L=2. */
ccl_device_template_spec float3 spherical_harmonics_get<2, -2>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 3);
}
ccl_device_template_spec float3 spherical_harmonics_get<2, -1>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 4);
}
ccl_device_template_spec float3 spherical_harmonics_get<2, 0>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 5);
}
ccl_device_template_spec float3 spherical_harmonics_get<2, 1>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 6);
}
ccl_device_template_spec float3 spherical_harmonics_get<2, 2>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 7);
}

/* Band L=3. */
ccl_device_template_spec float3 spherical_harmonics_get<3, -3>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 8);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, -2>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 9);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, -1>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 10);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, 0>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 11);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, 1>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 12);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, 2>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 13);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, 3>(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics)
{
  return spherical_harmonics_get_coefficient(packed_spherical_harmonics, 14);
}

CCL_NAMESPACE_END
