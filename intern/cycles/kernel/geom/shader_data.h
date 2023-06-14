/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Functions to initialize ShaderData given.
 *
 * Could be from an incoming ray, intersection or sampled position. */

#pragma once

#include "kernel/util/differential.h"

CCL_NAMESPACE_BEGIN

/* ShaderData setup from incoming ray */

#ifdef __OBJECT_MOTION__
ccl_device void shader_setup_object_transforms(KernelGlobals kg,
                                               ccl_private ShaderData *ccl_restrict sd,
                                               float time)
{
  if (sd->object_flag & SD_OBJECT_MOTION) {
    sd->ob_tfm_motion = object_fetch_transform_motion(kg, sd->object, time);
    sd->ob_itfm_motion = transform_inverse(sd->ob_tfm_motion);
  }
}
#endif

/* TODO: break this up if it helps reduce register pressure to load data from
 * global memory as we write it to shader-data. */
ccl_device_inline void shader_setup_from_ray(KernelGlobals kg,
                                             ccl_private ShaderData *ccl_restrict sd,
                                             ccl_private const Ray *ccl_restrict ray,
                                             ccl_private const Intersection *ccl_restrict isect)
{
  /* Read intersection data into shader globals.
   *
   * TODO: this is redundant, could potentially remove some of this from
   * ShaderData but would need to ensure that it also works for shadow
   * shader evaluation. */
  sd->u = isect->u;
  sd->v = isect->v;
  sd->ray_length = isect->t;
  sd->type = isect->type;
  sd->object = isect->object;
  sd->object_flag = kernel_data_fetch(object_flag, sd->object);
  sd->prim = isect->prim;
  sd->lamp = LAMP_NONE;
  sd->flag = 0;

  /* Read matrices and time. */
  sd->time = ray->time;

#ifdef __OBJECT_MOTION__
  shader_setup_object_transforms(kg, sd, ray->time);
#endif

  /* Read ray data into shader globals. */
  sd->wi = -ray->D;

#ifdef __HAIR__
  if (sd->type & PRIMITIVE_CURVE) {
    /* curve */
    curve_shader_setup(kg, sd, ray->P, ray->D, isect->t, isect->object, isect->prim);
  }
  else
#endif
#ifdef __POINTCLOUD__
      if (sd->type & PRIMITIVE_POINT)
  {
    /* point */
    point_shader_setup(kg, sd, isect, ray);
  }
  else
#endif
  {
    if (sd->type == PRIMITIVE_TRIANGLE) {
      /* static triangle */
      float3 Ng = triangle_normal(kg, sd);
      sd->shader = kernel_data_fetch(tri_shader, sd->prim);

      /* vectors */
      sd->P = triangle_point_from_uv(kg, sd, isect->object, isect->prim, isect->u, isect->v);
      sd->Ng = Ng;
      sd->N = Ng;

      /* smooth normal */
      if (sd->shader & SHADER_SMOOTH_NORMAL)
        sd->N = triangle_smooth_normal(kg, Ng, sd->prim, sd->u, sd->v);

#ifdef __DPDU__
      /* dPdu/dPdv */
      triangle_dPdudv(kg, sd->prim, &sd->dPdu, &sd->dPdv);
#endif
    }
    else {
      /* motion triangle */
      motion_triangle_shader_setup(
          kg, sd, ray->P, ray->D, isect->t, isect->object, isect->prim, false);
    }

    if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      /* instance transform */
      object_normal_transform_auto(kg, sd, &sd->N);
      object_normal_transform_auto(kg, sd, &sd->Ng);
#ifdef __DPDU__
      object_dir_transform_auto(kg, sd, &sd->dPdu);
      object_dir_transform_auto(kg, sd, &sd->dPdv);
#endif
    }
  }

  sd->flag = kernel_data_fetch(shaders, (sd->shader & SHADER_MASK)).flags;

  /* backfacing test */
  bool backfacing = (dot(sd->Ng, sd->wi) < 0.0f);

  if (backfacing) {
    sd->flag |= SD_BACKFACING;
    sd->Ng = -sd->Ng;
    sd->N = -sd->N;
#ifdef __DPDU__
    sd->dPdu = -sd->dPdu;
    sd->dPdv = -sd->dPdv;
#endif
  }

#ifdef __RAY_DIFFERENTIALS__
  /* differentials */
  sd->dP = differential_transfer_compact(ray->dP, ray->D, ray->dD, sd->ray_length);
  sd->dI = differential_incoming_compact(ray->dD);
  differential_dudv_compact(&sd->du, &sd->dv, sd->dPdu, sd->dPdv, sd->dP, sd->Ng);
#endif
}

/* ShaderData setup from position sampled on mesh */

