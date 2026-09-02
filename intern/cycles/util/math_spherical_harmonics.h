/* SPDX-FileCopyrightText: 2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Evaluation of spherical harmonics.
 *
 * References:
 *
 *   [sloan2010] Peter-Pike Sloan
 *               Stupid Spherical Harmonics (SH) Trick
 *               Game Developers Conference 2008, February 2008 (updated 2/10/2010)
 */

#pragma once

#include "util/math_base.h"
#include "util/types_float3.h"
#include "util/types_spherical_harmonics.h"

CCL_NAMESPACE_BEGIN

/* The polynomial forms of the spherical basis functions.
 * [sloan2010] Appendix A2. */
template<int L, int M> ccl_device_inline float spherical_harmonics_polynomial_form(float3 v);

/* Band L=0. */
ccl_device_template_spec float spherical_harmonics_polynomial_form<0, 0>(const float3 /*v*/)
{
  return 0.282094792f;
}

/* Band L=1. */
ccl_device_template_spec float spherical_harmonics_polynomial_form<1, -1>(const float3 v)
{
  return -0.488602512f * v.y;
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<1, 0>(const float3 v)
{
  return 0.488602512f * v.z;
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<1, 1>(const float3 v)
{
  return -0.488602512f * v.x;
}

/* Band L=2. */
ccl_device_template_spec float spherical_harmonics_polynomial_form<2, -2>(const float3 v)
{
  return 1.092548431f * (v.x * v.y);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<2, -1>(const float3 v)
{
  return -1.092548431f * (v.y * v.z);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<2, 0>(const float3 v)
{
  return 0.315391565f * (3.0f * v.z * v.z - 1.0f);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<2, 1>(const float3 v)
{
  return -1.092548431f * (v.x * v.z);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<2, 2>(const float3 v)
{
  return 0.546274215f * (v.x * v.x - v.y * v.y);
}

/* Band L=3. */
ccl_device_template_spec float spherical_harmonics_polynomial_form<3, -3>(const float3 v)
{
  return -0.590043589f * v.y * (3.0f * v.x * v.x - v.y * v.y);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<3, -2>(const float3 v)
{
  return 2.890611442f * (v.y * v.x * v.z);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<3, -1>(const float3 v)
{
  return -0.457045799f * v.y * (-1.0f + 5.0f * v.z * v.z);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<3, 0>(const float3 v)
{
  return 0.373176332f * v.z * (5.0f * v.z * v.z - 3.0f);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<3, 1>(const float3 v)
{
  return -0.457045799f * v.x * (-1.0f + 5.0f * v.z * v.z);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<3, 2>(const float3 v)
{
  return 1.445305721f * v.z * (v.x * v.x - v.y * v.y);
}
ccl_device_template_spec float spherical_harmonics_polynomial_form<3, 3>(const float3 v)
{
  return -0.5900435899f * v.x * (v.x * v.x - 3.0f * v.y * v.y);
}

template<class T>
ccl_device_forceinline T spherical_harmonics_evaluate_band_L0(const float3 direction, const T M0)
{
  return spherical_harmonics_polynomial_form<0, 0>(direction) * M0;
}

template<class T>
ccl_device_forceinline T
spherical_harmonics_evaluate_band_L1(const float3 direction, const T Mn1, const T M0, const T Mp1)
{
  return spherical_harmonics_polynomial_form<1, -1>(direction) * Mn1 +
         spherical_harmonics_polynomial_form<1, 0>(direction) * M0 +
         spherical_harmonics_polynomial_form<1, 1>(direction) * Mp1;
}

template<class T>
ccl_device_forceinline T spherical_harmonics_evaluate_band_L2(
    const float3 direction, const T Mn2, const T Mn1, const T M0, const T Mp1, const T Mp2)
{
  return spherical_harmonics_polynomial_form<2, -2>(direction) * Mn2 +
         spherical_harmonics_polynomial_form<2, -1>(direction) * Mn1 +
         spherical_harmonics_polynomial_form<2, 0>(direction) * M0 +
         spherical_harmonics_polynomial_form<2, 1>(direction) * Mp1 +
         spherical_harmonics_polynomial_form<2, 2>(direction) * Mp2;
}

template<class T>
ccl_device_forceinline T spherical_harmonics_evaluate_band_L3(const float3 direction,
                                                              const T Mn3,
                                                              const T Mn2,
                                                              const T Mn1,
                                                              const T M0,
                                                              const T Mp1,
                                                              const T Mp2,
                                                              const T Mp3)
{
  return spherical_harmonics_polynomial_form<3, -3>(direction) * Mn3 +
         spherical_harmonics_polynomial_form<3, -2>(direction) * Mn2 +
         spherical_harmonics_polynomial_form<3, -1>(direction) * Mn1 +
         spherical_harmonics_polynomial_form<3, 0>(direction) * M0 +
         spherical_harmonics_polynomial_form<3, 1>(direction) * Mp1 +
         spherical_harmonics_polynomial_form<3, 2>(direction) * Mp2 +
         spherical_harmonics_polynomial_form<3, 3>(direction) * Mp3;
}

/* Evaluate bands L1, L2, and L3. */
ccl_device_inline float3 spherical_harmonics_evaluate_rest(
    const ccl_global PackedSphericalHarmonics &packed_spherical_harmonics, const float3 direction)
{
  float3 result;

  result = spherical_harmonics_evaluate_band_L1(
      direction,
      spherical_harmonics_get<1, -1>(packed_spherical_harmonics),
      spherical_harmonics_get<1, 0>(packed_spherical_harmonics),
      spherical_harmonics_get<1, 1>(packed_spherical_harmonics));

  result += spherical_harmonics_evaluate_band_L2(
      direction,
      spherical_harmonics_get<2, -2>(packed_spherical_harmonics),
      spherical_harmonics_get<2, -1>(packed_spherical_harmonics),
      spherical_harmonics_get<2, 0>(packed_spherical_harmonics),
      spherical_harmonics_get<2, 1>(packed_spherical_harmonics),
      spherical_harmonics_get<2, 2>(packed_spherical_harmonics));

  result += spherical_harmonics_evaluate_band_L3(
      direction,
      spherical_harmonics_get<3, -3>(packed_spherical_harmonics),
      spherical_harmonics_get<3, -2>(packed_spherical_harmonics),
      spherical_harmonics_get<3, -1>(packed_spherical_harmonics),
      spherical_harmonics_get<3, 0>(packed_spherical_harmonics),
      spherical_harmonics_get<3, 1>(packed_spherical_harmonics),
      spherical_harmonics_get<3, 2>(packed_spherical_harmonics),
      spherical_harmonics_get<3, 3>(packed_spherical_harmonics));

  return result;
}

CCL_NAMESPACE_END
