/*
 * Copyright 2009-2020 Intel Corporation. Adapted from Embree with
 * with modifications.
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

/* Curve primitive intersection functions.
 *
 * The code here was adapted from curve_intersector_sweep.h in Embree, to get
 * an exact match betwee Embree CPU ray-tracing and our GPU ray-tracing. */

#define CURVE_NUM_BEZIER_SUBDIVISIONS 3
#define CURVE_NUM_BEZIER_SUBDIVISIONS_UNSTABLE (CURVE_NUM_BEZIER_SUBDIVISIONS + 1)
#define CURVE_NUM_BEZIER_STEPS 2
#define CURVE_NUM_JACOBIAN_ITERATIONS 5

#ifdef __HAIR__

/* Catmull-rom curve evaluation. */

ccl_device_inline float4 catmull_rom_basis_eval(const float4 curve[4], float u)
{
  const float t = u;
  const float s = 1.0f - u;
  const float n0 = -t * s * s;
  const float n1 = 2.0f + t * t * (3.0f * t - 5.0f);
  const float n2 = 2.0f + s * s * (3.0f * s - 5.0f);
  const float n3 = -s * t * t;
  return 0.5f * (curve[0] * n0 + curve[1] * n1 + curve[2] * n2 + curve[3] * n3);
}

ccl_device_inline float4 catmull_rom_basis_derivative(const float4 curve[4], float u)
{
  const float t = u;
  const float s = 1.0f - u;
  const float n0 = -s * s + 2.0f * s * t;
  const float n1 = 2.0f * t * (3.0f * t - 5.0f) + 3.0f * t * t;
  const float n2 = 2.0f * s * (3.0f * t + 2.0f) - 3.0f * s * s;
  const float n3 = -2.0f * s * t + t * t;
  return 0.5f * (curve[0] * n0 + curve[1] * n1 + curve[2] * n2 + curve[3] * n3);
}

ccl_device_inline float4 catmull_rom_basis_derivative2(const float4 curve[4], float u)
{

  const float t = u;
  const float n0 = -3.0f * t + 2.0f;
  const float n1 = 9.0f * t - 5.0f;
  const float n2 = -9.0f * t + 4.0f;
  const float n3 = 3.0f * t - 1.0f;
  return (curve[0] * n0 + curve[1] * n1 + curve[2] * n2 + curve[3] * n3);
}

/* Thick Curve */

ccl_device_inline float3 dnormalize(const float3 p, const float3 dp)
{
  const float pp = dot(p, p);
  const float pdp = dot(p, dp);
  return (pp * dp - pdp * p) / (pp * sqrtf(pp));
}

ccl_device_inline float sqr_point_to_line_distance(const float3 PmQ0, const float3 Q1mQ0)
{
  const float3 N = cross(PmQ0, Q1mQ0);
  const float3 D = Q1mQ0;
  return dot(N, N) / dot(D, D);
}

ccl_device_inline bool cylinder_intersect(const float3 cylinder_start,
                                          const float3 cylinder_end,
                                          const float cylinder_radius,
                                          const float3 ray_dir,
                                          float2 *t_o,
                                          float *u0_o,
                                          float3 *Ng0_o,
                                          float *u1_o,
                                          float3 *Ng1_o)
{
  /* Calculate quadratic equation to solve. */
  const float rl = 1.0f / len(cylinder_end - cylinder_start);
  const float3 P0 = cylinder_start, dP = (cylinder_end - cylinder_start) * rl;
  const float3 O = -P0, dO = ray_dir;

  const float dOdO = dot(dO, dO);
  const float OdO = dot(dO, O);
  const float OO = dot(O, O);
  const float dOz = dot(dP, dO);
  const float Oz = dot(dP, O);

  const float A = dOdO - sqr(dOz);
  const float B = 2.0f * (OdO - dOz * Oz);
  const float C = OO - sqr(Oz) - sqr(cylinder_radius);

  /* We miss the cylinder if determinant is smaller than zero. */
  const float D = B * B - 4.0f * A * C;
  if (!(D >= 0.0f)) {
    *t_o = make_float2(FLT_MAX, -FLT_MAX);
    return false;
  }

  /* Special case for rays that are parallel to the cylinder. */
  const float eps = 16.0f * FLT_EPSILON * max(fabsf(dOdO), fabsf(sqr(dOz)));
  if (fabsf(A) < eps) {
    if (C <= 0.0f) {
      *t_o = make_float2(-FLT_MAX, FLT_MAX);
      return true;
    }
    else {
      *t_o = make_float2(-FLT_MAX, FLT_MAX);
      return false;
    }
  }

  /* Standard case for rays that are not parallel to the cylinder. */
  const float Q = sqrtf(D);
  const float rcp_2A = 1.0f / (2.0f * A);
  const float t0 = (-B - Q) * rcp_2A;
  const float t1 = (-B + Q) * rcp_2A;

  /* Calculates u and Ng for near hit. */
  {
    *u0_o = (t0 * dOz + Oz) * rl;
    const float3 Pr = t0 * ray_dir;
    const float3 Pl = (*u0_o) * (cylinder_end - cylinder_start) + cylinder_start;
    *Ng0_o = Pr - Pl;
  }

  /* Calculates u and Ng for far hit. */
  {
    *u1_o = (t1 * dOz + Oz) * rl;
    const float3 Pr = t1 * ray_dir;
    const float3 Pl = (*u1_o) * (cylinder_end - cylinder_start) + cylinder_start;
    *Ng1_o = Pr - Pl;
  }

  *t_o = make_float2(t0, t1);

  return true;
}

