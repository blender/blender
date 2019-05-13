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

/*
 * ShaderData, used in four steps:
 *
 * Setup from incoming ray, sampled position and background.
 * Execute for surface, volume or displacement.
 * Evaluate one or more closures.
 * Release.
 */

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/bsdf.h"
#include "kernel/closure/emissive.h"

#include "kernel/svm/svm.h"

CCL_NAMESPACE_BEGIN

/* ShaderData setup from incoming ray */

#ifdef __OBJECT_MOTION__
ccl_device void shader_setup_object_transforms(KernelGlobals *kg, ShaderData *sd, float time)
{
  if (sd->object_flag & SD_OBJECT_MOTION) {
    sd->ob_tfm = object_fetch_transform_motion(kg, sd->object, time);
    sd->ob_itfm = transform_quick_inverse(sd->ob_tfm);
  }
  else {
    sd->ob_tfm = object_fetch_transform(kg, sd->object, OBJECT_TRANSFORM);
    sd->ob_itfm = object_fetch_transform(kg, sd->object, OBJECT_INVERSE_TRANSFORM);
  }
}
#endif

ccl_device_noinline void shader_setup_from_ray(KernelGlobals *kg,
                                               ShaderData *sd,
                                               const Intersection *isect,
                                               const Ray *ray)
{
  PROFILING_INIT(kg, PROFILING_SHADER_SETUP);

#ifdef __INSTANCING__
  sd->object = (isect->object == OBJECT_NONE) ? kernel_tex_fetch(__prim_object, isect->prim) :
                                                isect->object;
#endif
  sd->lamp = LAMP_NONE;

  sd->type = isect->type;
  sd->flag = 0;
  sd->object_flag = kernel_tex_fetch(__object_flag, sd->object);

  /* matrices and time */
#ifdef __OBJECT_MOTION__
  shader_setup_object_transforms(kg, sd, ray->time);
#endif
  sd->time = ray->time;

  sd->prim = kernel_tex_fetch(__prim_index, isect->prim);
  sd->ray_length = isect->t;

#ifdef __UV__
  sd->u = isect->u;
  sd->v = isect->v;
#endif

#ifdef __HAIR__
  if (sd->type & PRIMITIVE_ALL_CURVE) {
    /* curve */
    float4 curvedata = kernel_tex_fetch(__curves, sd->prim);

    sd->shader = __float_as_int(curvedata.z);
    sd->P = curve_refine(kg, sd, isect, ray);
  }
  else
#endif
      if (sd->type & PRIMITIVE_TRIANGLE) {
    /* static triangle */
    float3 Ng = triangle_normal(kg, sd);
    sd->shader = kernel_tex_fetch(__tri_shader, sd->prim);

    /* vectors */
    sd->P = triangle_refine(kg, sd, isect, ray);
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
    motion_triangle_shader_setup(kg, sd, isect, ray, false);
  }

  sd->I = -ray->D;

  sd->flag |= kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).flags;

#ifdef __INSTANCING__
  if (isect->object != OBJECT_NONE) {
    /* instance transform */
    object_normal_transform_auto(kg, sd, &sd->N);
    object_normal_transform_auto(kg, sd, &sd->Ng);
#  ifdef __DPDU__
    object_dir_transform_auto(kg, sd, &sd->dPdu);
    object_dir_transform_auto(kg, sd, &sd->dPdv);
#  endif
  }
#endif

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
  differential_transfer(&sd->dP, ray->dP, ray->D, ray->dD, sd->Ng, isect->t);
  differential_incoming(&sd->dI, ray->dD);
  differential_dudv(&sd->du, &sd->dv, sd->dPdu, sd->dPdv, sd->dP, sd->Ng);
#endif

  PROFILING_SHADER(sd->shader);
  PROFILING_OBJECT(sd->object);
}

/* ShaderData setup from BSSRDF scatter */

#ifdef __SUBSURFACE__
#  ifndef __KERNEL_CUDA__
ccl_device
#  else
ccl_device_inline
#  endif
    void
    shader_setup_from_subsurface(KernelGlobals *kg,
                                 ShaderData *sd,
                                 const Intersection *isect,
                                 const Ray *ray)
{
  PROFILING_INIT(kg, PROFILING_SHADER_SETUP);

  const bool backfacing = sd->flag & SD_BACKFACING;

  /* object, matrices, time, ray_length stay the same */
  sd->flag = 0;
  sd->object_flag = kernel_tex_fetch(__object_flag, sd->object);
  sd->prim = kernel_tex_fetch(__prim_index, isect->prim);
  sd->type = isect->type;

#  ifdef __UV__
  sd->u = isect->u;
  sd->v = isect->v;
#  endif

  /* fetch triangle data */
  if (sd->type == PRIMITIVE_TRIANGLE) {
    float3 Ng = triangle_normal(kg, sd);
    sd->shader = kernel_tex_fetch(__tri_shader, sd->prim);

    /* static triangle */
    sd->P = triangle_refine_local(kg, sd, isect, ray);
    sd->Ng = Ng;
    sd->N = Ng;

    if (sd->shader & SHADER_SMOOTH_NORMAL)
      sd->N = triangle_smooth_normal(kg, Ng, sd->prim, sd->u, sd->v);

#  ifdef __DPDU__
    /* dPdu/dPdv */
    triangle_dPdudv(kg, sd->prim, &sd->dPdu, &sd->dPdv);
#  endif
  }
  else {
    /* motion triangle */
    motion_triangle_shader_setup(kg, sd, isect, ray, true);
  }

  sd->flag |= kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).flags;

