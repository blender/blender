/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

float fresnel_dielectric_cos(float cosi, float eta)
{
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  float c = abs(cosi);
  float g = eta * eta - 1.0f + c * c;
  float result;

  if (g > 0.0f) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1.0f) / (c * (g - c) + 1.0f);
    result = 0.5f * A * A * (1.0f + B * B);
  }
  else {
    result = 1.0f; /* TIR (no refracted component) */
  }

  return result;
}

float fresnel_dielectric(float3 Incoming, float3 Normal, float eta)
{
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  return fresnel_dielectric_cos(dot(Incoming, Normal), eta);
}

void node_fresnel(float ior, float3 N, out float result)
{
  N = normalize(N);
  float3 V = coordinate_incoming(g_data.P);

  float eta = max(ior, 0.00001f);
  result = fresnel_dielectric(V, N, (FrontFacing) ? eta : 1.0f / eta);
}