ccl_device_inline float2 half_plane_intersect(const float3 P, const float3 N, const float3 ray_dir)
{
  const float3 O = -P;
  const float3 D = ray_dir;
  const float ON = dot(O, N);
  const float DN = dot(D, N);
  const float min_rcp_input = 1e-18f;
  const bool eps = fabsf(DN) < min_rcp_input;
  const float t = -ON / DN;
  const float lower = (eps || DN < 0.0f) ? -FLT_MAX : t;
  const float upper = (eps || DN > 0.0f) ? FLT_MAX : t;
  return make_float2(lower, upper);
}

ccl_device bool curve_intersect_iterative(const float3 ray_dir,
                                          const float dt,
                                          const float4 curve[4],
                                          float u,
                                          float t,
                                          const bool use_backfacing,
                                          Intersection *isect)
{
  const float length_ray_dir = len(ray_dir);

  /* Error of curve evaluations is propertional to largest coordinate. */
  const float4 box_min = min(min(curve[0], curve[1]), min(curve[2], curve[3]));
  const float4 box_max = max(min(curve[0], curve[1]), max(curve[2], curve[3]));
  const float4 box_abs = max(fabs(box_min), fabs(box_max));
  const float P_err = 16.0f * FLT_EPSILON *
                      max(box_abs.x, max(box_abs.y, max(box_abs.z, box_abs.w)));
  const float radius_max = box_max.w;

  for (int i = 0; i < CURVE_NUM_JACOBIAN_ITERATIONS; i++) {
    const float3 Q = ray_dir * t;
    const float3 dQdt = ray_dir;
    const float Q_err = 16.0f * FLT_EPSILON * length_ray_dir * t;

    const float4 P4 = catmull_rom_basis_eval(curve, u);
    const float4 dPdu4 = catmull_rom_basis_derivative(curve, u);

    const float3 P = float4_to_float3(P4);
    const float3 dPdu = float4_to_float3(dPdu4);
    const float radius = P4.w;
    const float dradiusdu = dPdu4.w;

    const float3 ddPdu = float4_to_float3(catmull_rom_basis_derivative2(curve, u));

    const float3 R = Q - P;
    const float len_R = len(R);
    const float R_err = max(Q_err, P_err);
    const float3 dRdu = -dPdu;
    const float3 dRdt = dQdt;

    const float3 T = normalize(dPdu);
    const float3 dTdu = dnormalize(dPdu, ddPdu);
    const float cos_err = P_err / len(dPdu);

    const float f = dot(R, T);
    const float f_err = len_R * P_err + R_err + cos_err * (1.0f + len_R);
    const float dfdu = dot(dRdu, T) + dot(R, dTdu);
    const float dfdt = dot(dRdt, T);

    const float K = dot(R, R) - sqr(f);
    const float dKdu = (dot(R, dRdu) - f * dfdu);
    const float dKdt = (dot(R, dRdt) - f * dfdt);
    const float rsqrt_K = inversesqrtf(K);

    const float g = sqrtf(K) - radius;
    const float g_err = R_err + f_err + 16.0f * FLT_EPSILON * radius_max;
    const float dgdu = dKdu * rsqrt_K - dradiusdu;
    const float dgdt = dKdt * rsqrt_K;

    const float invdet = 1.0f / (dfdu * dgdt - dgdu * dfdt);
    u -= (dgdt * f - dfdt * g) * invdet;
    t -= (-dgdu * f + dfdu * g) * invdet;

    if (fabsf(f) < f_err && fabsf(g) < g_err) {
      t += dt;
      if (!(0.0f <= t && t <= isect->t)) {
        return false; /* Rejects NaNs */
      }
      if (!(u >= 0.0f && u <= 1.0f)) {
        return false; /* Rejects NaNs */
      }

      /* Backface culling. */
      const float3 R = normalize(Q - P);
      const float3 U = dradiusdu * R + dPdu;
      const float3 V = cross(dPdu, R);
      const float3 Ng = cross(V, U);
      if (!use_backfacing && dot(ray_dir, Ng) > 0.0f) {
        return false;
      }

      /* Record intersection. */
      isect->t = t;
      isect->u = u;
      isect->v = 0.0f;

      return true;
    }
  }
  return false;
}