#  ifdef __INSTANCING__
  if (isect->object != OBJECT_NONE) {
    /* instance transform */
    object_normal_transform_auto(kg, sd, &sd->N);
    object_normal_transform_auto(kg, sd, &sd->Ng);
#    ifdef __DPDU__
    object_dir_transform_auto(kg, sd, &sd->dPdu);
    object_dir_transform_auto(kg, sd, &sd->dPdv);
#    endif
  }
#  endif

  /* backfacing test */
  if (backfacing) {
    sd->flag |= SD_BACKFACING;
    sd->Ng = -sd->Ng;
    sd->N = -sd->N;
#  ifdef __DPDU__
    sd->dPdu = -sd->dPdu;
    sd->dPdv = -sd->dPdv;
#  endif
  }

  /* should not get used in principle as the shading will only use a diffuse
   * BSDF, but the shader might still access it */
  sd->I = sd->N;

#  ifdef __RAY_DIFFERENTIALS__
  /* differentials */
  differential_dudv(&sd->du, &sd->dv, sd->dPdu, sd->dPdv, sd->dP, sd->Ng);
  /* don't modify dP and dI */
#  endif

  PROFILING_SHADER(sd->shader);
}
#endif

/* ShaderData setup from position sampled on mesh */

ccl_device_inline void shader_setup_from_sample(KernelGlobals *kg,
                                                ShaderData *sd,
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
  PROFILING_INIT(kg, PROFILING_SHADER_SETUP);

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
#ifdef __INSTANCING__
  sd->object = object;
#endif
  sd->lamp = LAMP_NONE;
  /* currently no access to bvh prim index for strand sd->prim*/
  sd->prim = prim;
#ifdef __UV__
  sd->u = u;
  sd->v = v;
#endif
  sd->time = time;
  sd->ray_length = t;

  sd->flag = kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).flags;
  sd->object_flag = 0;
  if (sd->object != OBJECT_NONE) {
    sd->object_flag |= kernel_tex_fetch(__object_flag, sd->object);

#ifdef __OBJECT_MOTION__
    shader_setup_object_transforms(kg, sd, time);
  }
  else if (lamp != LAMP_NONE) {
    sd->ob_tfm = lamp_fetch_transform(kg, lamp, false);
    sd->ob_itfm = lamp_fetch_transform(kg, lamp, true);
    sd->lamp = lamp;
#else
  }
  else if (lamp != LAMP_NONE) {
    sd->lamp = lamp;
#endif
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

#ifdef __INSTANCING__
      if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
        object_normal_transform_auto(kg, sd, &sd->N);
      }
#endif
    }

    /* dPdu/dPdv */
#ifdef __DPDU__
    triangle_dPdudv(kg, sd->prim, &sd->dPdu, &sd->dPdv);

#  ifdef __INSTANCING__
    if (!(sd->object_flag & SD_OBJECT_TRANSFORM_APPLIED)) {
      object_dir_transform_auto(kg, sd, &sd->dPdu);
      object_dir_transform_auto(kg, sd, &sd->dPdv);
    }
#  endif
#endif
  }
  else {
#ifdef __DPDU__
    sd->dPdu = make_float3(0.0f, 0.0f, 0.0f);
    sd->dPdv = make_float3(0.0f, 0.0f, 0.0f);
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

  PROFILING_SHADER(sd->shader);
  PROFILING_OBJECT(sd->object);
}

/* ShaderData setup for displacement */

ccl_device void shader_setup_from_displace(
    KernelGlobals *kg, ShaderData *sd, int object, int prim, float u, float v)
{
  float3 P, Ng, I = make_float3(0.0f, 0.0f, 0.0f);
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

ccl_device_inline void shader_setup_from_background(KernelGlobals *kg,
                                                    ShaderData *sd,
                                                    const Ray *ray)
{
  PROFILING_INIT(kg, PROFILING_SHADER_SETUP);

  /* vectors */
  sd->P = ray->D;
  sd->N = -ray->D;
  sd->Ng = -ray->D;
  sd->I = -ray->D;
  sd->shader = kernel_data.background.surface_shader;
  sd->flag = kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).flags;
  sd->object_flag = 0;
  sd->time = ray->time;
  sd->ray_length = 0.0f;

#ifdef __INSTANCING__
  sd->object = OBJECT_NONE;
#endif
  sd->lamp = LAMP_NONE;
  sd->prim = PRIM_NONE;
#ifdef __UV__
  sd->u = 0.0f;
  sd->v = 0.0f;
#endif

#ifdef __DPDU__
  /* dPdu/dPdv */
  sd->dPdu = make_float3(0.0f, 0.0f, 0.0f);
  sd->dPdv = make_float3(0.0f, 0.0f, 0.0f);
#endif

#ifdef __RAY_DIFFERENTIALS__
  /* differentials */
  sd->dP = ray->dD;
  differential_incoming(&sd->dI, sd->dP);
  sd->du = differential_zero();
  sd->dv = differential_zero();
#endif

  /* for NDC coordinates */
  sd->ray_P = ray->P;

  PROFILING_SHADER(sd->shader);
  PROFILING_OBJECT(sd->object);
}