ccl_device_inline void shader_setup_from_sample(KernelGlobals kg,
                                                ccl_private ShaderData *ccl_restrict sd,
                                                const float3 P,
                                                const float3 Ng,
                                                const float3 I,
                                                int shader,
                                                int object,
                                                int prim,
                                                float u,
                                                float v,
                                                float t,
                                                float time,
                                                bool object_space,
                                                int lamp)
{
  /* vectors */
  sd->P = P;
  sd->N = Ng;
  sd->Ng = Ng;
  sd->wi = I;
  sd->shader = shader;
  if (lamp != LAMP_NONE) {
    sd->type = PRIMITIVE_LAMP;
  }
  else if (prim != PRIM_NONE) {
    sd->type = PRIMITIVE_TRIANGLE;
  }
  else {
    sd->type = PRIMITIVE_NONE;
  }

  /* primitive */
  sd->object = object;
  sd->lamp = LAMP_NONE;
  /* Currently no access to bvh prim index for strand sd->prim. */
  sd->prim = prim;
  sd->u = u;
  sd->v = v;
  sd->time = time;
  sd->ray_length = t;

  sd->flag = kernel_data_fetch(shaders, (sd->shader & SHADER_MASK)).flags;
  sd->object_flag = 0;
  if (sd->object != OBJECT_NONE) {
    sd->object_flag |= kernel_data_fetch(object_flag, sd->object);

#ifdef __OBJECT_MOTION__
    shader_setup_object_transforms(kg, sd, time);
#endif

    /* transform into world space */
    if (object_space) {
      object_position_transform_auto(kg, sd, &sd->P);
      object_normal_transform_auto(kg, sd, &sd->Ng);
      sd->N = sd->Ng;
      object_dir_transform_auto(kg, sd, &sd->wi);
    }

    if (sd->type == PRIMITIVE_TRIANGLE) {
      /* smooth normal */
      if (sd->shader & SHADER_SMOOTH_NORMAL) {
        sd->N = triangle_smooth_normal(kg, Ng, sd->prim, sd->u, sd->v);

        if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
          object_normal_transform_auto(kg, sd, &sd->N);
        }
      }

      /* dPdu/dPdv */
#ifdef __DPDU__
      triangle_dPdudv(kg, sd->prim, &sd->dPdu, &sd->dPdv);

      if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
        object_dir_transform_auto(kg, sd, &sd->dPdu);
        object_dir_transform_auto(kg, sd, &sd->dPdv);
      }
#endif
    }
    else {
#ifdef __DPDU__
      sd->dPdu = zero_float3();
      sd->dPdv = zero_float3();
#endif
    }
  }
  else {
    if (lamp != LAMP_NONE) {
      sd->lamp = lamp;
    }
#ifdef __DPDU__
    sd->dPdu = zero_float3();
    sd->dPdv = zero_float3();
#endif
  }

  /* backfacing test */
  if (sd->prim != PRIM_NONE) {
    bool backfacing = (dot(sd->Ng, sd->wi) < 0.0f);

    if (backfacing) {
      sd->flag |= SD_BACKFACING;
      sd->Ng = -sd->Ng;
      sd->N = -sd->N;
#ifdef __DPDU__
      sd->dPdu = -sd->dPdu;
      sd->dPdv = -sd->dPdv;
#endif
    }
  }

#ifdef __RAY_DIFFERENTIALS__
  /* no ray differentials here yet */
  sd->dP = differential_zero_compact();
  sd->dI = differential_zero_compact();
  sd->du = differential_zero();
  sd->dv = differential_zero();
#endif
}

/* ShaderData setup for displacement */