ccl_device bool curve_intersect_recursive(const float3 ray_orig,
                                          const float3 ray_dir,
                                          float4 curve[4],
                                          Intersection *isect)
{
  /* Move ray closer to make intersection stable. */
  const float3 center = float4_to_float3(0.25f * (curve[0] + curve[1] + curve[2] + curve[3]));
  const float dt = dot(center - ray_orig, ray_dir) / dot(ray_dir, ray_dir);
  const float3 ref = ray_orig + ray_dir * dt;
  const float4 ref4 = make_float4(ref.x, ref.y, ref.z, 0.0f);
  curve[0] -= ref4;
  curve[1] -= ref4;
  curve[2] -= ref4;
  curve[3] -= ref4;

  const bool use_backfacing = false;
  const float step_size = 1.0f / (float)(CURVE_NUM_BEZIER_STEPS);

  int depth = 0;

  /* todo: optimize stack for GPU somehow? Possibly some bitflags are enough, and
   * u0/u1 can be derived from the depth. */
  struct {
    float u0, u1;
    int i;
  } stack[CURVE_NUM_BEZIER_SUBDIVISIONS_UNSTABLE];

  bool found = false;

  float u0 = 0.0f;
  float u1 = 1.0f;
  int i = 0;

  while (1) {
    for (; i < CURVE_NUM_BEZIER_STEPS; i++) {
      const float step = i * step_size;

      /* Subdivide curve. */
      const float dscale = (u1 - u0) * (1.0f / 3.0f) * step_size;
      const float vu0 = mix(u0, u1, step);
      const float vu1 = mix(u0, u1, step + step_size);

      const float4 P0 = catmull_rom_basis_eval(curve, vu0);
      const float4 dP0du = dscale * catmull_rom_basis_derivative(curve, vu0);
      const float4 P3 = catmull_rom_basis_eval(curve, vu1);
      const float4 dP3du = dscale * catmull_rom_basis_derivative(curve, vu1);

      const float4 P1 = P0 + dP0du;
      const float4 P2 = P3 - dP3du;

      /* Calculate bounding cylinders. */
      const float rr1 = sqr_point_to_line_distance(float4_to_float3(dP0du),
                                                   float4_to_float3(P3 - P0));
      const float rr2 = sqr_point_to_line_distance(float4_to_float3(dP3du),
                                                   float4_to_float3(P3 - P0));
      const float maxr12 = sqrtf(max(rr1, rr2));
      const float one_plus_ulp = 1.0f + 2.0f * FLT_EPSILON;
      const float one_minus_ulp = 1.0f - 2.0f * FLT_EPSILON;
      float r_outer = max(max(P0.w, P1.w), max(P2.w, P3.w)) + maxr12;
      float r_inner = min(min(P0.w, P1.w), min(P2.w, P3.w)) - maxr12;
      r_outer = one_plus_ulp * r_outer;
      r_inner = max(0.0f, one_minus_ulp * r_inner);
      bool valid = true;

      /* Intersect with outer cylinder. */
      float2 tc_outer;
      float u_outer0, u_outer1;
      float3 Ng_outer0, Ng_outer1;
      valid = cylinder_intersect(float4_to_float3(P0),
                                 float4_to_float3(P3),
                                 r_outer,
                                 ray_dir,
                                 &tc_outer,
                                 &u_outer0,
                                 &Ng_outer0,
                                 &u_outer1,
                                 &Ng_outer1);
      if (!valid) {
        continue;
      }

      /* Intersect with cap-planes. */
      float2 tp = make_float2(-dt, isect->t - dt);
      tp = make_float2(max(tp.x, tc_outer.x), min(tp.y, tc_outer.y));
      const float2 h0 = half_plane_intersect(
          float4_to_float3(P0), float4_to_float3(dP0du), ray_dir);
      tp = make_float2(max(tp.x, h0.x), min(tp.y, h0.y));
      const float2 h1 = half_plane_intersect(
          float4_to_float3(P3), -float4_to_float3(dP3du), ray_dir);
      tp = make_float2(max(tp.x, h1.x), min(tp.y, h1.y));
      valid = tp.x <= tp.y;
      if (!valid) {
        continue;
      }

      /* Clamp and correct u parameter. */
      u_outer0 = clamp(u_outer0, 0.0f, 1.0f);
      u_outer1 = clamp(u_outer1, 0.0f, 1.0f);
      u_outer0 = mix(u0, u1, (step + u_outer0) * (1.0f / (float)(CURVE_NUM_BEZIER_STEPS + 1)));
      u_outer1 = mix(u0, u1, (step + u_outer1) * (1.0f / (float)(CURVE_NUM_BEZIER_STEPS + 1)));

      /* Intersect with inner cylinder. */
      float2 tc_inner;
      float u_inner0, u_inner1;
      float3 Ng_inner0, Ng_inner1;
      const bool valid_inner = cylinder_intersect(float4_to_float3(P0),
                                                  float4_to_float3(P3),
                                                  r_inner,
                                                  ray_dir,
                                                  &tc_inner,
                                                  &u_inner0,
                                                  &Ng_inner0,
                                                  &u_inner1,
                                                  &Ng_inner1);

      /* At the unstable area we subdivide deeper. */
#  if 0
      const bool unstable0 = (!valid_inner) |
                             (fabsf(dot(normalize(ray_dir), normalize(Ng_inner0))) < 0.3f);
      const bool unstable1 = (!valid_inner) |
                             (fabsf(dot(normalize(ray_dir), normalize(Ng_inner1))) < 0.3f);
#  else
      /* On the GPU appears to be a little faster if always enabled. */
      (void)valid_inner;

      const bool unstable0 = true;
      const bool unstable1 = true;
#  endif

      /* Subtract the inner interval from the current hit interval. */
      float2 tp0 = make_float2(tp.x, min(tp.y, tc_inner.x));
      float2 tp1 = make_float2(max(tp.x, tc_inner.y), tp.y);
      bool valid0 = valid && (tp0.x <= tp0.y);
      bool valid1 = valid && (tp1.x <= tp1.y);
      if (!(valid0 || valid1)) {
        continue;
      }

      /* Process one or two hits. */
      bool recurse = false;
      if (valid0) {
        const int termDepth = unstable0 ? CURVE_NUM_BEZIER_SUBDIVISIONS_UNSTABLE :
                                          CURVE_NUM_BEZIER_SUBDIVISIONS;
        if (depth >= termDepth) {
          found |= curve_intersect_iterative(
              ray_dir, dt, curve, u_outer0, tp0.x, use_backfacing, isect);
        }
        else {
          recurse = true;
        }
      }

      if (valid1 && (tp1.x + dt <= isect->t)) {
        const int termDepth = unstable1 ? CURVE_NUM_BEZIER_SUBDIVISIONS_UNSTABLE :
                                          CURVE_NUM_BEZIER_SUBDIVISIONS;
        if (depth >= termDepth) {
          found |= curve_intersect_iterative(
              ray_dir, dt, curve, u_outer1, tp1.y, use_backfacing, isect);
        }
        else {
          recurse = true;
        }
      }

      if (recurse) {
        stack[depth].u0 = u0;
        stack[depth].u1 = u1;
        stack[depth].i = i + 1;
        depth++;

        u0 = vu0;
        u1 = vu1;
        i = -1;
      }
    }

    if (depth > 0) {
      depth--;
      u0 = stack[depth].u0;
      u1 = stack[depth].u1;
      i = stack[depth].i;
    }
    else {
      break;
    }
  }

  return found;
}