/* ShaderData setup from point inside volume */

#ifdef __VOLUME__
ccl_device_inline void shader_setup_from_volume(KernelGlobals *kg, ShaderData *sd, const Ray *ray)
{
  PROFILING_INIT(kg, PROFILING_SHADER_SETUP);

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

#  ifdef __INSTANCING__
  sd->object = OBJECT_NONE; /* todo: fill this for texture coordinates */
#  endif
  sd->lamp = LAMP_NONE;
  sd->prim = PRIM_NONE;
  sd->type = PRIMITIVE_NONE;

#  ifdef __UV__
  sd->u = 0.0f;
  sd->v = 0.0f;
#  endif

#  ifdef __DPDU__
  /* dPdu/dPdv */
  sd->dPdu = make_float3(0.0f, 0.0f, 0.0f);
  sd->dPdv = make_float3(0.0f, 0.0f, 0.0f);
#  endif

#  ifdef __RAY_DIFFERENTIALS__
  /* differentials */
  sd->dP = ray->dD;
  differential_incoming(&sd->dI, sd->dP);
  sd->du = differential_zero();
  sd->dv = differential_zero();
#  endif

  /* for NDC coordinates */
  sd->ray_P = ray->P;
  sd->ray_dP = ray->dP;

  PROFILING_SHADER(sd->shader);
  PROFILING_OBJECT(sd->object);
}
#endif /* __VOLUME__ */

/* Merging */

#if defined(__BRANCHED_PATH__) || defined(__VOLUME__)
ccl_device_inline void shader_merge_closures(ShaderData *sd)
{
  /* merge identical closures, better when we sample a single closure at a time */
  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sci = &sd->closure[i];

    for (int j = i + 1; j < sd->num_closure; j++) {
      ShaderClosure *scj = &sd->closure[j];

      if (sci->type != scj->type)
        continue;
      if (!bsdf_merge(sci, scj))
        continue;

      sci->weight += scj->weight;
      sci->sample_weight += scj->sample_weight;

      int size = sd->num_closure - (j + 1);
      if (size > 0) {
        for (int k = 0; k < size; k++) {
          scj[k] = scj[k + 1];
        }
      }

      sd->num_closure--;
      kernel_assert(sd->num_closure >= 0);
      j--;
    }
  }
}
#endif /* __BRANCHED_PATH__ || __VOLUME__ */

/* Defensive sampling. */

ccl_device_inline void shader_prepare_closures(ShaderData *sd, ccl_addr_space PathState *state)
{
  /* We can likely also do defensive sampling at deeper bounces, particularly
   * for cases like a perfect mirror but possibly also others. This will need
   * a good heuristic. */
  if (state->bounce + state->transparent_bounce == 0 && sd->num_closure > 1) {
    float sum = 0.0f;

    for (int i = 0; i < sd->num_closure; i++) {
      ShaderClosure *sc = &sd->closure[i];
      if (CLOSURE_IS_BSDF_OR_BSSRDF(sc->type)) {
        sum += sc->sample_weight;
      }
    }

    for (int i = 0; i < sd->num_closure; i++) {
      ShaderClosure *sc = &sd->closure[i];
      if (CLOSURE_IS_BSDF_OR_BSSRDF(sc->type)) {
        sc->sample_weight = max(sc->sample_weight, 0.125f * sum);
      }
    }
  }
}

/* BSDF */

ccl_device_inline void _shader_bsdf_multi_eval(KernelGlobals *kg,
                                               ShaderData *sd,
                                               const float3 omega_in,
                                               float *pdf,
                                               const ShaderClosure *skip_sc,
                                               BsdfEval *result_eval,
                                               float sum_pdf,
                                               float sum_sample_weight)
{
  /* this is the veach one-sample model with balance heuristic, some pdf
   * factors drop out when using balance heuristic weighting */
  for (int i = 0; i < sd->num_closure; i++) {
    const ShaderClosure *sc = &sd->closure[i];

    if (sc != skip_sc && CLOSURE_IS_BSDF(sc->type)) {
      float bsdf_pdf = 0.0f;
      float3 eval = bsdf_eval(kg, sd, sc, omega_in, &bsdf_pdf);

      if (bsdf_pdf != 0.0f) {
        bsdf_eval_accum(result_eval, sc->type, eval * sc->weight, 1.0f);
        sum_pdf += bsdf_pdf * sc->sample_weight;
      }

      sum_sample_weight += sc->sample_weight;
    }
  }

  *pdf = (sum_sample_weight > 0.0f) ? sum_pdf / sum_sample_weight : 0.0f;
}

