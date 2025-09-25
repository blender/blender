/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "infos/eevee_common_infos.hh"

/**
 * Adapted from :
 * Real-Time Polygonal-Light Shading with Linearly Transformed Cosines.
 * Eric Heitz, Jonathan Dupuy, Stephen Hill and David Neubelt.
 * ACM Transactions on Graphics (Proceedings of ACM SIGGRAPH 2016) 35(4), 2016.
 * Project page: https://eheitzresearch.wordpress.com/415-2/
 */

#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* Diffuse *clipped* sphere integral. */
float ltc_diffuse_sphere_integral(sampler2DArray utility_tx, float avg_dir_z, float form_factor)
{
#if 1
  /* use tabulated horizon-clipped sphere */
  float2 uv = float2(avg_dir_z * 0.5f + 0.5f, form_factor);
  uv = uv * UTIL_TEX_UV_SCALE + UTIL_TEX_UV_BIAS;

  return texture(utility_tx, float3(uv, UTIL_DISK_INTEGRAL_LAYER))[UTIL_DISK_INTEGRAL_COMP];
#else
  /* Cheap approximation. Less smooth and have energy issues. */
  return max((form_factor * form_factor + avg_dir_z) / (form_factor + 1.0f), 0.0f);
#endif
}

/**
 * An extended version of the implementation from
 * "How to solve a cubic equation, revisited"
 * http://momentsingraphics.de/?p=105
 */
float3 ltc_solve_cubic(float4 coefs)
{
  /* Normalize the polynomial */
  coefs.xyz /= coefs.w;
  /* Divide middle coefficients by three */
  coefs.yz /= 3.0f;

  float A = coefs.w;
  float B = coefs.z;
  float C = coefs.y;
  float D = coefs.x;

  /* Compute the Hessian and the discriminant */
  float3 delta = float3(-coefs.zy * coefs.zz + coefs.yx, dot(float2(coefs.z, -coefs.y), coefs.xy));

  /* Discriminant */
  float discr = dot(float2(4.0f * delta.x, -delta.y), delta.zy);

  /* Clamping avoid NaN output on some platform. (see #67060) */
  float sqrt_discr = sqrt(clamp(discr, 0.0f, FLT_MAX));

  float2 xlc, xsc;

  /* Algorithm A */
  {
    float C_a = delta.x;
    float D_a = -2.0f * B * delta.x + delta.y;

    /* Take the cubic root of a normalized complex number */
    float theta = atan(sqrt_discr, -D_a) / 3.0f;

    float _2_sqrt_C_a = 2.0f * sqrt(-C_a);
    float x_1a = _2_sqrt_C_a * cos(theta);
    float x_3a = _2_sqrt_C_a * cos(theta + (2.0f / 3.0f) * M_PI);

    float xl;
    if ((x_1a + x_3a) > 2.0f * B) {
      xl = x_1a;
    }
    else {
      xl = x_3a;
    }

    xlc = float2(xl - B, A);
  }

  /* Algorithm D */
  {
    float C_d = delta.z;
    float D_d = -D * delta.y + 2.0f * C * delta.z;

    /* Take the cubic root of a normalized complex number */
    float theta = atan(D * sqrt_discr, -D_d) / 3.0f;

    float _2_sqrt_C_d = 2.0f * sqrt(-C_d);
    float x_1d = _2_sqrt_C_d * cos(theta);
    float x_3d = _2_sqrt_C_d * cos(theta + (2.0f / 3.0f) * M_PI);

    float xs;
    if (x_1d + x_3d < 2.0f * C) {
      xs = x_1d;
    }
    else {
      xs = x_3d;
    }

    xsc = float2(-D, xs + C);
  }

  float E = xlc.y * xsc.y;
  float F = -xlc.x * xsc.y - xlc.y * xsc.x;
  float G = xlc.x * xsc.x;

  float2 xmc = float2(C * F - B * G, -B * F + C * E);

  float3 root = float3(xsc.x / xsc.y, xmc.x / xmc.y, xlc.x / xlc.y);

  if (root.x < root.y && root.x < root.z) {
    root.xyz = root.yxz;
  }
  else if (root.z < root.x && root.z < root.y) {
    root.xyz = root.xzy;
  }

  return root;
}