/* Ribbons */

ccl_device_inline bool cylinder_culling_test(const float2 p1, const float2 p2, const float r)
{
  /* Performs culling against a cylinder. */
  const float2 dp = p2 - p1;
  const float num = dp.x * p1.y - dp.y * p1.x;
  const float den2 = dot(p2 - p1, p2 - p1);
  return num * num <= r * r * den2;
}

/*! Intersects a ray with a quad with backface culling
 *  enabled. The quad v0,v1,v2,v3 is split into two triangles
 *  v0,v1,v3 and v2,v3,v1. The edge v1,v2 decides which of the two
 *  triangles gets intersected. */
ccl_device_inline bool ribbon_intersect_quad(const float ray_tfar,
                                             const float3 quad_v0,
                                             const float3 quad_v1,
                                             const float3 quad_v2,
                                             const float3 quad_v3,
                                             float *u_o,
                                             float *v_o,
                                             float *t_o)
{
  /* Calculate vertices relative to ray origin? */
  const float3 O = make_float3(0.0f, 0.0f, 0.0f);
  const float3 D = make_float3(0.0f, 0.0f, 1.0f);
  const float3 va = quad_v0 - O;
  const float3 vb = quad_v1 - O;
  const float3 vc = quad_v2 - O;
  const float3 vd = quad_v3 - O;

  const float3 edb = vb - vd;
  const float WW = dot(cross(vd, edb), D);
  const float3 v0 = (WW <= 0.0f) ? va : vc;
  const float3 v1 = (WW <= 0.0f) ? vb : vd;
  const float3 v2 = (WW <= 0.0f) ? vd : vb;

  /* Calculate edges? */
  const float3 e0 = v2 - v0;
  const float3 e1 = v0 - v1;

  /* perform edge tests */
  const float U = dot(cross(v0, e0), D);
  const float V = dot(cross(v1, e1), D);
  if (!(max(U, V) <= 0.0f)) {
    return false;
  }

  /* Calculate geometry normal and denominator? */
  const float3 Ng = cross(e1, e0);
  const float den = dot(Ng, D);
  const float rcpDen = 1.0f / den;

  /* Perform depth test? */
  const float t = rcpDen * dot(v0, Ng);
  if (!(0.0f <= t && t <= ray_tfar)) {
    return false;
  }

  /* Avoid division by 0? */
  if (!(den != 0.0f)) {
    return false;
  }

  /* Update hit information? */
  *t_o = t;
  *u_o = U * rcpDen;
  *v_o = V * rcpDen;
  *u_o = (WW <= 0.0f) ? *u_o : 1.0f - *u_o;
  *v_o = (WW <= 0.0f) ? *v_o : 1.0f - *v_o;
  return true;
}