#ifdef __BRANCHED_PATH__
ccl_device_inline void _shader_bsdf_multi_eval_branched(KernelGlobals *kg,
                                                        ShaderData *sd,
                                                        const float3 omega_in,
                                                        BsdfEval *result_eval,
                                                        float light_pdf,
                                                        bool use_mis)
{
  for (int i = 0; i < sd->num_closure; i++) {
    const ShaderClosure *sc = &sd->closure[i];
    if (CLOSURE_IS_BSDF(sc->type)) {
      float bsdf_pdf = 0.0f;
      float3 eval = bsdf_eval(kg, sd, sc, omega_in, &bsdf_pdf);
      if (bsdf_pdf != 0.0f) {
        float mis_weight = use_mis ? power_heuristic(light_pdf, bsdf_pdf) : 1.0f;
        bsdf_eval_accum(result_eval, sc->type, eval * sc->weight, mis_weight);
      }
    }
  }
}
#endif /* __BRANCHED_PATH__ */

#ifndef __KERNEL_CUDA__
ccl_device
#else
ccl_device_inline
#endif
    void
    shader_bsdf_eval(KernelGlobals *kg,
                     ShaderData *sd,
                     const float3 omega_in,
                     BsdfEval *eval,
                     float light_pdf,
                     bool use_mis)
{
  PROFILING_INIT(kg, PROFILING_CLOSURE_EVAL);

  bsdf_eval_init(
      eval, NBUILTIN_CLOSURES, make_float3(0.0f, 0.0f, 0.0f), kernel_data.film.use_light_pass);

#ifdef __BRANCHED_PATH__
  if (kernel_data.integrator.branched)
    _shader_bsdf_multi_eval_branched(kg, sd, omega_in, eval, light_pdf, use_mis);
  else
#endif
  {
    float pdf;
    _shader_bsdf_multi_eval(kg, sd, omega_in, &pdf, NULL, eval, 0.0f, 0.0f);
    if (use_mis) {
      float weight = power_heuristic(light_pdf, pdf);
      bsdf_eval_mis(eval, weight);
    }
  }
}

ccl_device_inline const ShaderClosure *shader_bsdf_pick(ShaderData *sd, float *randu)
{
  /* Note the sampling here must match shader_bssrdf_pick,
   * since we reuse the same random number. */
  int sampled = 0;

  if (sd->num_closure > 1) {
    /* Pick a BSDF or based on sample weights. */
    float sum = 0.0f;

    for (int i = 0; i < sd->num_closure; i++) {
      const ShaderClosure *sc = &sd->closure[i];

      if (CLOSURE_IS_BSDF_OR_BSSRDF(sc->type)) {
        sum += sc->sample_weight;
      }
    }

    float r = (*randu) * sum;
    float partial_sum = 0.0f;

    for (int i = 0; i < sd->num_closure; i++) {
      const ShaderClosure *sc = &sd->closure[i];

      if (CLOSURE_IS_BSDF_OR_BSSRDF(sc->type)) {
        float next_sum = partial_sum + sc->sample_weight;

        if (r < next_sum) {
          sampled = i;

          /* Rescale to reuse for direction sample, to better
           * preserve stratifaction. */
          *randu = (r - partial_sum) / sc->sample_weight;
          break;
        }

        partial_sum = next_sum;
      }
    }
  }

  const ShaderClosure *sc = &sd->closure[sampled];
  return CLOSURE_IS_BSDF(sc->type) ? sc : NULL;
}

ccl_device_inline const ShaderClosure *shader_bssrdf_pick(ShaderData *sd,
                                                          ccl_addr_space float3 *throughput,
                                                          float *randu)
{
  /* Note the sampling here must match shader_bsdf_pick,
   * since we reuse the same random number. */
  int sampled = 0;

  if (sd->num_closure > 1) {
    /* Pick a BSDF or BSSRDF or based on sample weights. */
    float sum_bsdf = 0.0f;
    float sum_bssrdf = 0.0f;

    for (int i = 0; i < sd->num_closure; i++) {
      const ShaderClosure *sc = &sd->closure[i];

      if (CLOSURE_IS_BSDF(sc->type)) {
        sum_bsdf += sc->sample_weight;
      }
      else if (CLOSURE_IS_BSSRDF(sc->type)) {
        sum_bssrdf += sc->sample_weight;
      }
    }

    float r = (*randu) * (sum_bsdf + sum_bssrdf);
    float partial_sum = 0.0f;

    for (int i = 0; i < sd->num_closure; i++) {
      const ShaderClosure *sc = &sd->closure[i];

      if (CLOSURE_IS_BSDF_OR_BSSRDF(sc->type)) {
        float next_sum = partial_sum + sc->sample_weight;

        if (r < next_sum) {
          if (CLOSURE_IS_BSDF(sc->type)) {
            *throughput *= (sum_bsdf + sum_bssrdf) / sum_bsdf;
            return NULL;
          }
          else {
            *throughput *= (sum_bsdf + sum_bssrdf) / sum_bssrdf;
            sampled = i;

            /* Rescale to reuse for direction sample, to better
             * preserve stratifaction. */
            *randu = (r - partial_sum) / sc->sample_weight;
            break;
          }
        }

        partial_sum = next_sum;
      }
    }
  }

  const ShaderClosure *sc = &sd->closure[sampled];
  return CLOSURE_IS_BSSRDF(sc->type) ? sc : NULL;
}

