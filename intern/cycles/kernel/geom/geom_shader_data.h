/*
 * Copyright 2011-2013 Blender Foundation
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

/* Functions to initialize ShaderData given.
 *
 * Could be from an incoming ray, intersection or sampled position. */

#pragma once

CCL_NAMESPACE_BEGIN

/* ShaderData setup from incoming ray */

#ifdef __OBJECT_MOTION__
ccl_device void shader_setup_object_transforms(ccl_global const KernelGlobals *ccl_restrict kg,
                                               ccl_private ShaderData *ccl_restrict sd,
                                               float time)
{
  if (sd->object_flag & SD_OBJECT_MOTION) {
    sd->ob_tfm_motion = object_fetch_transform_motion(kg, sd->object, time);
    sd->ob_itfm_motion = transform_quick_inverse(sd->ob_tfm_motion);
  }
}
#endif

/* TODO: break this up if it helps reduce register pressure to load data from
 * global memory as we write it to shader-data. */
ccl_device_inline void shader_setup_from_ray(ccl_global const KernelGlobals *ccl_restrict kg,
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
  sd->object_flag = kernel_tex_fetch(__object_flag, sd->object);
  sd->prim = isect->prim;
  sd->lamp = LAMP_NONE;
  sd->flag = 0;

  /* Read matrices and time. */
  sd->time = ray->time;

#ifdef __OBJECT_MOTION__
  shader_setup_object_transforms(kg, sd, ray->time);
#endif

  /* Read ray data into shader globals. */
  sd->I = -ray->D;

#ifdef __HAIR__
  if (sd->type & PRIMITIVE_ALL_CURVE) {
    /* curve */
    curve_shader_setup(kg, sd, ray->P, ray->D, isect->t, isect->object, isect->prim);
  }
  else
#endif
      if (sd->type & PRIMITIVE_TRIANGLE) {
    /* static triangle */
    float3 Ng = triangle_normal(kg, sd);
    sd->shader = kernel_tex_fetch(__tri_shader, sd->prim);

    /* vectors */
    sd->P = triangle_refine(kg, sd, ray->P, ray->D, isect->t, isect->object, isect->prim);
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

  sd->flag |= kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).flags;

  if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
    /* instance transform */
    object_normal_transform_auto(kg, sd, &sd->N);
    object_normal_transform_auto(kg, sd, &sd->Ng);
#ifdef __DPDU__
    object_dir_transform_auto(kg, sd, &sd->dPdu);
    object_dir_transform_auto(kg, sd, &sd->dPdv);
#endif
  }

  /* backfacing test */
  bool backfacing = (dot(sd->Ng, sd->I) < 0.0f);

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
  differential_transfer_compact(&sd->dP, ray->dP, ray->D, ray->dD, sd->Ng, sd->ray_length);
  differential_incoming_compact(&sd->dI, ray->D, ray->dD);
  differential_dudv(&sd->du, &sd->dv, sd->dPdu, sd->dPdv, sd->dP, sd->Ng);
#endif
}

/* ShaderData setup from position sampled on mesh */

ccl_device_inline void shader_setup_from_sample(ccl_global const KernelGlobals *ccl_restrict kg,
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
  sd->I = I;
  sd->shader = shader;
  if (prim != PRIM_NONE)
    sd->type = PRIMITIVE_TRIANGLE;
  else if (lamp != LAMP_NONE)
    sd->type = PRIMITIVE_LAMP;
  else
    sd->type = PRIMITIVE_NONE;

  /* primitive */
  sd->object = object;
  sd->lamp = LAMP_NONE;
  /* Currently no access to bvh prim index for strand sd->prim. */
  sd->prim = prim;
  sd->u = u;
  sd->v = v;
  sd->time = time;
  sd->ray_length = t;

  sd->flag = kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).flags;
  sd->object_flag = 0;
  if (sd->object != OBJECT_NONE) {
    sd->object_flag |= kernel_tex_fetch(__object_flag, sd->object);

#ifdef __OBJECT_MOTION__
    shader_setup_object_transforms(kg, sd, time);
#endif
  }
  else if (lamp != LAMP_NONE) {
    sd->lamp = lamp;
  }

  /* transform into world space */
  if (object_space) {
    object_position_transform_auto(kg, sd, &sd->P);
    object_normal_transform_auto(kg, sd, &sd->Ng);
    sd->N = sd->Ng;
    object_dir_transform_auto(kg, sd, &sd->I);
  }

  if (sd->type & PRIMITIVE_TRIANGLE) {
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

  /* backfacing test */
  if (sd->prim != PRIM_NONE) {
    bool backfacing = (dot(sd->Ng, sd->I) < 0.0f);

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
  sd->dP = differential3_zero();
  sd->dI = differential3_zero();
  sd->du = differential_zero();
  sd->dv = differential_zero();
#endif
}

/* ShaderData setup for displacement */

ccl_device void shader_setup_from_displace(ccl_global const KernelGlobals *ccl_restrict kg,
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

  shader_setup_from_sample(
      kg,
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
      !(kernel_tex_fetch(__object_flag, object) & SD_OBJECT_TRANSFORM_APPLIED),
      LAMP_NONE);
}

/* ShaderData setup from ray into background */

ccl_device_inline void shader_setup_from_background(ccl_global const KernelGlobals *ccl_restrict
                                                        kg,
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
  sd->I = -ray_D;
  sd->shader = kernel_data.background.surface_shader;
  sd->flag = kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).flags;
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
  sd->dP = differential3_zero(); /* TODO: ray->dP */
  differential_incoming(&sd->dI, sd->dP);
  sd->du = differential_zero();
  sd->dv = differential_zero();
#endif
}

/* ShaderData setup from point inside volume */

#ifdef __VOLUME__
ccl_device_inline void shader_setup_from_volume(ccl_global const KernelGlobals *ccl_restrict kg,
                                                ccl_private ShaderData *ccl_restrict sd,
                                                ccl_private const Ray *ccl_restrict ray)
{

  /* vectors */
  sd->P = ray->P;
  sd->N = -ray->D;
  sd->Ng = -ray->D;
  sd->I = -ray->D;
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
  sd->dP = differential3_zero(); /* TODO ray->dD */
  differential_incoming(&sd->dI, sd->dP);
  sd->du = differential_zero();
  sd->dv = differential_zero();
#  endif

  /* for NDC coordinates */
  sd->ray_P = ray->P;
  sd->ray_dP = ray->dP;
}
#endif /* __VOLUME__ */

CCL_NAMESPACE_END
