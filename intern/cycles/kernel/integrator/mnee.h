/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/geom/motion_triangle.h"
#include "kernel/geom/shader_data.h"
#include "kernel/geom/triangle.h"

#include "kernel/light/sample.h"

/*
 * Manifold Next Event Estimation
 *
 * This code adds manifold next event estimation through refractive surface(s) as a new sampling
 * technique for direct lighting, i.e. finding the point on the refractive surface(s) along the
 * path to a light sample, which satisfies fermat's principle for a given microfacet normal and
 * the path's end points. This technique involves walking on the "specular manifold" using a pseudo
 * newton solver. Such a manifold is defined by the specular constraint matrix from the manifold
 * exploration framework [2]. For each refractive interface, this constraint is defined by
 * enforcing that the generalized half-vector projection onto the interface local tangent plane is
 * null. The newton solver guides the walk by linearizing the manifold locally before reprojecting
 * the linear solution onto the refractive surface. See paper [1] for more details about
 * the technique itself and [3] for the half-vector light transport formulation, from which it is
 * derived.
 *
 * [1] Manifold Next Event Estimation
 * Johannes Hanika, Marc Droske, and Luca Fascione. 2015.
 * Comput. Graph. Forum 34, 4 (July 2015), 87–97.
 * https://jo.dreggn.org/home/2015_mnee.pdf
 *
 * [2] Manifold exploration: a Markov Chain Monte Carlo technique for rendering scenes with
 * difficult specular transport Wenzel Jakob and Steve Marschner. 2012. ACM Trans. Graph. 31, 4,
 * Article 58 (July 2012), 13 pages.
 * https://www.cs.cornell.edu/projects/manifolds-sg12/
 *
 * [3] The Natural-Constraint Representation of the Path Space for Efficient Light Transport
 * Simulation Anton S. Kaplanyan, Johannes Hanika, and Carsten Dachsbacher. 2014. ACM Trans. Graph.
 * 33, 4, Article 102 (July 2014), 13 pages.
 *  https://cg.ivd.kit.edu/english/HSLT.php
 */

// NOLINTBEGIN
#define MNEE_MAX_ITERATIONS 64
#define MNEE_MAX_INTERSECTION_COUNT 10
#define MNEE_SOLVER_THRESHOLD 0.001f
#define MNEE_MINIMUM_STEP_SIZE 0.0001f
#define MNEE_MAX_CAUSTIC_CASTERS 6
#define MNEE_MIN_DISTANCE 0.001f
#define MNEE_MIN_PROGRESS_DISTANCE 0.0001f
#define MNEE_MIN_DETERMINANT 0.0001f
#define MNEE_PROJECTION_DISTANCE_MULTIPLIER 2.f
// NOLINTEND

CCL_NAMESPACE_BEGIN

/* Manifold struct containing the local differential geometry quantity */
struct ManifoldVertex {
  /* Position and partials */
  float3 p;
  float3 dp_du;
  float3 dp_dv;

  /* Normal and partials */
  float3 n;
  float3 ng;
  float3 dn_du;
  float3 dn_dv;

  /* geometric info */
  float2 uv;
  int object;
  int prim;
  int shader;

  /* closure info */
  float eta;
  ccl_private ShaderClosure *bsdf;
  float2 n_offset;

  /* constraint and its derivative matrices */
  float2 constraint;
  float4 a;
  float4 b;
  float4 c;
};

/* Multiplication of a 2x2 matrix encoded in a row-major order float4 by a vector */
ccl_device_inline float2 mat22_mult(const float4 a, const float2 b)
{
  return make_float2(a.x * b.x + a.y * b.y, a.z * b.x + a.w * b.y);
}

/* Multiplication of 2x2 matrices encoded in a row-major order float4 */
ccl_device_inline float4 mat22_mult(const float4 a, const float4 b)
{
  return make_float4(
      a.x * b.x + a.y * b.z, a.x * b.y + a.y * b.w, a.z * b.x + a.w * b.z, a.z * b.y + a.w * b.w);
}

/* Determinant of a 2x2 matrix encoded in a row-major order float4 */
ccl_device_inline float mat22_determinant(const float4 m)
{
  return m.x * m.w - m.y * m.z;
}

/* Inverse of a 2x2 matrix encoded in a row-major order float4 */
ccl_device_inline float mat22_inverse(const float4 m, ccl_private float4 &m_inverse)
{
  const float det = mat22_determinant(m);
  if (fabsf(det) < MNEE_MIN_DETERMINANT) {
    return 0.f;
  }
  m_inverse = make_float4(m.w, -m.y, -m.z, m.x) / det;
  return det;
}