ccl_device_inline int shader_bsdf_sample(KernelGlobals *kg,
                                         ShaderData *sd,
                                         float randu,
                                         float randv,
                                         BsdfEval *bsdf_eval,
                                         float3 *omega_in,
                                         differential3 *domega_in,
                                         float *pdf)
{
  PROFILING_INIT(kg, PROFILING_CLOSURE_SAMPLE);

  const ShaderClosure *sc = shader_bsdf_pick(sd, &randu);
  if (sc == NULL) {
    *pdf = 0.0f;
    return LABEL_NONE;
  }

  /* BSSRDF should already have been handled elsewhere. */
  kernel_assert(CLOSURE_IS_BSDF(sc->type));

  int label;
  float3 eval;

  *pdf = 0.0f;
  label = bsdf_sample(kg, sd, sc, randu, randv, &eval, omega_in, domega_in, pdf);

  if (*pdf != 0.0f) {
    bsdf_eval_init(bsdf_eval, sc->type, eval * sc->weight, kernel_data.film.use_light_pass);

    if (sd->num_closure > 1) {
      float sweight = sc->sample_weight;
      _shader_bsdf_multi_eval(kg, sd, *omega_in, pdf, sc, bsdf_eval, *pdf * sweight, sweight);
    }
  }

  return label;
}

ccl_device int shader_bsdf_sample_closure(KernelGlobals *kg,
                                          ShaderData *sd,
                                          const ShaderClosure *sc,
                                          float randu,
                                          float randv,
                                          BsdfEval *bsdf_eval,
                                          float3 *omega_in,
                                          differential3 *domega_in,
                                          float *pdf)
{
  PROFILING_INIT(kg, PROFILING_CLOSURE_SAMPLE);

  int label;
  float3 eval;

  *pdf = 0.0f;
  label = bsdf_sample(kg, sd, sc, randu, randv, &eval, omega_in, domega_in, pdf);

  if (*pdf != 0.0f)
    bsdf_eval_init(bsdf_eval, sc->type, eval * sc->weight, kernel_data.film.use_light_pass);

  return label;
}

ccl_device float shader_bsdf_average_roughness(ShaderData *sd)
{
  float roughness = 0.0f;
  float sum_weight = 0.0f;

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_BSDF(sc->type)) {
      /* sqrt once to undo the squaring from multiplying roughness on the
       * two axes, and once for the squared roughness convention. */
      float weight = fabsf(average(sc->weight));
      roughness += weight * sqrtf(safe_sqrtf(bsdf_get_roughness_squared(sc)));
      sum_weight += weight;
    }
  }

  return (sum_weight > 0.0f) ? roughness / sum_weight : 0.0f;
}

ccl_device void shader_bsdf_blur(KernelGlobals *kg, ShaderData *sd, float roughness)
{
  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_BSDF(sc->type))
      bsdf_blur(kg, sc, roughness);
  }
}

ccl_device float3 shader_bsdf_transparency(KernelGlobals *kg, const ShaderData *sd)
{
  if (sd->flag & SD_HAS_ONLY_VOLUME) {
    return make_float3(1.0f, 1.0f, 1.0f);
  }
  else if (sd->flag & SD_TRANSPARENT) {
    return sd->closure_transparent_extinction;
  }
  else {
    return make_float3(0.0f, 0.0f, 0.0f);
  }
}

ccl_device void shader_bsdf_disable_transparency(KernelGlobals *kg, ShaderData *sd)
{
  if (sd->flag & SD_TRANSPARENT) {
    for (int i = 0; i < sd->num_closure; i++) {
      ShaderClosure *sc = &sd->closure[i];

      if (sc->type == CLOSURE_BSDF_TRANSPARENT_ID) {
        sc->sample_weight = 0.0f;
        sc->weight = make_float3(0.0f, 0.0f, 0.0f);
      }
    }

    sd->flag &= ~SD_TRANSPARENT;
  }
}

ccl_device float3 shader_bsdf_alpha(KernelGlobals *kg, ShaderData *sd)
{
  float3 alpha = make_float3(1.0f, 1.0f, 1.0f) - shader_bsdf_transparency(kg, sd);

  alpha = max(alpha, make_float3(0.0f, 0.0f, 0.0f));
  alpha = min(alpha, make_float3(1.0f, 1.0f, 1.0f));

  return alpha;
}

ccl_device float3 shader_bsdf_diffuse(KernelGlobals *kg, ShaderData *sd)
{
  float3 eval = make_float3(0.0f, 0.0f, 0.0f);

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_BSDF_DIFFUSE(sc->type))
      eval += sc->weight;
  }

  return eval;
}

