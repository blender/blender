/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

/* Volume shader evaluation and sampling. */

#pragma once

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/emissive.h"

#ifdef __SVM__
#  include "kernel/svm/svm.h"
#endif
#ifdef __OSL__
#  include "kernel/osl/osl.h"
#endif

CCL_NAMESPACE_BEGIN

#ifdef __VOLUME__

/* Merging */
ccl_device_inline void volume_shader_merge_closures(ccl_private ShaderData *sd)
{
  /* Merge identical closures to save closure space with stacked volumes. */
  for (int i = 0; i < sd->num_closure; i++) {
    ccl_private ShaderClosure *sci = &sd->closure[i];

    if (sci->type != CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID) {
      continue;
    }

    for (int j = i + 1; j < sd->num_closure; j++) {
      ccl_private ShaderClosure *scj = &sd->closure[j];
      if (sci->type != scj->type) {
        continue;
      }

      ccl_private const HenyeyGreensteinVolume *hgi = (ccl_private const HenyeyGreensteinVolume *)
          sci;
      ccl_private const HenyeyGreensteinVolume *hgj = (ccl_private const HenyeyGreensteinVolume *)
          scj;
      if (!(hgi->g == hgj->g)) {
        continue;
      }

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

ccl_device_inline void volume_shader_copy_phases(ccl_private ShaderVolumePhases *ccl_restrict
                                                     phases,
                                                 ccl_private const ShaderData *ccl_restrict sd)
{
  phases->num_closure = 0;

  for (int i = 0; i < sd->num_closure; i++) {
    ccl_private const ShaderClosure *from_sc = &sd->closure[i];
    ccl_private const HenyeyGreensteinVolume *from_hg =
        (ccl_private const HenyeyGreensteinVolume *)from_sc;

    if (from_sc->type == CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID) {
      ccl_private ShaderVolumeClosure *to_sc = &phases->closure[phases->num_closure];

      to_sc->weight = from_sc->weight;
      to_sc->sample_weight = from_sc->sample_weight;
      to_sc->g = from_hg->g;
      phases->num_closure++;
      if (phases->num_closure >= MAX_VOLUME_CLOSURE) {
        break;
      }
    }
  }
}

ccl_device_inline float _volume_shader_phase_eval_mis(ccl_private const ShaderData *sd,
                                                      ccl_private const ShaderVolumePhases *phases,
                                                      const float3 omega_in,
                                                      int skip_phase,
                                                      ccl_private BsdfEval *result_eval,
                                                      float sum_pdf,
                                                      float sum_sample_weight)
{
  for (int i = 0; i < phases->num_closure; i++) {
    if (i == skip_phase)
      continue;

    ccl_private const ShaderVolumeClosure *svc = &phases->closure[i];
    float phase_pdf = 0.0f;
    Spectrum eval = volume_phase_eval(sd, svc, omega_in, &phase_pdf);

    if (phase_pdf != 0.0f) {
      bsdf_eval_accum(result_eval, CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID, eval);
      sum_pdf += phase_pdf * svc->sample_weight;
    }

    sum_sample_weight += svc->sample_weight;
  }

  return (sum_sample_weight > 0.0f) ? sum_pdf / sum_sample_weight : 0.0f;
}

ccl_device float volume_shader_phase_eval(KernelGlobals kg,
                                          ccl_private const ShaderData *sd,
                                          ccl_private const ShaderVolumePhases *phases,
                                          const float3 omega_in,
                                          ccl_private BsdfEval *phase_eval)
{
  bsdf_eval_init(phase_eval, CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID, zero_spectrum());

  return _volume_shader_phase_eval_mis(sd, phases, omega_in, -1, phase_eval, 0.0f, 0.0f);
}

ccl_device int volume_shader_phase_sample(KernelGlobals kg,
                                          ccl_private const ShaderData *sd,
                                          ccl_private const ShaderVolumePhases *phases,
                                          float2 rand_phase,
                                          ccl_private BsdfEval *phase_eval,
                                          ccl_private float3 *omega_in,
                                          ccl_private float *pdf)
{
  int sampled = 0;

  if (phases->num_closure > 1) {
    /* pick a phase closure based on sample weights */
    float sum = 0.0f;

    for (sampled = 0; sampled < phases->num_closure; sampled++) {
      ccl_private const ShaderVolumeClosure *svc = &phases->closure[sampled];
      sum += svc->sample_weight;
    }

    float r = rand_phase.x * sum;
    float partial_sum = 0.0f;

    for (sampled = 0; sampled < phases->num_closure; sampled++) {
      ccl_private const ShaderVolumeClosure *svc = &phases->closure[sampled];
      float next_sum = partial_sum + svc->sample_weight;

      if (r <= next_sum) {
        /* Rescale to reuse for BSDF direction sample. */
        rand_phase.x = (r - partial_sum) / svc->sample_weight;
        break;
      }

      partial_sum = next_sum;
    }

    if (sampled == phases->num_closure) {
      *pdf = 0.0f;
      return LABEL_NONE;
    }
  }

  /* todo: this isn't quite correct, we don't weight anisotropy properly
   * depending on color channels, even if this is perhaps not a common case */
  ccl_private const ShaderVolumeClosure *svc = &phases->closure[sampled];
  int label;
  Spectrum eval = zero_spectrum();

  *pdf = 0.0f;
  label = volume_phase_sample(sd, svc, rand_phase.x, rand_phase.y, &eval, omega_in, pdf);

  if (*pdf != 0.0f) {
    bsdf_eval_init(phase_eval, CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID, eval);
  }

  return label;
}

ccl_device int volume_shader_phase_sample_closure(KernelGlobals kg,
                                                  ccl_private const ShaderData *sd,
                                                  ccl_private const ShaderVolumeClosure *sc,
                                                  const float2 rand_phase,
                                                  ccl_private BsdfEval *phase_eval,
                                                  ccl_private float3 *omega_in,
                                                  ccl_private float *pdf)
{
  int label;
  Spectrum eval = zero_spectrum();

  *pdf = 0.0f;
  label = volume_phase_sample(sd, sc, rand_phase.x, rand_phase.y, &eval, omega_in, pdf);

  if (*pdf != 0.0f)
    bsdf_eval_init(phase_eval, CLOSURE_VOLUME_HENYEY_GREENSTEIN_ID, eval);

  return label;
}

/* Motion Blur */

#  ifdef __OBJECT_MOTION__
ccl_device_inline void volume_shader_motion_blur(KernelGlobals kg,
                                                 ccl_private ShaderData *ccl_restrict sd)
{
  if ((sd->object_flag & SD_OBJECT_HAS_VOLUME_MOTION) == 0) {
    return;
  }

  AttributeDescriptor v_desc = find_attribute(kg, sd, ATTR_STD_VOLUME_VELOCITY);
  kernel_assert(v_desc.offset != ATTR_STD_NOT_FOUND);

  const float3 P = sd->P;
  const float velocity_scale = kernel_data_fetch(objects, sd->object).velocity_scale;
  const float time_offset = kernel_data.cam.motion_position == MOTION_POSITION_CENTER ? 0.5f :
                                                                                        0.0f;
  const float time = kernel_data.cam.motion_position == MOTION_POSITION_END ?
                         (1.0f - kernel_data.cam.shuttertime) + sd->time :
                         sd->time;

  /* Use a 1st order semi-lagrangian advection scheme to estimate what volume quantity
   * existed, or will exist, at the given time:
   *
   * `phi(x, T) = phi(x - (T - t) * u(x, T), t)`
   *
   * where
   *
   * x : position
   * T : super-sampled time (or ray time)
   * t : current time of the simulation (in rendering we assume this is center frame with
   * relative time = 0)
   * phi : the volume quantity
   * u : the velocity field
   *
   * But first we need to determine the velocity field `u(x, T)`, which we can estimate also
   * using semi-lagrangian advection.
   *
   * `u(x, T) = u(x - (T - t) * u(x, T), t)`
   *
   * This is the typical way to model self-advection in fluid dynamics, however, we do not
   * account for other forces affecting the velocity during simulation (pressure, buoyancy,
   * etc.): this gives a linear interpolation when fluid are mostly "curvy". For better
   * results, a higher order interpolation scheme can be used (at the cost of more lookups),
   * or an interpolation of the velocity fields for the previous and next frames could also
   * be used to estimate `u(x, T)` (which will cost more memory and lookups).
   *
   * References:
   * "Eulerian Motion Blur", Kim and Ko, 2007
   * "Production Volume Rendering", Wreninge et al., 2012
   */

  /* Find velocity. */
  float3 velocity = primitive_volume_attribute_float3(kg, sd, v_desc);
  object_dir_transform(kg, sd, &velocity);

  /* Find advected P. */
  sd->P = P - (time - time_offset) * velocity_scale * velocity;

  /* Find advected velocity. */
  velocity = primitive_volume_attribute_float3(kg, sd, v_desc);
  object_dir_transform(kg, sd, &velocity);

  /* Find advected P. */
  sd->P = P - (time - time_offset) * velocity_scale * velocity;
}
#  endif

/* Volume Evaluation */

template<const bool shadow, typename StackReadOp, typename ConstIntegratorGenericState>
ccl_device_inline void volume_shader_eval(KernelGlobals kg,
                                          ConstIntegratorGenericState state,
                                          ccl_private ShaderData *ccl_restrict sd,
                                          const uint32_t path_flag,
                                          StackReadOp stack_read)
{
  /* If path is being terminated, we are tracing a shadow ray or evaluating
   * emission, then we don't need to store closures. The emission and shadow
   * shader data also do not have a closure array to save GPU memory. */
  int max_closures;
  if (path_flag & (PATH_RAY_TERMINATE | PATH_RAY_SHADOW | PATH_RAY_EMISSION)) {
    max_closures = 0;
  }
  else {
    max_closures = kernel_data.max_closures;
  }

  /* reset closures once at the start, we will be accumulating the closures
   * for all volumes in the stack into a single array of closures */
  sd->num_closure = 0;
  sd->num_closure_left = max_closures;
  sd->flag = 0;
  sd->object_flag = 0;

  for (int i = 0;; i++) {
    const VolumeStack entry = stack_read(i);
    if (entry.shader == SHADER_NONE) {
      break;
    }

    /* Setup shader-data from stack. it's mostly setup already in
     * shader_setup_from_volume, this switching should be quick. */
    sd->object = entry.object;
    sd->lamp = LAMP_NONE;
    sd->shader = entry.shader;

    sd->flag &= ~SD_SHADER_FLAGS;
    sd->flag |= kernel_data_fetch(shaders, (sd->shader & SHADER_MASK)).flags;
    sd->object_flag &= ~SD_OBJECT_FLAGS;

    if (sd->object != OBJECT_NONE) {
      sd->object_flag |= kernel_data_fetch(object_flag, sd->object);

#  ifdef __OBJECT_MOTION__
      /* todo: this is inefficient for motion blur, we should be
       * caching matrices instead of recomputing them each step */
      shader_setup_object_transforms(kg, sd, sd->time);

      volume_shader_motion_blur(kg, sd);
#  endif
    }

    /* evaluate shader */
#  ifdef __OSL__
    if (kg->osl) {
      OSLShader::eval_volume(kg, state, sd, path_flag);
    }
    else
#  endif
    {
#  ifdef __SVM__
      svm_eval_nodes<KERNEL_FEATURE_NODE_MASK_VOLUME, SHADER_TYPE_VOLUME>(
          kg, state, sd, NULL, path_flag);
#  endif
    }

    /* Merge closures to avoid exceeding number of closures limit. */
    if (!shadow) {
      if (i > 0) {
        volume_shader_merge_closures(sd);
      }
    }
  }
}

#endif /* __VOLUME__ */

CCL_NAMESPACE_END
