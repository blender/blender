/*
 * Copyright 2011-2020 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* Area light sampling */

/* Uses the following paper:
 *
 * Carlos Urena et al.
 * An Area-Preserving Parametrization for Spherical Rectangles.
 *
 * https://www.solidangle.com/research/egsr2013_spherical_rectangle.pdf
 *
 * Note: light_p is modified when sample_coord is true.
 */
ccl_device_inline float rect_light_sample(float3 P,
                                          float3 *light_p,
                                          float3 axisu,
                                          float3 axisv,
                                          float randu,
                                          float randv,
                                          bool sample_coord)
{
  /* In our name system we're using P for the center,
   * which is o in the paper.
   */

  float3 corner = *light_p - axisu * 0.5f - axisv * 0.5f;
  float axisu_len, axisv_len;
  /* Compute local reference system R. */
  float3 x = normalize_len(axisu, &axisu_len);
  float3 y = normalize_len(axisv, &axisv_len);
  float3 z = cross(x, y);
  /* Compute rectangle coords in local reference system. */
  float3 dir = corner - P;
  float z0 = dot(dir, z);
  /* Flip 'z' to make it point against Q. */
  if (z0 > 0.0f) {
    z *= -1.0f;
    z0 *= -1.0f;
  }
  float x0 = dot(dir, x);
  float y0 = dot(dir, y);
  float x1 = x0 + axisu_len;
  float y1 = y0 + axisv_len;
  /* Compute internal angles (gamma_i). */
  float4 diff = make_float4(x0, y1, x1, y0) - make_float4(x1, y0, x0, y1);
  float4 nz = make_float4(y0, x1, y1, x0) * diff;
  nz = nz / sqrt(z0 * z0 * diff * diff + nz * nz);
  float g0 = safe_acosf(-nz.x * nz.y);
  float g1 = safe_acosf(-nz.y * nz.z);
  float g2 = safe_acosf(-nz.z * nz.w);
  float g3 = safe_acosf(-nz.w * nz.x);
  /* Compute predefined constants. */
  float b0 = nz.x;
  float b1 = nz.z;
  float b0sq = b0 * b0;
  float k = M_2PI_F - g2 - g3;
  /* Compute solid angle from internal angles. */
  float S = g0 + g1 - k;

  if (sample_coord) {
    /* Compute cu. */
    float au = randu * S + k;
    float fu = (cosf(au) * b0 - b1) / sinf(au);
    float cu = 1.0f / sqrtf(fu * fu + b0sq) * (fu > 0.0f ? 1.0f : -1.0f);
    cu = clamp(cu, -1.0f, 1.0f);
    /* Compute xu. */
    float xu = -(cu * z0) / max(sqrtf(1.0f - cu * cu), 1e-7f);
    xu = clamp(xu, x0, x1);
    /* Compute yv. */
    float z0sq = z0 * z0;
    float y0sq = y0 * y0;
    float y1sq = y1 * y1;
    float d = sqrtf(xu * xu + z0sq);
    float h0 = y0 / sqrtf(d * d + y0sq);
    float h1 = y1 / sqrtf(d * d + y1sq);
    float hv = h0 + randv * (h1 - h0), hv2 = hv * hv;
    float yv = (hv2 < 1.0f - 1e-6f) ? (hv * d) / sqrtf(1.0f - hv2) : y1;

    /* Transform (xu, yv, z0) to world coords. */
    *light_p = P + xu * x + yv * y + z0 * z;
  }

  /* return pdf */
  if (S != 0.0f)
    return 1.0f / S;
  else
    return 0.0f;
}

ccl_device_inline float3 ellipse_sample(float3 ru, float3 rv, float randu, float randv)
{
  to_unit_disk(&randu, &randv);
  return ru * randu + rv * randv;
}

ccl_device float3 disk_light_sample(float3 v, float randu, float randv)
{
  float3 ru, rv;

  make_orthonormals(v, &ru, &rv);

  return ellipse_sample(ru, rv, randu, randv);
}

ccl_device float3 distant_light_sample(float3 D, float radius, float randu, float randv)
{
  return normalize(D + disk_light_sample(D, randu, randv) * radius);
}

ccl_device float3
sphere_light_sample(float3 P, float3 center, float radius, float randu, float randv)
{
  return disk_light_sample(normalize(P - center), randu, randv) * radius;
}

ccl_device float spot_light_attenuation(float3 dir, float spot_angle, float spot_smooth, float3 N)
{
  float attenuation = dot(dir, N);

  if (attenuation <= spot_angle) {
    attenuation = 0.0f;
  }
  else {
    float t = attenuation - spot_angle;

    if (t < spot_smooth && spot_smooth != 0.0f)
      attenuation *= smoothstepf(t / spot_smooth);
  }

  return attenuation;
}

ccl_device float lamp_light_pdf(KernelGlobals *kg, const float3 Ng, const float3 I, float t)
{
  float cos_pi = dot(Ng, I);

  if (cos_pi <= 0.0f)
    return 0.0f;

  return t * t / cos_pi;
}

CCL_NAMESPACE_END