ccl_device float3 shader_bsdf_glossy(KernelGlobals *kg, ShaderData *sd)
{
  float3 eval = make_float3(0.0f, 0.0f, 0.0f);

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_BSDF_GLOSSY(sc->type))
      eval += sc->weight;
  }

  return eval;
}

ccl_device float3 shader_bsdf_transmission(KernelGlobals *kg, ShaderData *sd)
{
  float3 eval = make_float3(0.0f, 0.0f, 0.0f);

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_BSDF_TRANSMISSION(sc->type))
      eval += sc->weight;
  }

  return eval;
}

ccl_device float3 shader_bsdf_subsurface(KernelGlobals *kg, ShaderData *sd)
{
  float3 eval = make_float3(0.0f, 0.0f, 0.0f);

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_BSSRDF(sc->type) || CLOSURE_IS_BSDF_BSSRDF(sc->type))
      eval += sc->weight;
  }

  return eval;
}

ccl_device float3 shader_bsdf_average_normal(KernelGlobals *kg, ShaderData *sd)
{
  float3 N = make_float3(0.0f, 0.0f, 0.0f);

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];
    if (CLOSURE_IS_BSDF_OR_BSSRDF(sc->type))
      N += sc->N * fabsf(average(sc->weight));
  }

  return (is_zero(N)) ? sd->N : normalize(N);
}

ccl_device float3 shader_bsdf_ao(KernelGlobals *kg, ShaderData *sd, float ao_factor, float3 *N_)
{
  float3 eval = make_float3(0.0f, 0.0f, 0.0f);
  float3 N = make_float3(0.0f, 0.0f, 0.0f);

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_BSDF_DIFFUSE(sc->type)) {
      const DiffuseBsdf *bsdf = (const DiffuseBsdf *)sc;
      eval += sc->weight * ao_factor;
      N += bsdf->N * fabsf(average(sc->weight));
    }
  }

  *N_ = (is_zero(N)) ? sd->N : normalize(N);
  return eval;
}

#ifdef __SUBSURFACE__
ccl_device float3 shader_bssrdf_sum(ShaderData *sd, float3 *N_, float *texture_blur_)
{
  float3 eval = make_float3(0.0f, 0.0f, 0.0f);
  float3 N = make_float3(0.0f, 0.0f, 0.0f);
  float texture_blur = 0.0f, weight_sum = 0.0f;

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_BSSRDF(sc->type)) {
      const Bssrdf *bssrdf = (const Bssrdf *)sc;
      float avg_weight = fabsf(average(sc->weight));

      N += bssrdf->N * avg_weight;
      eval += sc->weight;
      texture_blur += bssrdf->texture_blur * avg_weight;
      weight_sum += avg_weight;
    }
  }

  if (N_)
    *N_ = (is_zero(N)) ? sd->N : normalize(N);

  if (texture_blur_)
    *texture_blur_ = safe_divide(texture_blur, weight_sum);

  return eval;
}
#endif /* __SUBSURFACE__ */

/* Constant emission optimization */

ccl_device bool shader_constant_emission_eval(KernelGlobals *kg, int shader, float3 *eval)
{
  int shader_index = shader & SHADER_MASK;
  int shader_flag = kernel_tex_fetch(__shaders, shader_index).flags;

  if (shader_flag & SD_HAS_CONSTANT_EMISSION) {
    *eval = make_float3(kernel_tex_fetch(__shaders, shader_index).constant_emission[0],
                        kernel_tex_fetch(__shaders, shader_index).constant_emission[1],
                        kernel_tex_fetch(__shaders, shader_index).constant_emission[2]);

    return true;
  }

  return false;
}

/* Background */

ccl_device float3 shader_background_eval(ShaderData *sd)
{
  if (sd->flag & SD_EMISSION) {
    return sd->closure_emission_background;
  }
  else {
    return make_float3(0.0f, 0.0f, 0.0f);
  }
}

/* Emission */

ccl_device float3 shader_emissive_eval(ShaderData *sd)
{
  if (sd->flag & SD_EMISSION) {
    return emissive_simple_eval(sd->Ng, sd->I) * sd->closure_emission_background;
  }
  else {
    return make_float3(0.0f, 0.0f, 0.0f);
  }
}

/* Holdout */

ccl_device float3 shader_holdout_eval(KernelGlobals *kg, ShaderData *sd)
{
  float3 weight = make_float3(0.0f, 0.0f, 0.0f);

  for (int i = 0; i < sd->num_closure; i++) {
    ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_HOLDOUT(sc->type))
      weight += sc->weight;
  }

  return weight;
}

/* Surface Evaluation */

