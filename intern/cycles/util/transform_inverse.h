/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#pragma once

CCL_NAMESPACE_BEGIN

/* Custom cross and dot implementations that match Embree bit for bit.
 * Normally we don't use SSE41/AVX outside the kernel, but for this it's
 * important to match exactly for ray tracing precision. */

ccl_device_forceinline float3 transform_inverse_cross(const float3 a, const float3 b)
{
#ifdef __AVX2__
  const ssef sse_a = (const __m128 &)a;
  const ssef sse_b = (const __m128 &)b;
  const ssef r = shuffle<1, 2, 0, 3>(
      ssef(_mm_fmsub_ps(sse_a, shuffle<1, 2, 0, 3>(sse_b), shuffle<1, 2, 0, 3>(sse_a) * sse_b)));
  return (const float3 &)r;
#endif

  return cross(a, b);
}

ccl_device_forceinline float transform_inverse_dot(const float3 a, const float3 b)
{
#ifdef __SSE4_1__
  return _mm_cvtss_f32(_mm_dp_ps((const __m128 &)a, (const __m128 &)b, 0x7F));
#endif

  return dot(a, b);
}

ccl_device_inline Transform transform_inverse_impl(const Transform tfm)
{
  /* This implementation matches the one in Embree exactly, to ensure consistent
   * results with the ray intersection of instances. */
  float3 x = make_float3(tfm.x.x, tfm.y.x, tfm.z.x);
  float3 y = make_float3(tfm.x.y, tfm.y.y, tfm.z.y);
  float3 z = make_float3(tfm.x.z, tfm.y.z, tfm.z.z);
  float3 w = make_float3(tfm.x.w, tfm.y.w, tfm.z.w);

  /* Compute determinant. */
  float det = transform_inverse_dot(x, transform_inverse_cross(y, z));

  if (det == 0.0f) {
    /* Matrix is degenerate (e.g. 0 scale on some axis), ideally we should
     * never be in this situation, but try to invert it anyway with tweak.
     *
     * This logic does not match Embree which would just give an invalid
     * matrix. A better solution would be to remove this and ensure any object
     * matrix is valid. */
    x.x += 1e-8f;
    y.y += 1e-8f;
    z.z += 1e-8f;

    det = transform_inverse_dot(x, cross(y, z));
    if (det == 0.0f) {
      det = FLT_MAX;
    }
  }

  /* Divide adjoint matrix by the determinant to compute inverse of 3x3 matrix. */
  const float3 inverse_x = transform_inverse_cross(y, z) / det;
  const float3 inverse_y = transform_inverse_cross(z, x) / det;
  const float3 inverse_z = transform_inverse_cross(x, y) / det;

  /* Compute translation and fill transform. */
  Transform itfm;
  itfm.x = float3_to_float4(inverse_x, -transform_inverse_dot(inverse_x, w));
  itfm.y = float3_to_float4(inverse_y, -transform_inverse_dot(inverse_y, w));
  itfm.z = float3_to_float4(inverse_z, -transform_inverse_dot(inverse_z, w));

  return itfm;
}
CCL_NAMESPACE_END
