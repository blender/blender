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

/* A packed representation of spherical harmonics for bands 1 to 3.
 *
 * The spherical harmonics for band=0 is stored as a separate attribute. Such division allows for
 * slightly better packing with opacity, and also allows to have higher bands optional.
 *
 * The naming is inspired by the storage of gaussians splats in the PLY format, where the band=0
 * is stored as f_dc, and the higher bands are stored as f_rest.
 *
 * The values are quantized to 8 bit.
 *
 * The order is: <L=1 M=-1>, <L=1 M=0>, <L=1 M=1>, <L=2 M=-2> ...
 * where L is the band index, M is the  basis function.
 *
 * NOTE: There is an utility spherical_harmonics_get<L, M> to access bands in a more semantic way.
 */
struct PackedSphericalHarmonicsRest {
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
   * methods from a `ccl_global PackedSphericalHarmonicsRest *` conflicts with the address space
   * for `this`. */
};

/* Set all spherical harmonics values to 0. */
ccl_device_forceinline void spherical_harmonics_rest_fill_zero(
    ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
#if !defined(__KERNEL_GPU__)
  memset(packed_spherical_harmonics_rest.coefficients,
         0,
         sizeof(packed_spherical_harmonics_rest.coefficients));
#else
  /* Metal does not support memset() call. */
  for (int i = 0; i < 3 * PackedSphericalHarmonicsRest::MAX_COEFFICIENTS; ++i) {
    packed_spherical_harmonics_rest.coefficients[i] = 0;
  }
#endif
}

/* Quantize spherical harmonics coefficients from float to uint8_t.
 * Similar to the float_to_byte(), but deals with input values in the range [-1 .. 1]. */
ccl_device_forceinline uint8_t quantize_spherical_harmonics(const float value)
{
  const float rescaled = saturatef((value + 1.0f) / 2.0f);
  return float_to_byte(rescaled);
}

ccl_device_forceinline float unquantize_spherical_harmonics(const uint8_t value)
{
  return byte_to_float(value) * 2.0f - 1.0f;
}

/* Set the spherical harmonic coefficient by its flat index. */
ccl_device_forceinline void spherical_harmonics_rest_set_coefficient(
    ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest,
    const int index,
    const float3 value)
{
  util_assert(index >= 0);
  util_assert(index < PackedSphericalHarmonicsRest::MAX_COEFFICIENTS);
  const int i = index * 3;
  packed_spherical_harmonics_rest.coefficients[i + 0] = quantize_spherical_harmonics(value.x);
  packed_spherical_harmonics_rest.coefficients[i + 1] = quantize_spherical_harmonics(value.y);
  packed_spherical_harmonics_rest.coefficients[i + 2] = quantize_spherical_harmonics(value.z);
}

/* Get unquantized spherical harmonics coefficient by its flat index. */
ccl_device_forceinline float3 spherical_harmonics_rest_get_coefficient(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest,
    const int index)
{
  util_assert(index >= 0);
  util_assert(index < PackedSphericalHarmonicsRest::MAX_COEFFICIENTS);
  const int i = index * 3;
  return make_float3(
      unquantize_spherical_harmonics(packed_spherical_harmonics_rest.coefficients[i + 0]),
      unquantize_spherical_harmonics(packed_spherical_harmonics_rest.coefficients[i + 1]),
      unquantize_spherical_harmonics(packed_spherical_harmonics_rest.coefficients[i + 2]));
}

/* Get unquantized spherical harmonics coefficient by the band L and the basis function M. */
template<int L, int M>
ccl_device_inline float3 spherical_harmonics_get(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest);

/* Band L=1. */
ccl_device_template_spec float3 spherical_harmonics_get<1, -1>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 0);
}
ccl_device_template_spec float3 spherical_harmonics_get<1, 0>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 1);
}
ccl_device_template_spec float3 spherical_harmonics_get<1, 1>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 2);
}

/* Band L=2. */
ccl_device_template_spec float3 spherical_harmonics_get<2, -2>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 3);
}
ccl_device_template_spec float3 spherical_harmonics_get<2, -1>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 4);
}
ccl_device_template_spec float3 spherical_harmonics_get<2, 0>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 5);
}
ccl_device_template_spec float3 spherical_harmonics_get<2, 1>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 6);
}
ccl_device_template_spec float3 spherical_harmonics_get<2, 2>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 7);
}

/* Band L=3. */
ccl_device_template_spec float3 spherical_harmonics_get<3, -3>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 8);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, -2>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 9);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, -1>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 10);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, 0>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 11);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, 1>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 12);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, 2>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 13);
}
ccl_device_template_spec float3 spherical_harmonics_get<3, 3>(
    const ccl_global PackedSphericalHarmonicsRest &packed_spherical_harmonics_rest)
{
  return spherical_harmonics_rest_get_coefficient(packed_spherical_harmonics_rest, 14);
}

CCL_NAMESPACE_END