ccl_device void shader_eval_surface(KernelGlobals *kg,
                                    ShaderData *sd,
                                    ccl_addr_space PathState *state,
                                    int path_flag)
{
  PROFILING_INIT(kg, PROFILING_SHADER_EVAL);

  /* If path is being terminated, we are tracing a shadow ray or evaluating
   * emission, then we don't need to store closures. The emission and shadow
   * shader data also do not have a closure array to save GPU memory. */
  int max_closures;
  if (path_flag & (PATH_RAY_TERMINATE | PATH_RAY_SHADOW | PATH_RAY_EMISSION)) {
    max_closures = 0;
  }
  else {
    max_closures = kernel_data.integrator.max_closures;
  }

  sd->num_closure = 0;
  sd->num_closure_left = max_closures;

#ifdef __OSL__
  if (kg->osl) {
    if (sd->object == OBJECT_NONE && sd->lamp == LAMP_NONE) {
      OSLShader::eval_background(kg, sd, state, path_flag);
    }
    else {
      OSLShader::eval_surface(kg, sd, state, path_flag);
    }
  }
  else
#endif
  {
#ifdef __SVM__
    svm_eval_nodes(kg, sd, state, SHADER_TYPE_SURFACE, path_flag);
#else
    if (sd->object == OBJECT_NONE) {
      sd->closure_emission_background = make_float3(0.8f, 0.8f, 0.8f);
      sd->flag |= SD_EMISSION;
    }
    else {
      DiffuseBsdf *bsdf = (DiffuseBsdf *)bsdf_alloc(
          sd, sizeof(DiffuseBsdf), make_float3(0.8f, 0.8f, 0.8f));
      if (bsdf != NULL) {
        bsdf->N = sd->N;
        sd->flag |= bsdf_diffuse_setup(bsdf);
      }
    }
#endif
  }

  if (sd->flag & SD_BSDF_NEEDS_LCG) {
    sd->lcg_state = lcg_state_init_addrspace(state, 0xb4bc3953);
  }
}

/* Volume */

#ifdef __VOLUME__

ccl_device_inline void _shader_volume_phase_multi_eval(const ShaderData *sd,
                                                       const float3 omega_in,
                                                       float *pdf,
                                                       int skip_phase,
                                                       BsdfEval *result_eval,
                                                       float sum_pdf,
                                                       float sum_sample_weight)
{
  for (int i = 0; i < sd->num_closure; i++) {
    if (i == skip_phase)
      continue;

    const ShaderClosure *sc = &sd->closure[i];

    if (CLOSURE_IS_PHASE(sc->type)) {
      float phase_pdf = 0.0f;
      float3 eval = volume_phase_eval(sd, sc, omega_in, &phase_pdf);

      if (phase_pdf != 0.0f) {
        bsdf_eval_accum(result_eval, sc->type, eval, 1.0f);
        sum_pdf += phase_pdf * sc->sample_weight;
      }

      sum_sample_weight += sc->sample_weight;
    }
  }

  *pdf = (sum_sample_weight > 0.0f) ? sum_pdf / sum_sample_weight : 0.0f;
}

ccl_device void shader_volume_phase_eval(
    KernelGlobals *kg, const ShaderData *sd, const float3 omega_in, BsdfEval *eval, float *pdf)
{
  PROFILING_INIT(kg, PROFILING_CLOSURE_VOLUME_EVAL);

  bsdf_eval_init(
      eval, NBUILTIN_CLOSURES, make_float3(0.0f, 0.0f, 0.0f), kernel_data.film.use_light_pass);

  _shader_volume_phase_multi_eval(sd, omega_in, pdf, -1, eval, 0.0f, 0.0f);
}

ccl_device int shader_volume_phase_sample(KernelGlobals *kg,
                                          const ShaderData *sd,
                                          float randu,
                                          float randv,
                                          BsdfEval *phase_eval,
                                          float3 *omega_in,
                                          differential3 *domega_in,
                                          float *pdf)
{
  PROFILING_INIT(kg, PROFILING_CLOSURE_VOLUME_SAMPLE);

  int sampled = 0;

  if (sd->num_closure > 1) {
    /* pick a phase closure based on sample weights */
    float sum = 0.0f;

    for (sampled = 0; sampled < sd->num_closure; sampled++) {
      const ShaderClosure *sc = &sd->closure[sampled];

      if (CLOSURE_IS_PHASE(sc->type))
        sum += sc->sample_weight;
    }

    float r = randu * sum;
    float partial_sum = 0.0f;

    for (sampled = 0; sampled < sd->num_closure; sampled++) {
      const ShaderClosure *sc = &sd->closure[sampled];

      if (CLOSURE_IS_PHASE(sc->type)) {
        float next_sum = partial_sum + sc->sample_weight;

        if (r <= next_sum) {
          /* Rescale to reuse for BSDF direction sample. */
          randu = (r - partial_sum) / sc->sample_weight;
          break;
        }

        partial_sum = next_sum;
      }
    }

    if (sampled == sd->num_closure) {
      *pdf = 0.0f;
      return LABEL_NONE;
    }
  }

  /* todo: this isn't quite correct, we don't weight anisotropy properly
   * depending on color channels, even if this is perhaps not a common case */
  const ShaderClosure *sc = &sd->closure[sampled];
  int label;
  float3 eval;

  *pdf = 0.0f;
  label = volume_phase_sample(sd, sc, randu, randv, &eval, omega_in, domega_in, pdf);

  if (*pdf != 0.0f) {
    bsdf_eval_init(phase_eval, sc->type, eval, kernel_data.film.use_light_pass);
  }

  return label;
}