/* Manifold vertex setup from ray and intersection data */
ccl_device_forceinline void mnee_setup_manifold_vertex(KernelGlobals kg,
                                                       ccl_private ManifoldVertex *vtx,
                                                       ccl_private ShaderClosure *bsdf,
                                                       const float eta,
                                                       const float2 n_offset,
                                                       const ccl_private Ray *ray,
                                                       const ccl_private Intersection *isect,
                                                       ccl_private ShaderData *sd_vtx)
{
  sd_vtx->object = (isect->object == OBJECT_NONE) ? kernel_data_fetch(prim_object, isect->prim) :
                                                    isect->object;

  sd_vtx->type = isect->type;
  sd_vtx->flag = 0;
  sd_vtx->object_flag = kernel_data_fetch(object_flag, sd_vtx->object);

  /* Matrices and time. */
  shader_setup_object_transforms(kg, sd_vtx, ray->time);
  sd_vtx->time = ray->time;

  sd_vtx->prim = isect->prim;
  sd_vtx->ray_length = isect->t;

  sd_vtx->u = isect->u;
  sd_vtx->v = isect->v;

  sd_vtx->shader = kernel_data_fetch(tri_shader, sd_vtx->prim);

  float3 verts[3];
  float3 normals[3];
  if (sd_vtx->type & PRIMITIVE_TRIANGLE) {
    /* Load triangle vertices and normals. */
    triangle_vertices_and_normals(kg, sd_vtx->prim, verts, normals);

    /* Compute refined position (same code as in triangle_point_from_uv). */
    sd_vtx->P = (1.f - isect->u - isect->v) * verts[0] + isect->u * verts[1] + isect->v * verts[2];
    if (!(sd_vtx->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      const Transform tfm = object_get_transform(kg, sd_vtx);
      sd_vtx->P = transform_point(&tfm, sd_vtx->P);
    }
  }
  else { /* if (sd_vtx->type & PRIMITIVE_MOTION_TRIANGLE) */
    /* Load triangle vertices and normals. */
    motion_triangle_vertices_and_normals(
        kg, sd_vtx->object, sd_vtx->prim, sd_vtx->time, verts, normals);

    /* Compute refined position. */
    sd_vtx->P = motion_triangle_point_from_uv(kg, sd_vtx, isect->u, isect->v, verts);
  }

  /* Instance transform. */
  if (!(sd_vtx->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    object_position_transform_auto(kg, sd_vtx, &verts[0]);
    object_position_transform_auto(kg, sd_vtx, &verts[1]);
    object_position_transform_auto(kg, sd_vtx, &verts[2]);
    object_normal_transform_auto(kg, sd_vtx, &normals[0]);
    object_normal_transform_auto(kg, sd_vtx, &normals[1]);
    object_normal_transform_auto(kg, sd_vtx, &normals[2]);
  }

  /* Tangent space (position derivatives) WRT barycentric (u, v). */
  float3 dp_du = verts[1] - verts[0];
  float3 dp_dv = verts[2] - verts[0];

  /* Geometric normal. */
  vtx->ng = normalize(cross(dp_du, dp_dv));
  if (sd_vtx->object_flag & SD_OBJECT_NEGATIVE_SCALE) {
    vtx->ng = -vtx->ng;
  }

  /* Shading normals: Interpolate normals between vertices. */
  float n_len;
  vtx->n = normalize_len(normals[0] * (1.0f - sd_vtx->u - sd_vtx->v) + normals[1] * sd_vtx->u +
                             normals[2] * sd_vtx->v,
                         &n_len);

  /* Shading normal derivatives WRT barycentric (u, v)
   * we calculate the derivative of n = |u*n0 + v*n1 + (1-u-v)*n2| using:
   * d/du [f(u)/|f(u)|] = [d/du f(u)]/|f(u)| - f(u)/|f(u)|^3 <f(u), d/du f(u)>. */
  const float inv_n_len = 1.f / n_len;
  float3 dn_du = inv_n_len * (normals[1] - normals[0]);
  float3 dn_dv = inv_n_len * (normals[2] - normals[0]);
  dn_du -= vtx->n * dot(vtx->n, dn_du);
  dn_dv -= vtx->n * dot(vtx->n, dn_dv);

  /* Orthonormalize (dp_du,dp_dv) using a linear transformation, which
   * we use on (dn_du,dn_dv) as well so the new (u,v) are consistent. */
  const float inv_len_dp_du = 1.f / len(dp_du);
  dp_du *= inv_len_dp_du;
  dn_du *= inv_len_dp_du;

  const float dpdu_dot_dpdv = dot(dp_du, dp_dv);
  dp_dv -= dpdu_dot_dpdv * dp_du;
  dn_dv -= dpdu_dot_dpdv * dn_du;

  const float inv_len_dp_dv = 1.f / len(dp_dv);
  dp_dv *= inv_len_dp_dv;
  dn_dv *= inv_len_dp_dv;

  /* Find consistent tangent frame for every point on the surface. */
  make_orthonormals(vtx->ng, &vtx->dp_du, &vtx->dp_dv);
  /* Apply the equivalent rotation to the normal derivatives. */
  const float cos_theta = dot(dp_du, vtx->dp_du);
  const float sin_theta = -dot(dp_dv, vtx->dp_du);
  vtx->dn_du = cos_theta * dn_du - sin_theta * dn_dv;
  vtx->dn_dv = sin_theta * dn_du + cos_theta * dn_dv;

  /* Manifold vertex position. */
  vtx->p = sd_vtx->P;

  /* Initialize constraint and its derivates. */
  vtx->a = vtx->c = zero_float4();
  vtx->b = make_float4(1.f, 0.f, 0.f, 1.f);
  vtx->constraint = zero_float2();
  vtx->n_offset = n_offset;

  /* Closure information. */
  vtx->bsdf = bsdf;
  vtx->eta = eta;

  /* Geometric information. */
  vtx->uv = make_float2(isect->u, isect->v);
  vtx->object = sd_vtx->object;
  vtx->prim = sd_vtx->prim;
  vtx->shader = sd_vtx->shader;
}

/* Compute constraint derivatives. */

#if defined(__KERNEL_METAL__)
/* Temporary workaround for front-end compilation bug (incorrect MNEE rendering when this is
 * inlined). */
__attribute__((noinline))
#else
ccl_device_forceinline
#endif
bool mnee_compute_constraint_derivatives(
  const int vertex_count,
    ccl_private ManifoldVertex *vertices,
     const ccl_private  float3 &surface_sample_pos,
    const bool light_fixed_direction,
    const float3 light_sample)
{
  for (int vi = 0; vi < vertex_count; vi++) {
    ccl_private ManifoldVertex &v = vertices[vi];

    /* Direction toward surface sample. */
    float3 wi = (vi == 0) ? surface_sample_pos - v.p : vertices[vi - 1].p - v.p;
    float ili = len(wi);
    if (ili < MNEE_MIN_DISTANCE) {
      return false;
    }
    ili = 1.f / ili;
    wi *= ili;

    /* Direction toward light sample. */
    float3 wo = (vi == vertex_count - 1) ?
                    (light_fixed_direction ? light_sample : light_sample - v.p) :
                    vertices[vi + 1].p - v.p;
    float ilo = len(wo);
    if (ilo < MNEE_MIN_DISTANCE) {
      return false;
    }
    ilo = 1.f / ilo;
    wo *= ilo;

    /* Invert ior if coming from inside. */
    float eta = v.eta;
    if (dot(wi, v.ng) < .0f) {
      eta = 1.f / eta;
    }

    /* Half vector. */
    float3 H = -(wi + eta * wo);
    const float ilh = 1.f / len(H);
    H *= ilh;

    ilo *= eta * ilh;
    ili *= ilh;

    /* Local shading frame. */
    const float dp_du_dot_n = dot(v.dp_du, v.n);
    float3 s = v.dp_du - dp_du_dot_n * v.n;
    const float inv_len_s = 1.f / len(s);
    s *= inv_len_s;
    const float3 t = cross(v.n, s);

    float3 dH_du;
    float3 dH_dv;

    /* Constraint derivatives WRT previous vertex. */
    if (vi > 0) {
      const ccl_private ManifoldVertex &v_prev = vertices[vi - 1];
      dH_du = (v_prev.dp_du - wi * dot(wi, v_prev.dp_du)) * ili;
      dH_dv = (v_prev.dp_dv - wi * dot(wi, v_prev.dp_dv)) * ili;
      dH_du -= H * dot(dH_du, H);
      dH_dv -= H * dot(dH_dv, H);
      dH_du = -dH_du;
      dH_dv = -dH_dv;

      v.a = make_float4(dot(dH_du, s), dot(dH_dv, s), dot(dH_du, t), dot(dH_dv, t));
    }

    /* Constraint derivatives WRT current vertex. */
    if (vi == vertex_count - 1 && light_fixed_direction) {
      dH_du = ili * (-v.dp_du + wi * dot(wi, v.dp_du));
      dH_dv = ili * (-v.dp_dv + wi * dot(wi, v.dp_dv));
    }
    else {
      dH_du = -v.dp_du * (ili + ilo) + wi * (dot(wi, v.dp_du) * ili) +
              wo * (dot(wo, v.dp_du) * ilo);
      dH_dv = -v.dp_dv * (ili + ilo) + wi * (dot(wi, v.dp_dv) * ili) +
              wo * (dot(wo, v.dp_dv) * ilo);
    }
    dH_du -= H * dot(dH_du, H);
    dH_dv -= H * dot(dH_dv, H);
    dH_du = -dH_du;
    dH_dv = -dH_dv;

    float3 ds_du = -inv_len_s * (dot(v.dp_du, v.dn_du) * v.n + dp_du_dot_n * v.dn_du);
    float3 ds_dv = -inv_len_s * (dot(v.dp_du, v.dn_dv) * v.n + dp_du_dot_n * v.dn_dv);
    ds_du -= s * dot(s, ds_du);
    ds_dv -= s * dot(s, ds_dv);
    const float3 dt_du = cross(v.dn_du, s) + cross(v.n, ds_du);
    const float3 dt_dv = cross(v.dn_dv, s) + cross(v.n, ds_dv);

    v.b = make_float4(dot(dH_du, s) + dot(H, ds_du),
                      dot(dH_dv, s) + dot(H, ds_dv),
                      dot(dH_du, t) + dot(H, dt_du),
                      dot(dH_dv, t) + dot(H, dt_dv));

    /* Constraint derivatives WRT next vertex. */
    if (vi < vertex_count - 1) {
      const ccl_private ManifoldVertex &v_next = vertices[vi + 1];
      dH_du = (v_next.dp_du - wo * dot(wo, v_next.dp_du)) * ilo;
      dH_dv = (v_next.dp_dv - wo * dot(wo, v_next.dp_dv)) * ilo;
      dH_du -= H * dot(dH_du, H);
      dH_dv -= H * dot(dH_dv, H);
      dH_du = -dH_du;
      dH_dv = -dH_dv;

      v.c = make_float4(dot(dH_du, s), dot(dH_dv, s), dot(dH_du, t), dot(dH_dv, t));
    }

    /* Constraint vector WRT. the local shading frame. */
    v.constraint = make_float2(dot(s, H), dot(t, H)) - v.n_offset;
  }
  return true;
}

/* Invert (block) constraint derivative matrix and solve linear system so we can map dh back to dx:
 *  dh / dx = A
 *  dx = inverse(A) x dh
 *  to use for specular manifold walk
 * (See for example http://faculty.washington.edu/finlayso/ebook/algebraic/advanced/LUtri.htm
 *  for block tridiagonal matrix based linear system solve) */
ccl_device_forceinline bool mnee_solve_matrix_h_to_x(const int vertex_count,
                                                     ccl_private ManifoldVertex *vertices,
                                                     ccl_private float2 *dx)
{
  float4 Li[MNEE_MAX_CAUSTIC_CASTERS];
  float2 C[MNEE_MAX_CAUSTIC_CASTERS];

  /* Block tridiagonal LU factorization. */
  float4 Lk = vertices[0].b;
  if (mat22_inverse(Lk, Li[0]) == 0.f) {
    return false;
  }

  C[0] = vertices[0].constraint;

  for (int k = 1; k < vertex_count; k++) {
    const float4 A = mat22_mult(vertices[k].a, Li[k - 1]);

    Lk = vertices[k].b - mat22_mult(A, vertices[k - 1].c);
    if (mat22_inverse(Lk, Li[k]) == 0.f) {
      return false;
    }

    C[k] = vertices[k].constraint - mat22_mult(A, C[k - 1]);
  }

  dx[vertex_count - 1] = mat22_mult(Li[vertex_count - 1], C[vertex_count - 1]);
  for (int k = vertex_count - 2; k > -1; k--) {
    dx[k] = mat22_mult(Li[k], C[k] - mat22_mult(vertices[k].c, dx[k + 1]));
  }

  return true;
}

/* Newton solver to walk on specular manifold. */
ccl_device_forceinline bool mnee_newton_solver(KernelGlobals kg,
                                               const ccl_private ShaderData *sd,
                                               ccl_private ShaderData *sd_vtx,
                                               const ccl_private LightSample *ls,
                                               const bool light_fixed_direction,
                                               const int vertex_count,
                                               ccl_private ManifoldVertex *vertices)
{
  float2 dx[MNEE_MAX_CAUSTIC_CASTERS];
  ManifoldVertex tentative[MNEE_MAX_CAUSTIC_CASTERS];

  Ray projection_ray;
  projection_ray.self.light_object = OBJECT_NONE;
  projection_ray.self.light_prim = PRIM_NONE;
  projection_ray.self.light = LAMP_NONE;
  projection_ray.dP = differential_make_compact(sd->dP);
  projection_ray.dD = differential_zero_compact();
  projection_ray.tmin = 0.0f;
  projection_ray.time = sd->time;
  Intersection projection_isect;

  const float3 light_sample = light_fixed_direction ? ls->D : ls->P;

  /* We start gently, potentially ramping up to beta = 1, since target configurations
   * far from the seed path can send the proposed solution further than the linearized
   * local differential geometric quantities are meant for (especially dn/du and dn/dv). */
  float beta = .1f;
  bool reduce_stepsize = false;
  bool resolve_constraint = true;
  for (int iteration = 0; iteration < MNEE_MAX_ITERATIONS; iteration++) {
    if (resolve_constraint) {
      /* Calculate constraint and its derivatives for vertices. */
      if (!mnee_compute_constraint_derivatives(
              vertex_count, vertices, sd->P, light_fixed_direction, light_sample))
      {
        return false;
      }

      /* Calculate constraint norm. */
      float constraint_norm = 0.f;
      for (int vi = 0; vi < vertex_count; vi++) {
        constraint_norm = fmaxf(constraint_norm, len(vertices[vi].constraint));
      }

      /* Return if solve successful. */
      if (constraint_norm < MNEE_SOLVER_THRESHOLD) {
        return true;
      }

      /* Invert derivative matrix. */
      if (!mnee_solve_matrix_h_to_x(vertex_count, vertices, dx)) {
        return false;
      }
    }

    /* Construct tentative new vertices and project back onto surface. */
    for (int vi = 0; vi < vertex_count; vi++) {
      const ccl_private ManifoldVertex &mv = vertices[vi];

      /* Tentative new position on linearized manifold (tangent plane). */
      float3 tentative_p = mv.p - beta * (dx[vi].x * mv.dp_du + dx[vi].y * mv.dp_dv);

      /* For certain configs, the first solve ends up below the receiver. */
      if (vi == 0) {
        const float3 wo = tentative_p - sd->P;
        if (dot(sd->Ng, wo) <= 0.f) {
          /* Change direction for the 1st interface. */
          tentative_p = mv.p + beta * (dx[vi].x * mv.dp_du + dx[vi].y * mv.dp_dv);
        }
      }

      /* Project tentative point from tangent plane back to surface
       * we ignore all other intersections since this tentative path could lead
       * valid to a valid path even if occluded. */
      if (vi == 0) {
        projection_ray.self.object = sd->object;
        projection_ray.self.prim = sd->prim;
        projection_ray.P = sd->P;
      }
      else {
        const ccl_private ManifoldVertex &pv = vertices[vi - 1];
        projection_ray.self.object = pv.object;
        projection_ray.self.prim = pv.prim;
        projection_ray.P = pv.p;
      }
      projection_ray.D = normalize_len(tentative_p - projection_ray.P, &projection_ray.tmax);
      projection_ray.tmax *= MNEE_PROJECTION_DISTANCE_MULTIPLIER;

      bool projection_success = false;
      for (int isect_count = 0; isect_count < MNEE_MAX_INTERSECTION_COUNT; isect_count++) {
        const bool hit = scene_intersect(
            kg, &projection_ray, PATH_RAY_TRANSMIT, &projection_isect);
        if (!hit) {
          break;
        }

        if (projection_isect.object == mv.object) {
          projection_success = true;
          break;
        }

        projection_ray.self.object = projection_isect.object;
        projection_ray.self.prim = projection_isect.prim;
        projection_ray.tmin = intersection_t_offset(projection_isect.t);
      }
      if (!projection_success) {
        reduce_stepsize = true;
        break;
      }

      /* Initialize tangent frame, which will be used as reference. */
      ccl_private ManifoldVertex &tv = tentative[vi];
      tv.p = mv.p;
      tv.dp_du = mv.dp_du;
      tv.dp_dv = mv.dp_dv;

      /* Setup corrected manifold vertex. */
      mnee_setup_manifold_vertex(
          kg, &tv, mv.bsdf, mv.eta, mv.n_offset, &projection_ray, &projection_isect, sd_vtx);

      /* Fail newton solve if we are not making progress, probably stuck trying to move off the
       * edge of the mesh. */
      const float distance = len(tv.p - mv.p);
      if (distance < MNEE_MIN_PROGRESS_DISTANCE) {
        return false;
      }
    }

    /* Check that tentative path is still transmissive. */
    if (!reduce_stepsize) {
      for (int vi = 0; vi < vertex_count; vi++) {
        const ccl_private ManifoldVertex &tv = tentative[vi];

        /* Direction toward surface sample. */
        const float3 wi = (vi == 0 ? sd->P : tentative[vi - 1].p) - tv.p;
        /* Direction toward light sample. */
        const float3 wo = (vi == vertex_count - 1) ? light_fixed_direction ? ls->D : ls->P - tv.p :
                                                     tentative[vi + 1].p - tv.p;

        if (dot(tv.n, wi) * dot(tv.n, wo) >= 0.f) {
          reduce_stepsize = true;
          break;
        }
      }
    }

    if (reduce_stepsize) {
      /* Adjust step if can't land on right surface. */
      reduce_stepsize = false;
      resolve_constraint = false;
      beta *= .5f;

      /* Fail newton solve if the stepsize is too small. */
      if (beta < MNEE_MINIMUM_STEP_SIZE) {
        return false;
      }

      continue;
    }

    /* Copy tentative vertices to main vertex list. */
    for (int vi = 0; vi < vertex_count; vi++) {
      vertices[vi] = tentative[vi];
    }

    /* Increase the step to get back to 1. */
    resolve_constraint = true;
    beta = min(1.f, 2.f * beta);
  }

  return false;
}

/* Sample bsdf in half-vector measure. */
ccl_device_forceinline float2 mnee_sample_bsdf_dh(ClosureType type,
                                                  const float alpha_x,
                                                  const float alpha_y,
                                                  const float sample_u,
                                                  const float sample_v)
{
  float alpha2;
  float cos_phi;
  float sin_phi;

  if (alpha_x == alpha_y) {
    const float phi = sample_v * M_2PI_F;
    fast_sincosf(phi, &sin_phi, &cos_phi);
    alpha2 = alpha_x * alpha_x;
  }
  else {
    float phi = atanf(alpha_y / alpha_x * tanf(M_2PI_F * sample_v + M_PI_2_F));
    if (sample_v > .5f) {
      phi += M_PI_F;
    }
    fast_sincosf(phi, &sin_phi, &cos_phi);
    const float alpha_x2 = alpha_x * alpha_x;
    const float alpha_y2 = alpha_y * alpha_y;
    alpha2 = 1.f / (cos_phi * cos_phi / alpha_x2 + sin_phi * sin_phi / alpha_y2);
  }

  /* Map sampled angles to micro-normal direction h. */
  float tan2_theta = alpha2;
  if (type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID) {
    tan2_theta *= -logf(1.0f - sample_u);
  }
  else { /* type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID assumed */
    tan2_theta *= sample_u / (1.0f - sample_u);
  }
  const float cos2_theta = 1.0f / (1.0f + tan2_theta);
  const float sin_theta = safe_sqrtf(1.0f - cos2_theta);
  return make_float2(cos_phi * sin_theta, sin_phi * sin_theta);
}

/* Evaluate product term inside eq.6 at solution interface vi
 * divided by corresponding sampled pdf:
 * fr(vi)_do / pdf_dh(vi) x |do/dh| x |n.wo / n.h|
 * We assume here that the pdf (in half-vector measure) is the same as
 * the one calculation when sampling the microfacet normals from the
 * specular chain above: this allows us to simplify the bsdf weight */
ccl_device_forceinline Spectrum mnee_eval_bsdf_contribution(KernelGlobals kg,
                                                            ccl_private ShaderClosure *closure,
                                                            const float3 wi,
                                                            const float3 wo)
{
  ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)closure;

  const float cosNI = dot(bsdf->N, wi);
  const float cosNO = dot(bsdf->N, wo);

  const float3 Ht = normalize(-(bsdf->ior * wo + wi));
  const float cosHI = dot(Ht, wi);

  const float alpha2 = bsdf->alpha_x * bsdf->alpha_y;
  const float cosThetaM = dot(bsdf->N, Ht);

  /* Now calculate G1(i, m) and G1(o, m). */
  float G;
  if (bsdf->type == CLOSURE_BSDF_MICROFACET_BECKMANN_REFRACTION_ID) {
    G = bsdf_G<MicrofacetType::BECKMANN>(alpha2, cosNI, cosNO);
  }
  else { /* bsdf->type == CLOSURE_BSDF_MICROFACET_GGX_REFRACTION_ID assumed */
    G = bsdf_G<MicrofacetType::GGX>(alpha2, cosNI, cosNO);
  }

  Spectrum reflectance;
  Spectrum transmittance;
  microfacet_fresnel(kg, bsdf, cosHI, nullptr, &reflectance, &transmittance);

  /*
   * bsdf_do = (1 - F) * D_do * G * |h.wi| / (n.wi * n.wo)
   *  pdf_dh = D_dh * cosThetaM
   *    D_do = D_dh * |dh/do|
   *
   * contribution = bsdf_do * |do/dh| * |n.wo / n.h| / pdf_dh
   *              = (1 - F) * G * |h.wi / (n.wi * n.h^2)|
   */
  /* TODO: energy compensation for multi-GGX. */
  return bsdf->weight * transmittance * G * fabsf(cosHI / (cosNI * sqr(cosThetaM)));
}

/* Compute transfer matrix determinant |T1| = |dx1/dxn| (and |dh/dx| in the process) */
ccl_device_forceinline bool mnee_compute_transfer_matrix(const ccl_private ShaderData *sd,
                                                         const ccl_private LightSample *ls,
                                                         const bool light_fixed_direction,
                                                         const int vertex_count,
                                                         ccl_private ManifoldVertex *vertices,
                                                         ccl_private float *dx1_dxlight,
                                                         ccl_private float *dh_dx)
{
  /* Simplified block tridiagonal LU factorization. */
  float4 Li;
  float4 U[MNEE_MAX_CAUSTIC_CASTERS - 1];

  float4 Lk = vertices[0].b;
  float Lk_det = mat22_inverse(Lk, Li);
  if (Lk_det == 0.f) {
    return false;
  }

  float det_dh_dx = Lk_det;

  for (int k = 1; k < vertex_count; k++) {
    U[k - 1] = mat22_mult(Li, vertices[k - 1].c);

    Lk = vertices[k].b - mat22_mult(vertices[k].a, U[k - 1]);
    Lk_det = mat22_inverse(Lk, Li);
    if (Lk_det == 0.f) {
      return false;
    }

    det_dh_dx *= Lk_det;
  }

  /* Fill out constraint derivatives WRT light vertex param. */

  /* Local shading frame at last free vertex. */
  const int mi = vertex_count - 1;
  const ccl_private ManifoldVertex &m = vertices[mi];

  const float3 s = normalize(m.dp_du - dot(m.dp_du, m.n) * m.n);
  const float3 t = cross(m.n, s);

  /* Local differential geometry. */
  float3 dp_du;
  float3 dp_dv;
  make_orthonormals(ls->Ng, &dp_du, &dp_dv);

  /* Direction toward surface sample. */
  float3 wi = vertex_count == 1 ? sd->P - m.p : vertices[mi - 1].p - m.p;
  const float ili = 1.f / len(wi);
  wi *= ili;

  /* Invert ior if coming from inside. */
  float eta = m.eta;
  if (dot(wi, m.ng) < .0f) {
    eta = 1.f / eta;
  }

  float dxn_dwn;
  float4 dc_dlight;

  if (light_fixed_direction) {
    /* Constant direction toward light sample. */
    const float3 wo = ls->D;

    /* Half vector. */
    float3 H = -(wi + eta * wo);
    const float ilh = 1.f / len(H);
    H *= ilh;

    const float ilo = -eta * ilh;

    const float cos_theta = dot(wo, m.n);
    const float sin_theta = sin_from_cos(cos_theta);
    const float cos_phi = dot(wo, s);
    const float sin_phi = sin_from_cos(cos_phi);

    /* Wo = (cos_phi * sin_theta) * s + (sin_phi * sin_theta) * t + cos_theta * n. */
    float3 dH_dtheta = ilo * (cos_theta * (cos_phi * s + sin_phi * t) - sin_theta * m.n);
    float3 dH_dphi = ilo * sin_theta * (-sin_phi * s + cos_phi * t);
    dH_dtheta -= H * dot(dH_dtheta, H);
    dH_dphi -= H * dot(dH_dphi, H);

    /* Constraint derivatives WRT light direction expressed
     * in spherical coordinates (theta, phi). */
    dc_dlight = make_float4(
        dot(dH_dtheta, s), dot(dH_dphi, s), dot(dH_dtheta, t), dot(dH_dphi, t));

    /* Jacobian to convert dtheta x dphi to dw measure. */
    dxn_dwn = 1.f / fmaxf(MNEE_MIN_DISTANCE, fabsf(sin_theta));
  }
  else {
    /* Direction toward light sample. */
    float3 wo = ls->P - m.p;
    float ilo = 1.f / len(wo);
    wo *= ilo;

    /* Half vector. */
    float3 H = -(wi + eta * wo);
    const float ilh = 1.f / len(H);
    H *= ilh;

    ilo *= eta * ilh;

    float3 dH_du = (dp_du - wo * dot(wo, dp_du)) * ilo;
    float3 dH_dv = (dp_dv - wo * dot(wo, dp_dv)) * ilo;
    dH_du -= H * dot(dH_du, H);
    dH_dv -= H * dot(dH_dv, H);
    dH_du = -dH_du;
    dH_dv = -dH_dv;

    dc_dlight = make_float4(dot(dH_du, s), dot(dH_dv, s), dot(dH_du, t), dot(dH_dv, t));

    /* Neutral value since dc_dlight is already in the desired vertex area measure. */
    dxn_dwn = 1.f;
  }

  /* Compute transfer matrix. */
  float4 Tp = -mat22_mult(Li, dc_dlight);
  for (int k = vertex_count - 2; k > -1; k--) {
    Tp = -mat22_mult(U[k], Tp);
  }

  *dx1_dxlight = fabsf(mat22_determinant(Tp)) * dxn_dwn;
  *dh_dx = fabsf(det_dh_dx);
  return true;
}

/* Calculate the path contribution. */
ccl_device_forceinline bool mnee_path_contribution(KernelGlobals kg,
                                                   IntegratorState state,
                                                   ccl_private ShaderData *sd,
                                                   ccl_private ShaderData *sd_mnee,
                                                   ccl_private LightSample *ls,
                                                   const bool light_fixed_direction,
                                                   const int vertex_count,
                                                   ccl_private ManifoldVertex *vertices,
                                                   ccl_private BsdfEval *throughput)
{
  float wo_len;
  float3 wo = normalize_len(vertices[0].p - sd->P, &wo_len);

  /* Initialize throughput and evaluate receiver bsdf * |n.wo|. */
  surface_shader_bsdf_eval(kg, state, sd, wo, throughput, ls->shader);

  /* Update light sample with new position / direction and keep pdf in vertex area measure. */
  const uint32_t path_flag = INTEGRATOR_STATE(state, path, flag);
  light_sample_update(
      kg, ls, vertices[vertex_count - 1].p, vertices[vertex_count - 1].n, path_flag);

  /* Save state path bounce info in case a light path node is used in the refractive interface or
   * light shader graph. */
  const int transmission_bounce = INTEGRATOR_STATE(state, path, transmission_bounce);
  const int diffuse_bounce = INTEGRATOR_STATE(state, path, diffuse_bounce);
  const int bounce = INTEGRATOR_STATE(state, path, bounce);

  /* Set diffuse bounce info. */
  INTEGRATOR_STATE_WRITE(state, path, diffuse_bounce) = diffuse_bounce + 1;

  /* Evaluate light sample
   * in case the light has a node-based shader:
   * 1. sd_mnee will be used to store light data, which is why we need to do
   *    this evaluation here. sd_mnee needs to contain the solution's last
   *    interface data at the end of the call for the shadow ray setup to work.
   * 2. ls needs to contain the last interface data for the light shader to
   *    evaluate properly */

  /* Set bounce info in case a light path node is used in the light shader graph. */
  INTEGRATOR_STATE_WRITE(state, path, transmission_bounce) = transmission_bounce + vertex_count -
                                                             1;
  INTEGRATOR_STATE_WRITE(state, path, bounce) = bounce + vertex_count;

  const Spectrum light_eval = light_sample_shader_eval(kg, state, sd_mnee, ls, sd->time);
  bsdf_eval_mul(throughput, light_eval / ls->pdf);

  /* Generalized geometry term. */

  float dh_dx;
  float dx1_dxlight;
  if (!mnee_compute_transfer_matrix(
          sd, ls, light_fixed_direction, vertex_count, vertices, &dx1_dxlight, &dh_dx))
  {
    return false;
  }

  /* Receiver bsdf eval above already contains |n.wo|. */
  const float dw0_dx1 = fabsf(dot(wo, vertices[0].n)) / sqr(wo_len);

  /* Clamp since it has a tendency to be unstable. */
  const float G = fminf(dw0_dx1 * dx1_dxlight, 2.f);
  bsdf_eval_mul(throughput, G);

  /* Specular reflectance. */

  /* Probe ray / isect. */
  Ray probe_ray;
  probe_ray.self.light_object = ls->object;
  probe_ray.self.light_prim = ls->prim;
  probe_ray.self.light = ls->lamp;
  probe_ray.tmin = 0.0f;
  probe_ray.dP = differential_make_compact(sd->dP);
  probe_ray.dD = differential_zero_compact();
  probe_ray.time = sd->time;
  Intersection probe_isect;

  probe_ray.self.object = sd->object;
  probe_ray.self.prim = sd->prim;
  probe_ray.P = sd->P;

  float3 wi;
  float wi_len;
  for (int vi = 0; vi < vertex_count; vi++) {
    const ccl_private ManifoldVertex &v = vertices[vi];

    /* Check visibility. */
    probe_ray.D = normalize_len(v.p - probe_ray.P, &probe_ray.tmax);
    if (scene_intersect(kg, &probe_ray, PATH_RAY_TRANSMIT, &probe_isect)) {
      const int hit_object = (probe_isect.object == OBJECT_NONE) ?
                                 kernel_data_fetch(prim_object, probe_isect.prim) :
                                 probe_isect.object;
      /* Test whether the ray hit the appropriate object at its intended location. */
      if (hit_object != v.object || fabsf(probe_ray.tmax - probe_isect.t) > MNEE_MIN_DISTANCE) {
        return false;
      }
    }
    probe_ray.self.object = v.object;
    probe_ray.self.prim = v.prim;
    probe_ray.P = v.p;

    /* Set view looking direction. */
    wi = -wo;
    wi_len = wo_len;

    /* Setup shader data for vertex vi. */
    shader_setup_from_sample(kg,
                             sd_mnee,
                             v.p,
                             v.n,
                             wi,
                             v.shader,
                             v.object,
                             v.prim,
                             v.uv.x,
                             v.uv.y,
                             wi_len,
                             sd->time,
                             false,
                             LAMP_NONE);

    /* Set bounce info in case a light path node is used in the refractive interface
     * shader graph. */
    INTEGRATOR_STATE_WRITE(state, path, transmission_bounce) = transmission_bounce + vi;
    INTEGRATOR_STATE_WRITE(state, path, bounce) = bounce + 1 + vi;

    /* Evaluate shader nodes at solution vi. */
    surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW>(
        kg, state, sd_mnee, nullptr, PATH_RAY_DIFFUSE, true);

    /* Set light looking direction. */
    wo = (vi == vertex_count - 1) ? (light_fixed_direction ? ls->D : ls->P - v.p) :
                                    vertices[vi + 1].p - v.p;
    wo = normalize_len(wo, &wo_len);

    /* Evaluate product term inside eq.6 at solution interface. vi
     * divided by corresponding sampled pdf:
     * fr(vi)_do / pdf_dh(vi) x |do/dh| x |n.wo / n.h| */
    const Spectrum bsdf_contribution = mnee_eval_bsdf_contribution(kg, v.bsdf, wi, wo);
    bsdf_eval_mul(throughput, bsdf_contribution);
  }

  /* Restore original state path bounce info. */
  INTEGRATOR_STATE_WRITE(state, path, transmission_bounce) = transmission_bounce;
  INTEGRATOR_STATE_WRITE(state, path, diffuse_bounce) = diffuse_bounce;
  INTEGRATOR_STATE_WRITE(state, path, bounce) = bounce;

  return true;
}

/* Manifold next event estimation path sampling. */
ccl_device_forceinline int kernel_path_mnee_sample(KernelGlobals kg,
                                                   IntegratorState state,
                                                   ccl_private ShaderData *sd,
                                                   ccl_private ShaderData *sd_mnee,
                                                   const ccl_private RNGState *rng_state,
                                                   ccl_private LightSample *ls,
                                                   ccl_private BsdfEval *throughput)
{
  /*
   * 1. send seed ray from shading point to light sample position (or along sampled light
   * direction), making sure it intersects a caustic caster at least once, ignoring all other
   * intersections (the final path could be valid even though objects could occlude the light
   * this seed point), building an array of manifold vertices.
   */

  /* Setup probe ray. */
  Ray probe_ray;
  probe_ray.self.object = sd->object;
  probe_ray.self.prim = sd->prim;
  probe_ray.self.light_object = ls->object;
  probe_ray.self.light_prim = ls->prim;
  probe_ray.self.light = ls->lamp;
  probe_ray.P = sd->P;
  probe_ray.tmin = 0.0f;
  if (ls->t == FLT_MAX) {
    /* Distant / env light. */
    probe_ray.D = ls->D;
    probe_ray.tmax = ls->t;
  }
  else {
    /* Other lights, avoid self-intersection. */
    probe_ray.D = ls->P - probe_ray.P;
    probe_ray.D = normalize_len(probe_ray.D, &probe_ray.tmax);
  }
  probe_ray.dP = differential_make_compact(sd->dP);
  probe_ray.dD = differential_zero_compact();
  probe_ray.time = sd->time;
  Intersection probe_isect;

  ManifoldVertex vertices[MNEE_MAX_CAUSTIC_CASTERS];

  int vertex_count = 0;
  for (int isect_count = 0; isect_count < MNEE_MAX_INTERSECTION_COUNT; isect_count++) {
    const bool hit = scene_intersect(kg, &probe_ray, PATH_RAY_TRANSMIT, &probe_isect);
    if (!hit) {
      break;
    }

    const int object_flags = intersection_get_object_flags(kg, &probe_isect);
    if (object_flags & SD_OBJECT_CAUSTICS_CASTER) {

      /* Do we have enough slots. */
      if (vertex_count >= MNEE_MAX_CAUSTIC_CASTERS) {
        return 0;
      }

      /* Reject caster if it is not a triangles mesh. */
      if (!(probe_isect.type & PRIMITIVE_TRIANGLE)) {
        return 0;
      }

      ccl_private ManifoldVertex &mv = vertices[vertex_count++];

      /* Setup shader data on caustic caster and evaluate context. */
      shader_setup_from_ray(kg, sd_mnee, &probe_ray, &probe_isect);

      /* Reject caster if smooth normals are not available: Manifold exploration assumes local
       * differential geometry can be created at any point on the surface which is not possible if
       * normals are not smooth. */
      if (!(sd_mnee->shader & SHADER_SMOOTH_NORMAL)) {
        return 0;
      }

      /* Last bool argument is the MNEE flag (for TINY_MAX_CLOSURE cap in kernel_shader.h). */
      surface_shader_eval<KERNEL_FEATURE_NODE_MASK_SURFACE_SHADOW>(
          kg, state, sd_mnee, nullptr, PATH_RAY_DIFFUSE, true);

      /* Get and sample refraction bsdf */
      bool found_refractive_microfacet_bsdf = false;
      for (int ci = 0; ci < sd_mnee->num_closure; ci++) {
        ccl_private ShaderClosure *bsdf = &sd_mnee->closure[ci];
        if (CLOSURE_IS_REFRACTION(bsdf->type) || CLOSURE_IS_GLASS(bsdf->type)) {
          /* Note that Glass closures are treated as refractive further below. */

          found_refractive_microfacet_bsdf = true;
          ccl_private MicrofacetBsdf *microfacet_bsdf = (ccl_private MicrofacetBsdf *)bsdf;

          /* Figure out appropriate index of refraction ratio. */
          const float eta = (sd_mnee->flag & SD_BACKFACING) ? 1.0f / microfacet_bsdf->ior :
                                                              microfacet_bsdf->ior;

          float2 h = zero_float2();
          if (microfacet_bsdf->alpha_x > 0.f && microfacet_bsdf->alpha_y > 0.f) {
            /* Sample transmissive microfacet bsdf. */
            const float2 bsdf_uv = path_state_rng_2D(kg, rng_state, PRNG_SURFACE_BSDF);
            h = mnee_sample_bsdf_dh(bsdf->type,
                                    microfacet_bsdf->alpha_x,
                                    microfacet_bsdf->alpha_y,
                                    bsdf_uv.x,
                                    bsdf_uv.y);
          }

          /* Setup differential geometry on vertex. */
          mnee_setup_manifold_vertex(kg, &mv, bsdf, eta, h, &probe_ray, &probe_isect, sd_mnee);
          break;
        }
      }
      if (!found_refractive_microfacet_bsdf) {
        return 0;
      }
    }

    probe_ray.self.object = probe_isect.object;
    probe_ray.self.prim = probe_isect.prim;
    probe_ray.tmin = intersection_t_offset(probe_isect.t);
  };

  /* Mark the manifold walk invalid to keep mollification on by default. */
  INTEGRATOR_STATE_WRITE(state, path, mnee) &= ~PATH_MNEE_VALID;

  if (vertex_count == 0) {
    return 0;
  }

  /* Check whether the transmission depth limit is reached before continuing. */
  if ((INTEGRATOR_STATE(state, path, transmission_bounce) + vertex_count - 1) >=
      kernel_data.integrator.max_transmission_bounce)
  {
    return 0;
  }

  /* Check whether the diffuse depth limit is reached before continuing. */
  if ((INTEGRATOR_STATE(state, path, diffuse_bounce) + 1) >=
      kernel_data.integrator.max_diffuse_bounce)
  {
    return 0;
  }

  /* Check whether the overall depth limit is reached before continuing. */
  if ((INTEGRATOR_STATE(state, path, bounce) + vertex_count) >= kernel_data.integrator.max_bounce)
  {
    return 0;
  }

  /* Mark the manifold walk valid to turn off mollification regardless of how successful the walk
   * is: this is noticeable when another mnee is performed deeper in the path, for an internally
   * reflected ray for example. If mollification was active for the reflection, a clear
   * discontinuity is visible between direct and indirect contributions */
  INTEGRATOR_STATE_WRITE(state, path, mnee) |= PATH_MNEE_VALID;

  /* Distant or environment light. */
  bool light_fixed_direction = (ls->t == FLT_MAX);
  if (ls->type == LIGHT_AREA) {
    const ccl_global KernelLight *klight = &kernel_data_fetch(lights, ls->lamp);
    if (klight->area.tan_half_spread == 0.0f) {
      /* Area light with zero spread also has fixed direction. */
      light_fixed_direction = true;
    }
  }

  /* 2. Walk on the specular manifold to find vertices on the casters that satisfy snell's law for
   * each interface. */
  if (mnee_newton_solver(kg, sd, sd_mnee, ls, light_fixed_direction, vertex_count, vertices)) {
    /* 3. If a solution exists, calculate contribution of the corresponding path */
    if (!mnee_path_contribution(
            kg, state, sd, sd_mnee, ls, light_fixed_direction, vertex_count, vertices, throughput))
    {
      return 0;
    }

    return vertex_count;
  }

  return 0;
}

CCL_NAMESPACE_END