ccl_device_inline void ribbon_ray_space(const float3 ray_dir, float3 ray_space[3])
{
  const float3 dx0 = make_float3(0, ray_dir.z, -ray_dir.y);
  const float3 dx1 = make_float3(-ray_dir.z, 0, ray_dir.x);
  ray_space[0] = normalize(dot(dx0, dx0) > dot(dx1, dx1) ? dx0 : dx1);
  ray_space[1] = normalize(cross(ray_dir, ray_space[0]));
  ray_space[2] = ray_dir;
}

ccl_device_inline float4 ribbon_to_ray_space(const float3 ray_space[3],
                                             const float3 ray_org,
                                             const float4 P4)
{
  float3 P = float4_to_float3(P4) - ray_org;
  return make_float4(dot(ray_space[0], P), dot(ray_space[1], P), dot(ray_space[2], P), P4.w);
}

ccl_device_inline bool ribbon_intersect(const float3 ray_org,
                                        const float3 ray_dir,
                                        const float ray_tfar,
                                        const int N,
                                        float4 curve[4],
                                        Intersection *isect)
{
  /* Transform control points into ray space. */
  float3 ray_space[3];
  ribbon_ray_space(ray_dir, ray_space);

  curve[0] = ribbon_to_ray_space(ray_space, ray_org, curve[0]);
  curve[1] = ribbon_to_ray_space(ray_space, ray_org, curve[1]);
  curve[2] = ribbon_to_ray_space(ray_space, ray_org, curve[2]);
  curve[3] = ribbon_to_ray_space(ray_space, ray_org, curve[3]);

  const float4 mx = max(max(fabs(curve[0]), fabs(curve[1])), max(fabs(curve[2]), fabs(curve[3])));
  const float eps = 4.0f * FLT_EPSILON * max(max(mx.x, mx.y), max(mx.z, mx.w));
  const float step_size = 1.0f / (float)N;

  /* Evaluate first point and radius scaled normal direction. */
  float4 p0 = catmull_rom_basis_eval(curve, 0.0f);
  float3 dp0dt = float4_to_float3(catmull_rom_basis_derivative(curve, 0.0f));
  if (max3(fabs(dp0dt)) < eps) {
    const float4 p1 = catmull_rom_basis_eval(curve, step_size);
    dp0dt = float4_to_float3(p1 - p0);
  }
  float3 wn0 = normalize(make_float3(dp0dt.y, -dp0dt.x, 0.0f)) * p0.w;

  /* Evaluate the bezier curve. */
  for (int i = 0; i < N; i++) {
    const float u = i * step_size;
    const float4 p1 = catmull_rom_basis_eval(curve, u + step_size);
    bool valid = cylinder_culling_test(
        make_float2(p0.x, p0.y), make_float2(p1.x, p1.y), max(p0.w, p1.w));
    if (!valid) {
      continue;
    }

    /* Evaluate next point. */
    float3 dp1dt = float4_to_float3(catmull_rom_basis_derivative(curve, u + step_size));
    dp1dt = (max3(fabs(dp1dt)) < eps) ? float4_to_float3(p1 - p0) : dp1dt;
    const float3 wn1 = normalize(make_float3(dp1dt.y, -dp1dt.x, 0.0f)) * p1.w;

    /* Construct quad coordinates. */
    const float3 lp0 = float4_to_float3(p0) + wn0;
    const float3 lp1 = float4_to_float3(p1) + wn1;
    const float3 up0 = float4_to_float3(p0) - wn0;
    const float3 up1 = float4_to_float3(p1) - wn1;

    /* Intersect quad. */
    float vu, vv, vt;
    bool valid0 = ribbon_intersect_quad(isect->t, lp0, lp1, up1, up0, &vu, &vv, &vt);

    if (valid0) {
      /* ignore self intersections */
      const float avoidance_factor = 2.0f;
      if (avoidance_factor != 0.0f) {
        float r = mix(p0.w, p1.w, vu);
        valid0 = vt > avoidance_factor * r;
      }

      if (valid0) {
        vv = 2.0f * vv - 1.0f;

        /* Record intersection. */
        isect->t = vt;
        isect->u = u + vu * step_size;
        isect->v = vv;
        return true;
      }
    }

    p0 = p1;
    wn0 = wn1;
  }
  return false;
}

