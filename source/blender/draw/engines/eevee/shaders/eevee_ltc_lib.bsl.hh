/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * Adapted from :
 * Real-Time Polygonal-Light Shading with Linearly Transformed Cosines.
 * Eric Heitz, Jonathan Dupuy, Stephen Hill and David Neubelt.
 * ACM Transactions on Graphics (Proceedings of ACM SIGGRAPH 2016) 35(4), 2016.
 * Project page: https://eheitzresearch.wordpress.com/415-2/
 */

#pragma once

#include "eevee_bxdf_types.bsl.hh"
#include "eevee_defines.hh"
#include "eevee_light_lib.bsl.hh"
#include "eevee_ltc_lut_lib.bsl.hh"
#include "gpu_shader_compat.hh"
#include "gpu_shader_math_constants.bsl.hh"
#include "gpu_shader_math_matrix_construct.bsl.hh"
#include "gpu_shader_math_safe.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh" /* IWYU pragma: export. FLT_MAX */

namespace eevee::ltc {

namespace detail {

/**
 * Diffuse *clipped* sphere integral. This should equal the irradiance (form factor) of a
 * horizon-clipped sphere w.r.t. a single point. However, the form factor is divided out,
 * resulting in a multiplier defining the clipped sphere only.
 */
float diffuse_sphere_integral(sampler2DArray util_tx, float avg_dir_z, float form_factor)
{
#if 1
  /* use tabulated horizon-clipped sphere */
  float2 uv = float2(avg_dir_z * 0.5f + 0.5f, form_factor);
  uv = uv * UTIL_TEX_UV_SCALE + UTIL_TEX_UV_BIAS;

  return texture(util_tx, float3(uv, UTIL_DISK_INTEGRAL_LAYER))[UTIL_DISK_INTEGRAL_COMP];
#else
  /* Cheap approximation. Less smooth and have energy issues. */
  return max((form_factor * form_factor + avg_dir_z) / (form_factor + 1.0f), 0.0f);
#endif
}

/**
 * An extended version of the implementation from [How to solve a cubic equation,
 * revisited](http://momentsingraphics.de/?p=105).
 */
float3 solve_cubic(float4 coefs)
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

/**
 * Approximate edge integral as part of a polygon area integral over a cosine (hemi)sphere.

 * 1. This dates back to Lambert, see: [Geometric Derivation of the Irradiance of  Polygonal
 *    Lights](https://hal.science/hal-01458129/document).
 * 2. This is a fitting that avoids precision issues, see: [Real-Time Area Lighting: a Journey from
 *    Research to Production](https://advances.realtimerendering.com/s2016/s2016_ltc_rnd.pdf)
 */
float3 edge_integral_vec(float3 v1, float3 v2)
{
  float x = dot(v1, v2);
  float y = abs(x);

  float a = 0.8543985f + (0.4965155f + 0.0145206f * y) * y;
  float b = 3.4175940f + (4.1616724f + y) * y;
  float v = a / b;

  float theta_sintheta = (x > 0.0f) ? v : 0.5f * inversesqrt(max(1.0f - x * x, 1e-7f)) - v;

  return cross(v1, v2) * theta_sintheta;
}

/**
 * Upper parabolic curve of the form `x/(1+x)`, going through `y_1` at `x=1`,
 * with `scale` determining the curve shape.
 */
float upper_parabole(float x, float scale, float y_1)
{
  x = saturate(x / y_1);
  return saturate((x * scale + x) / (scale + x));
}

/**
 * Get the 3rd column of the inverse of 3x3 matrix M^{-1}, normalized. We can avoid
 * computing the reciprocal determinant as we need a unit vector.
 */
float3 dominant_bxdf_direction(float3x3 Minv)
{
  float3 adjoint_z = float3(+(Minv[1][0] * Minv[2][1] - Minv[2][0] * Minv[1][1]),
                            -(Minv[0][0] * Minv[2][1] - Minv[2][0] * Minv[0][1]),
                            +(Minv[0][0] * Minv[1][1] - Minv[1][0] * Minv[0][1]));
  return normalize(adjoint_z);
}

/**
 * Fitted function to attenuation light leakage caused by the sphere integral approximation.
 */
float form_factor_attenuation(LightShape shape, LTCData ltc_data, float3 L)
{
  /* First, find an approximate dominant BxDF direction D. Then, find a vector T on the light
   * plane, coplanar with D and L, i.e. T = cross(cross(L, D), shape.N). */
  float3 D = dominant_bxdf_direction(ltc_data.Minv);
  float3 T = normalize(D * dot(L, shape.N) - L * dot(D, shape.N));

  /* Find t where D intersects the line L + t*T. */
  float TD = dot(T, D);
  float t = safe_divide(dot(D, L) * TD - dot(T, L), 1.0f - square(TD));

  if (t > 0.0 && t < shape.disk_radius_projected) {
    /* If t lies within the disk radius on the positive side, we do not attenuate. */
    return 1.0;
  }
  /* Otherwise, restrict t to within on the disk radius. */
  t = min(abs(t), shape.disk_radius_projected);

  /* Compute L + t*T and warp it into LTC space. This disk point bounds the light shape in LTC
   * space, so we can use its z-component to safely fit an attenuation factor. */
  float attenuation = saturate(normalize(ltc_data.Minv * (L + t * T)).z);
  attenuation = upper_parabole(attenuation, 0.15f, 0.5f);
  attenuation += (1.0f - attenuation) * ltc_data.attenuation_factor;

  return attenuation;
}
}  // namespace detail

/**
 * Evaluate contribution of rectangle light.
 */
float evaluate_quad(sampler2DArray util_tx, LightShape shape, LightVector lv, LTCData ltc_data)
{
  /* Transform the quad corners into LTC space, and project on to sphere. */
  float3 V[4] = {normalize(ltc_data.Minv * shape.v[0]),
                 normalize(ltc_data.Minv * shape.v[1]),
                 normalize(ltc_data.Minv * shape.v[2]),
                 normalize(ltc_data.Minv * shape.v[3])};

  /* Approximation using a sphere with the same form factor as the unclipped quad.
   * Finding a clipped sphere's form factor is easier than clipping the quad. */
  float3 avg_dir;
  avg_dir = detail::edge_integral_vec(V[0], V[1]);
  avg_dir += detail::edge_integral_vec(V[1], V[2]);
  avg_dir += detail::edge_integral_vec(V[2], V[3]);
  avg_dir += detail::edge_integral_vec(V[3], V[0]);

  float form_factor_inv = inversesqrt(dot(avg_dir, avg_dir));
  float avg_dir_z = (avg_dir * form_factor_inv).z;
  float form_factor = saturate(1.0f / form_factor_inv);
  /* The form factor should always be finite. Check that the previous saturate works as filter. */
  // assert(!isnan(form_factor) && !isinf(form_factor));

  switch (ltc_data.form_factor_type) {
    case LTCFormFactorType::OneSidedCosineSphereClipped:
      /* The clipped sphere approximation causes light leakage for low roughness;
       * we attenuate with a fitted function that removes some energy below the horizon. */
      form_factor *= detail::form_factor_attenuation(shape, ltc_data, lv.L);
      form_factor *= detail::diffuse_sphere_integral(util_tx, avg_dir_z, form_factor);
      break;
    default: /* LTCFormFactorType::TwoSidedCosineSphere */
      form_factor *= M_1_PI;
      break;
  }

  return form_factor;
}

/**
 * Evaluate contribution of disk light.
 *
 * disk_points are WS vectors from the shading point to the disk "bounding domain".
 */
float evaluate_disk(sampler2DArray util_tx, LightShape shape, LightVector lv, LTCData ltc_data)
{
  /* Intermediate step: init ellipse. */
  float3 C = 0.5f * (shape.v[0] + shape.v[2]);
  float3 V1 = 0.5f * (shape.v[1] - shape.v[2]);
  float3 V2 = 0.5f * (shape.v[1] - shape.v[0]);

  /* Transform ellipse into LTC space. */
  C = ltc_data.Minv * C;
  V1 = ltc_data.Minv * V1;
  V2 = ltc_data.Minv * V2;

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

  /* Now find a front facing ellipse with the same solid angle. */

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

  float3 roots = detail::solve_cubic(float4(c0, c1, c2, c3));
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

  /* L1, L2 are the extents of the front facing ellipse. From here, find the sphere form factor. */
  float L1 = inversesqrt(-e3 / e2);
  float L2 = inversesqrt(-e1 / e2);
  float form_factor = saturate(L1 * L2 * inversesqrt((1.0f + L1 * L1) * (1.0f + L2 * L2)));
  /* The form factor should always be finite. Check that the previous saturate works as filter. */
  // assert(!isnan(form_factor) && !isinf(form_factor));

  switch (ltc_data.form_factor_type) {
    case LTCFormFactorType::OneSidedCosineSphereClipped:
      /* The clipped sphere approximation causes light leakage for low roughness;
       * we attenuate with a fitted function that removes some energy below the horizon. */
      form_factor *= detail::form_factor_attenuation(shape, ltc_data, lv.L);
      form_factor *= detail::diffuse_sphere_integral(util_tx, avg_dir.z, form_factor);
      break;
    default: /* LTCFormFactorType::TwoSidedCosineSphere */
      form_factor *= M_1_PI;
      break;
  }

  return form_factor;
}

/**
 * Perform LTC evaluation, returning the form factor of the light's shape to the shading point.
 * When multiplied by emission, this approximates the light's contributed irradiance.
 */
float evaluate(
    sampler2DArray util_tx, LightData light, LightShape shape, LightVector lv, LTCData ltc_data)
{
  if (is_sphere_light(light.type) && lv.dist < light.local().local.shape_radius) {
    /* Inside the sphere light, integrate over the hemisphere. */
    return 1.0f;
  }

  if (light.type == LIGHT_RECT) {
    return evaluate_quad(util_tx, shape, lv, ltc_data);
  }
  return evaluate_disk(util_tx, shape, lv, ltc_data);
}

}  // namespace eevee::ltc