ccl_device int shader_phase_sample_closure(KernelGlobals *kg,
                                           const ShaderData *sd,
                                           const ShaderClosure *sc,
                                           float randu,
                                           float randv,
                                           BsdfEval *phase_eval,
                                           float3 *omega_in,
                                           differential3 *domega_in,
                                           float *pdf)
{
  PROFILING_INIT(kg, PROFILING_CLOSURE_VOLUME_SAMPLE);

  int label;
  float3 eval;

  *pdf = 0.0f;
  label = volume_phase_sample(sd, sc, randu, randv, &eval, omega_in, domega_in, pdf);

  if (*pdf != 0.0f)
    bsdf_eval_init(phase_eval, sc->type, eval, kernel_data.film.use_light_pass);

  return label;
}

/* Volume Evaluation */

ccl_device_inline void shader_eval_volume(KernelGlobals *kg,
                                          ShaderData *sd,
                                          ccl_addr_space PathState *state,
                                          ccl_addr_space VolumeStack *stack,
                                          int path_flag)
{
  /* If path is being terminated, we are tracing a shadow ray or evaluating
   * emission, then we don't need to store closures. The emission and shadow
   * shader data also do not have a closure array to save GPU memory. */
  int max_closures;
  if (path_flag & (PATH_RAY_TERMINATE | PATH_RAY_SHADOW | PATH_RAY_EMISSION)) {
    max_closures = 0;
  }
  else {
    max_closures = kernel_data.integrator.max_closures;
  }

  /* reset closures once at the start, we will be accumulating the closures
   * for all volumes in the stack into a single array of closures */
  sd->num_closure = 0;
  sd->num_closure_left = max_closures;
  sd->flag = 0;
  sd->object_flag = 0;

  for (int i = 0; stack[i].shader != SHADER_NONE; i++) {
    /* setup shaderdata from stack. it's mostly setup already in
     * shader_setup_from_volume, this switching should be quick */
    sd->object = stack[i].object;
    sd->lamp = LAMP_NONE;
    sd->shader = stack[i].shader;

    sd->flag &= ~SD_SHADER_FLAGS;
    sd->flag |= kernel_tex_fetch(__shaders, (sd->shader & SHADER_MASK)).flags;
    sd->object_flag &= ~SD_OBJECT_FLAGS;

    if (sd->object != OBJECT_NONE) {
      sd->object_flag |= kernel_tex_fetch(__object_flag, sd->object);

#  ifdef __OBJECT_MOTION__
      /* todo: this is inefficient for motion blur, we should be
       * caching matrices instead of recomputing them each step */
      shader_setup_object_transforms(kg, sd, sd->time);
#  endif
    }

    /* evaluate shader */
#  ifdef __SVM__
#    ifdef __OSL__
    if (kg->osl) {
      OSLShader::eval_volume(kg, sd, state, path_flag);
    }
    else
#    endif
    {
      svm_eval_nodes(kg, sd, state, SHADER_TYPE_VOLUME, path_flag);
    }
#  endif

    /* merge closures to avoid exceeding number of closures limit */
    if (i > 0)
      shader_merge_closures(sd);
  }
}

#endif /* __VOLUME__ */

/* Displacement Evaluation */

ccl_device void shader_eval_displacement(KernelGlobals *kg,
                                         ShaderData *sd,
                                         ccl_addr_space PathState *state)
{
  sd->num_closure = 0;
  sd->num_closure_left = 0;

  /* this will modify sd->P */
#ifdef __SVM__
#  ifdef __OSL__
  if (kg->osl)
    OSLShader::eval_displacement(kg, sd, state);
  else
#  endif
  {
    svm_eval_nodes(kg, sd, state, SHADER_TYPE_DISPLACEMENT, 0);
  }
#endif
}

/* Transparent Shadows */

#ifdef __TRANSPARENT_SHADOWS__
ccl_device bool shader_transparent_shadow(KernelGlobals *kg, Intersection *isect)
{
  int prim = kernel_tex_fetch(__prim_index, isect->prim);
  int shader = 0;

#  ifdef __HAIR__
  if (kernel_tex_fetch(__prim_type, isect->prim) & PRIMITIVE_ALL_TRIANGLE) {
#  endif
    shader = kernel_tex_fetch(__tri_shader, prim);
#  ifdef __HAIR__
  }
  else {
    float4 str = kernel_tex_fetch(__curves, prim);
    shader = __float_as_int(str.z);
  }
#  endif
  int flag = kernel_tex_fetch(__shaders, (shader & SHADER_MASK)).flags;

  return (flag & SD_HAS_TRANSPARENT_SHADOW) != 0;
}
#endif /* __TRANSPARENT_SHADOWS__ */

ccl_device float shader_cryptomatte_id(KernelGlobals *kg, int shader)
{
  return kernel_tex_fetch(__shaders, (shader & SHADER_MASK)).cryptomatte_id;
}

CCL_NAMESPACE_END