ccl_device void shader_setup_from_displace(KernelGlobals kg,
                                           ccl_private ShaderData *ccl_restrict sd,
                                           int object,
                                           int prim,
                                           float u,
                                           float v)
{
  float3 P, Ng, I = zero_float3();
  int shader;

  triangle_point_normal(kg, object, prim, u, v, &P, &Ng, &shader);

  /* force smooth shading for displacement */
  shader |= SHADER_SMOOTH_NORMAL;

  shader_setup_from_sample(kg,
                           sd,
                           P,
                           Ng,
                           I,
                           shader,
                           object,
                           prim,
                           u,
                           v,
                           0.0f,
                           0.5f,
                           !(kernel_data_fetch(object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED),
                           LAMP_NONE);
}

/* ShaderData setup for point on curve. */

ccl_device void shader_setup_from_curve(KernelGlobals kg,
                                        ccl_private ShaderData *ccl_restrict sd,
                                        int object,
                                        int prim,
                                        int segment,
                                        float u)
{
  /* Primitive */
  sd->type = PRIMITIVE_PACK_SEGMENT(PRIMITIVE_CURVE_THICK, segment);
  sd->lamp = LAMP_NONE;
  sd->prim = prim;
  sd->u = u;
  sd->v = 0.0f;
  sd->time = 0.5f;
  sd->ray_length = 0.0f;

  /* Shader */
  sd->shader = kernel_data_fetch(curves, prim).shader_id;
  sd->flag = kernel_data_fetch(shaders, (sd->shader & SHADER_MASK)).flags;

  /* Object */
  sd->object = object;
  sd->object_flag = kernel_data_fetch(object_flag, sd->object);
#ifdef __OBJECT_MOTION__
  shader_setup_object_transforms(kg, sd, sd->time);
#endif

  /* Get control points. */
  KernelCurve kcurve = kernel_data_fetch(curves, prim);

  int k0 = kcurve.first_key + PRIMITIVE_UNPACK_SEGMENT(sd->type);
  int k1 = k0 + 1;
  int ka = max(k0 - 1, kcurve.first_key);
  int kb = min(k1 + 1, kcurve.first_key + kcurve.num_keys - 1);

  float4 P_curve[4];

  P_curve[0] = kernel_data_fetch(curve_keys, ka);
  P_curve[1] = kernel_data_fetch(curve_keys, k0);
  P_curve[2] = kernel_data_fetch(curve_keys, k1);
  P_curve[3] = kernel_data_fetch(curve_keys, kb);

  /* Interpolate position and tangent. */
  sd->P = float4_to_float3(catmull_rom_basis_derivative(P_curve, sd->u));
#ifdef __DPDU__
  sd->dPdu = float4_to_float3(catmull_rom_basis_derivative(P_curve, sd->u));
#endif

  /* Transform into world space */
  if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    object_position_transform_auto(kg, sd, &sd->P);
#ifdef __DPDU__
    object_dir_transform_auto(kg, sd, &sd->dPdu);
#endif
  }

  /* No view direction, normals or bitangent. */
  sd->wi = zero_float3();
  sd->N = zero_float3();
  sd->Ng = zero_float3();
#ifdef __DPDU__
  sd->dPdv = zero_float3();
#endif

  /* No ray differentials currently. */
#ifdef __RAY_DIFFERENTIALS__
  sd->dP = differential_zero_compact();
  sd->dI = differential_zero_compact();
  sd->du = differential_zero();
  sd->dv = differential_zero();
#endif
}

/* ShaderData setup from ray into background */

ccl_device_inline void shader_setup_from_background(KernelGlobals kg,
                                                    ccl_private ShaderData *ccl_restrict sd,
                                                    const float3 ray_P,
                                                    const float3 ray_D,
                                                    const float ray_time)
{
  /* for NDC coordinates */
  sd->ray_P = ray_P;

  /* vectors */
  sd->P = ray_D;
  sd->N = -ray_D;
  sd->Ng = -ray_D;
  sd->wi = -ray_D;
  sd->shader = kernel_data.background.surface_shader;
  sd->flag = kernel_data_fetch(shaders, (sd->shader & SHADER_MASK)).flags;
  sd->object_flag = 0;
  sd->time = ray_time;
  sd->ray_length = 0.0f;

  sd->object = OBJECT_NONE;
  sd->lamp = LAMP_NONE;
  sd->prim = PRIM_NONE;
  sd->u = 0.0f;
  sd->v = 0.0f;

#ifdef __DPDU__
  /* dPdu/dPdv */
  sd->dPdu = zero_float3();
  sd->dPdv = zero_float3();
#endif

#ifdef __RAY_DIFFERENTIALS__
  /* differentials */
  sd->dP = differential_zero_compact(); /* TODO: ray->dP */
  sd->dI = differential_zero_compact();
  sd->du = differential_zero();
  sd->dv = differential_zero();
#endif
}

/* ShaderData setup from point inside volume */

#ifdef __VOLUME__
ccl_device_inline void shader_setup_from_volume(KernelGlobals kg,
                                                ccl_private ShaderData *ccl_restrict sd,
                                                ccl_private const Ray *ccl_restrict ray)
{

  /* vectors */
  sd->P = ray->P + ray->D * ray->tmin;
  sd->N = -ray->D;
  sd->Ng = -ray->D;
  sd->wi = -ray->D;
  sd->shader = SHADER_NONE;
  sd->flag = 0;
  sd->object_flag = 0;
  sd->time = ray->time;
  sd->ray_length = 0.0f; /* todo: can we set this to some useful value? */

  sd->object = OBJECT_NONE; /* todo: fill this for texture coordinates */
  sd->lamp = LAMP_NONE;
  sd->prim = PRIM_NONE;
  sd->type = PRIMITIVE_VOLUME;

  sd->u = 0.0f;
  sd->v = 0.0f;

#  ifdef __DPDU__
  /* dPdu/dPdv */
  sd->dPdu = zero_float3();
  sd->dPdv = zero_float3();
#  endif

#  ifdef __RAY_DIFFERENTIALS__
  /* differentials */
  sd->dP = differential_zero_compact(); /* TODO ray->dD */
  sd->dI = differential_zero_compact();
  sd->du = differential_zero();
  sd->dv = differential_zero();
#  endif

  /* for NDC coordinates */
  sd->ray_P = ray->P;
}
#endif /* __VOLUME__ */

CCL_NAMESPACE_END