ccl_device_forceinline bool curve_intersect(KernelGlobals *kg,
                                            Intersection *isect,
                                            const float3 P,
                                            const float3 dir,
                                            uint visibility,
                                            int object,
                                            int curveAddr,
                                            float time,
                                            int type)
{
  const bool is_curve_primitive = (type & PRIMITIVE_CURVE);

#  ifndef __KERNEL_OPTIX__ /* See OptiX motion flag OPTIX_MOTION_FLAG_[START|END]_VANISH */
  if (!is_curve_primitive && kernel_data.bvh.use_bvh_steps) {
    const float2 prim_time = kernel_tex_fetch(__prim_time, curveAddr);
    if (time < prim_time.x || time > prim_time.y) {
      return false;
    }
  }
#  endif

  int segment = PRIMITIVE_UNPACK_SEGMENT(type);
  int prim = kernel_tex_fetch(__prim_index, curveAddr);

  float4 v00 = kernel_tex_fetch(__curves, prim);

  int k0 = __float_as_int(v00.x) + segment;
  int k1 = k0 + 1;

  int ka = max(k0 - 1, __float_as_int(v00.x));
  int kb = min(k1 + 1, __float_as_int(v00.x) + __float_as_int(v00.y) - 1);

  float4 curve[4];
  if (is_curve_primitive) {
    curve[0] = kernel_tex_fetch(__curve_keys, ka);
    curve[1] = kernel_tex_fetch(__curve_keys, k0);
    curve[2] = kernel_tex_fetch(__curve_keys, k1);
    curve[3] = kernel_tex_fetch(__curve_keys, kb);
  }
  else {
    int fobject = (object == OBJECT_NONE) ? kernel_tex_fetch(__prim_object, curveAddr) : object;
    motion_curve_keys(kg, fobject, prim, time, ka, k0, k1, kb, curve);
  }

#  ifdef __VISIBILITY_FLAG__
  if (!(kernel_tex_fetch(__prim_visibility, curveAddr) & visibility)) {
    return false;
  }
#  endif

  const bool use_ribbon = (kernel_data.curve.curveflags & CURVE_KN_RIBBONS) != 0;
  if (use_ribbon) {
    /* todo: adaptive number of subdivisions could help performance here. */
    const int subdivisions = kernel_data.curve.subdivisions;
    if (ribbon_intersect(P, dir, isect->t, subdivisions, curve, isect)) {
      isect->prim = curveAddr;
      isect->object = object;
      isect->type = type;
      return true;
    }

    return false;
  }
  else {
    if (curve_intersect_recursive(P, dir, curve, isect)) {
      isect->prim = curveAddr;
      isect->object = object;
      isect->type = type;
      return true;
    }

    return false;
  }
}