/* from Real-Time Area Lighting: a Journey from Research to Production
 * Stephen Hill and Eric Heitz */
float3 ltc_edge_integral_vec(float3 v1, float3 v2)
{
  float x = dot(v1, v2);
  float y = abs(x);

  float a = 0.8543985f + (0.4965155f + 0.0145206f * y) * y;
  float b = 3.4175940f + (4.1616724f + y) * y;
  float v = a / b;

  float theta_sintheta = (x > 0.0f) ? v : 0.5f * inversesqrt(max(1.0f - x * x, 1e-7f)) - v;

  return cross(v1, v2) * theta_sintheta;
}

float3x3 ltc_matrix(float4 lut)
{
  /* Load inverse matrix. */
  return float3x3(float3(lut.x, 0, lut.y), float3(0, 1, 0), float3(lut.z, 0, lut.w));
}

float3x3 ltc_tangent_basis(float3 N, float3 V)
{
  float NV = dot(N, V);
  if (NV > 0.999999f) {
    /* Mostly for orthographic view and surfel light eval. */
    return from_up_axis(N);
  }
  /* Construct orthonormal basis around N. */
  float3 T1 = normalize(V - N * NV);
  float3 T2 = cross(N, T1);
  return float3x3(T1, T2, N);
}

void ltc_transform_quad(float3 N, float3 V, float3x3 Minv, inout float3 corners[4])
{
  /* Construct orthonormal basis around N. */
  float3x3 T = ltc_tangent_basis(N, V);

  /* Rotate area light in (T1, T2, R) basis. */
  Minv = Minv * transpose(T);

  /* Apply LTC inverse matrix. */
  corners[0] = normalize(Minv * corners[0]);
  corners[1] = normalize(Minv * corners[1]);
  corners[2] = normalize(Minv * corners[2]);
  corners[3] = normalize(Minv * corners[3]);
}

/* If corners have already pass through ltc_transform_quad(),
 * then N **MUST** be float3(0.0f, 0.0f, 1.0f), corresponding to the Up axis of the shading basis.
 */
float ltc_evaluate_quad(sampler2DArray utility_tx, float3 corners[4], float3 N)
{
  /* Approximation using a sphere of the same solid angle than the quad.
   * Finding the clipped sphere diffuse integral is easier than clipping the quad. */
  float3 avg_dir;
  avg_dir = ltc_edge_integral_vec(corners[0], corners[1]);
  avg_dir += ltc_edge_integral_vec(corners[1], corners[2]);
  avg_dir += ltc_edge_integral_vec(corners[2], corners[3]);
  avg_dir += ltc_edge_integral_vec(corners[3], corners[0]);

  float form_factor = length(avg_dir);
  float avg_dir_z = dot(N, avg_dir / form_factor);
  return form_factor * ltc_diffuse_sphere_integral(utility_tx, avg_dir_z, form_factor);
}

/* If disk does not need to be transformed and is already front facing. */
float ltc_evaluate_disk_simple(sampler2DArray utility_tx, float disk_radius, float NL)
{
  float r_sqr = disk_radius * disk_radius;
  float form_factor = r_sqr / (1.0f + r_sqr);
  return form_factor * ltc_diffuse_sphere_integral(utility_tx, NL, form_factor);
}

