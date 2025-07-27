/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void differentiate_texco(float3 v, out float3 df)
{
  /* Implementation defined. */
  df = v + dF_impl(v);
}

/* Overload for UVs which are loaded as generic attributes. */
void differentiate_texco(float4 v, out float3 df)
{
  /* Implementation defined. */
  df = v.xyz + dF_impl(v.xyz);
}

void node_bump(float strength,
               float dist,
               float filter_width,
               float height,
               float3 N,
               float2 height_xy,
               float invert,
               out float3 result)
{
  N = normalize(N);
  dist *= FrontFacing ? invert : -invert;

#ifdef GPU_FRAGMENT_SHADER
  float3 dPdx = gpu_dfdx(g_data.P) * derivative_scale_get();
  float3 dPdy = gpu_dfdy(g_data.P) * derivative_scale_get();

  /* Get surface tangents from normal. */
  float3 Rx = cross(dPdy, N);
  float3 Ry = cross(N, dPdx);

  /* Compute surface gradient and determinant. */
  float det = dot(dPdx, Rx);

  float2 dHd = height_xy - float2(height);
  float3 surfgrad = dHd.x * Rx + dHd.y * Ry;

  strength = max(strength, 0.0f);

  result = normalize(filter_width * abs(det) * N - dist * sign(det) * surfgrad);
  result = normalize(mix(N, result, strength));
#else
  result = N;
#endif
}