ccl_device_inline void curve_shader_setup(KernelGlobals *kg,
                                          ShaderData *sd,
                                          const Intersection *isect,
                                          const Ray *ray)
{
  float t = isect->t;
  float3 P = ray->P;
  float3 D = ray->D;

  if (isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_itfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_INVERSE_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
    D = transform_direction(&tfm, D * t);
    D = normalize_len(D, &t);
  }

  int prim = kernel_tex_fetch(__prim_index, isect->prim);
  float4 v00 = kernel_tex_fetch(__curves, prim);

  int k0 = __float_as_int(v00.x) + PRIMITIVE_UNPACK_SEGMENT(sd->type);
  int k1 = k0 + 1;

  int ka = max(k0 - 1, __float_as_int(v00.x));
  int kb = min(k1 + 1, __float_as_int(v00.x) + __float_as_int(v00.y) - 1);

  float4 P_curve[4];

  if (sd->type & PRIMITIVE_CURVE) {
    P_curve[0] = kernel_tex_fetch(__curve_keys, ka);
    P_curve[1] = kernel_tex_fetch(__curve_keys, k0);
    P_curve[2] = kernel_tex_fetch(__curve_keys, k1);
    P_curve[3] = kernel_tex_fetch(__curve_keys, kb);
  }
  else {
    motion_curve_keys(kg, sd->object, sd->prim, sd->time, ka, k0, k1, kb, P_curve);
  }

  sd->u = isect->u;
  sd->v = isect->v;

  P = P + D * t;

  const float4 dPdu4 = catmull_rom_basis_derivative(P_curve, isect->u);
  const float3 dPdu = float4_to_float3(dPdu4);

  if (kernel_data.curve.curveflags & CURVE_KN_RIBBONS) {
    /* Rounded smooth normals for ribbons, to approximate thick curve shape. */
    const float3 tangent = normalize(dPdu);
    const float3 bitangent = normalize(cross(tangent, -D));
    const float sine = isect->v;
    const float cosine = safe_sqrtf(1.0f - sine * sine);

    sd->N = normalize(sine * bitangent - cosine * normalize(cross(tangent, bitangent)));
    sd->Ng = -D;

#  if 0
    /* This approximates the position and geometric normal of a thick curve too,
     * but gives too many issues with wrong self intersections. */
    const float dPdu_radius = dPdu4.w;
    sd->Ng = sd->N;
    P += sd->N * dPdu_radius;
#  endif
  }
  else {
    /* Thick curves, compute normal using direction from inside the curve.
     * This could be optimized by recording the normal in the intersection,
     * however for Optix this would go beyond the size of the payload. */
    const float3 P_inside = float4_to_float3(catmull_rom_basis_eval(P_curve, isect->u));
    sd->Ng = normalize(P - P_inside);
    sd->N = sd->Ng;
  }

#  ifdef __DPDU__
  /* dPdu/dPdv */
  sd->dPdu = dPdu;
  sd->dPdv = cross(dPdu, sd->Ng);
#  endif

  if (isect->object != OBJECT_NONE) {
#  ifdef __OBJECT_MOTION__
    Transform tfm = sd->ob_tfm;
#  else
    Transform tfm = object_fetch_transform(kg, isect->object, OBJECT_TRANSFORM);
#  endif

    P = transform_point(&tfm, P);
  }

  sd->P = P;

  float4 curvedata = kernel_tex_fetch(__curves, sd->prim);
  sd->shader = __float_as_int(curvedata.z);
}

#endif

CCL_NAMESPACE_END