/* disk_points are WS vectors from the shading point to the disk "bounding domain" */
float ltc_evaluate_disk(
    sampler2DArray utility_tx, float3 N, float3 V, float3x3 Minv, float3 disk_points[3])
{
  /* Construct orthonormal basis around N. */
  float3x3 T = ltc_tangent_basis(N, V);

  /* Rotate area light in (T1, T2, R) basis. */
  float3x3 R = transpose(T);

  /* Intermediate step: init ellipse. */
  float3 L_[3];
  L_[0] = R * disk_points[0];
  L_[1] = R * disk_points[1];
  L_[2] = R * disk_points[2];

  float3 C = 0.5f * (L_[0] + L_[2]);
  float3 V1 = 0.5f * (L_[1] - L_[2]);
  float3 V2 = 0.5f * (L_[1] - L_[0]);

  /* Transform ellipse by Minv. */
  C = Minv * C;
  V1 = Minv * V1;
  V2 = Minv * V2;

  /* Compute eigenvectors of new ellipse. */

  float d11 = dot(V1, V1);
  float d22 = dot(V2, V2);
  float d12 = dot(V1, V2);
  float a, inv_b;                      /* Eigenvalues */
  constexpr float threshold = 0.0007f; /* Can be adjusted. Fix artifacts. */
  if (abs(d12) / sqrt(d11 * d22) > threshold) {
    float tr = d11 + d22;
    float det = -d12 * d12 + d11 * d22;

    /* use sqrt matrix to solve for eigenvalues */
    det = sqrt(det);
    float u = 0.5f * sqrt(tr - 2.0f * det);
    float v = 0.5f * sqrt(tr + 2.0f * det);
    float e_max = (u + v);
    float e_min = (u - v);
    e_max *= e_max;
    e_min *= e_min;

    float3 V1_, V2_;
    if (d11 > d22) {
      V1_ = d12 * V1 + (e_max - d11) * V2;
      V2_ = d12 * V1 + (e_min - d11) * V2;
    }
    else {
      V1_ = d12 * V2 + (e_max - d22) * V1;
      V2_ = d12 * V2 + (e_min - d22) * V1;
    }

    a = 1.0f / e_max;
    inv_b = e_min;
    V1 = normalize(V1_);
    V2 = normalize(V2_);
  }
  else {
    a = 1.0f / d11;
    inv_b = d22;
    V1 *= sqrt(a);
    V2 *= inversesqrt(inv_b);
  }

  /* Now find front facing ellipse with same solid angle. */

  float3 V3 = normalize(cross(V1, V2));
  if (dot(C, V3) < 0.0f) {
    V3 *= -1.0f;
  }

  float L = dot(V3, C);
  float inv_L = 1.0f / L;
  float x0 = dot(V1, C) * inv_L;
  float y0 = dot(V2, C) * inv_L;

  float ab = a * inv_b;
  inv_b *= square(inv_L);
  float t = 1.0f + x0 * x0;

  /* Compared to the original LTC implementation, we scale the polynomial by `b` to avoid numerical
   * issues when light size is small.
   * i.e., instead of solving `c0 * e^3 + c1 * e^2 + c2 * e + c3 = 0`,
   * we solve `c0/b^3 * (be)^3 + c1/b^2 * (be)^2 + c2/b * be + c3 = 0`. */
  float c0 = ab * inv_b;
  float c1 = ab * (t + y0 * y0) - c0 - inv_b;
  float c2 = inv_b - ab * t - (1.0f + y0 * y0);
  float c3 = 1.0f;

  float3 roots = ltc_solve_cubic(float4(c0, c1, c2, c3));
  float e1 = roots.x;
  float e2 = roots.y;
  float e3 = roots.z;

  /* Scale the root back by multiplying `b`.
   * `a * x0 / (a - b * e2)` simplifies to `a/b * x0 / (a/b - e2)`,
   * `b * y0 / (b - b * e2)` simplifies to `y0 / (1.0f - e2)`. */
  float3 avg_dir = float3(ab * x0 / (ab - e2), y0 / (1.0f - e2), 1.0f);

  float3x3 rotate = float3x3(V1, V2, V3);

  avg_dir = rotate * avg_dir;
  avg_dir = normalize(avg_dir);

  /* L1, L2 are the extends of the front facing ellipse. */
  float L1 = sqrt(-e2 / e3);
  float L2 = sqrt(-e2 / e1);

  /* Find the sphere and compute lighting. */
  float form_factor = max(0.0f, L1 * L2 * inversesqrt((1.0f + L1 * L1) * (1.0f + L2 * L2)));
  return form_factor * ltc_diffuse_sphere_integral(utility_tx, avg_dir.z, form_factor);
}
