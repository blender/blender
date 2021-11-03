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

#include "scene/integrator.h"
#include "device/device.h"
#include "scene/background.h"
#include "scene/camera.h"
#include "scene/film.h"
#include "scene/jitter.h"
#include "scene/light.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/sobol.h"
#include "scene/stats.h"

#include "kernel/types.h"

#include "util/foreach.h"
#include "util/hash.h"
#include "util/log.h"
#include "util/task.h"
#include "util/time.h"

CCL_NAMESPACE_BEGIN

NODE_DEFINE(Integrator)
{
  NodeType *type = NodeType::add("integrator", create);

  SOCKET_INT(min_bounce, "Min Bounce", 0);
  SOCKET_INT(max_bounce, "Max Bounce", 7);

  SOCKET_INT(max_diffuse_bounce, "Max Diffuse Bounce", 7);
  SOCKET_INT(max_glossy_bounce, "Max Glossy Bounce", 7);
  SOCKET_INT(max_transmission_bounce, "Max Transmission Bounce", 7);
  SOCKET_INT(max_volume_bounce, "Max Volume Bounce", 7);

  SOCKET_INT(transparent_min_bounce, "Transparent Min Bounce", 0);
  SOCKET_INT(transparent_max_bounce, "Transparent Max Bounce", 7);

  SOCKET_INT(ao_bounces, "AO Bounces", 0);
  SOCKET_FLOAT(ao_factor, "AO Factor", 0.0f);
  SOCKET_FLOAT(ao_distance, "AO Distance", FLT_MAX);
  SOCKET_FLOAT(ao_additive_factor, "AO Additive Factor", 0.0f);

  SOCKET_INT(volume_max_steps, "Volume Max Steps", 1024);
  SOCKET_FLOAT(volume_step_rate, "Volume Step Rate", 1.0f);

  SOCKET_BOOLEAN(caustics_reflective, "Reflective Caustics", true);
  SOCKET_BOOLEAN(caustics_refractive, "Refractive Caustics", true);
  SOCKET_FLOAT(filter_glossy, "Filter Glossy", 0.0f);
  SOCKET_INT(seed, "Seed", 0);
  SOCKET_FLOAT(sample_clamp_direct, "Sample Clamp Direct", 0.0f);
  SOCKET_FLOAT(sample_clamp_indirect, "Sample Clamp Indirect", 0.0f);
  SOCKET_BOOLEAN(motion_blur, "Motion Blur", false);

  SOCKET_INT(aa_samples, "AA Samples", 0);
  SOCKET_INT(start_sample, "Start Sample", 0);

  SOCKET_BOOLEAN(use_adaptive_sampling, "Use Adaptive Sampling", false);
  SOCKET_FLOAT(adaptive_threshold, "Adaptive Threshold", 0.0f);
  SOCKET_INT(adaptive_min_samples, "Adaptive Min Samples", 0);

  SOCKET_FLOAT(light_sampling_threshold, "Light Sampling Threshold", 0.05f);

  static NodeEnum sampling_pattern_enum;
  sampling_pattern_enum.insert("sobol", SAMPLING_PATTERN_SOBOL);
  sampling_pattern_enum.insert("pmj", SAMPLING_PATTERN_PMJ);
  SOCKET_ENUM(sampling_pattern, "Sampling Pattern", sampling_pattern_enum, SAMPLING_PATTERN_SOBOL);
  SOCKET_FLOAT(scrambling_distance, "Scrambling Distance", 1.0f);

  static NodeEnum denoiser_type_enum;
  denoiser_type_enum.insert("optix", DENOISER_OPTIX);
  denoiser_type_enum.insert("openimagedenoise", DENOISER_OPENIMAGEDENOISE);

  static NodeEnum denoiser_prefilter_enum;
  denoiser_prefilter_enum.insert("none", DENOISER_PREFILTER_NONE);
  denoiser_prefilter_enum.insert("fast", DENOISER_PREFILTER_FAST);
  denoiser_prefilter_enum.insert("accurate", DENOISER_PREFILTER_ACCURATE);

  /* Default to accurate denoising with OpenImageDenoise. For interactive viewport
   * it's best use OptiX and disable the normal pass since it does not always have
   * the desired effect for that denoiser. */
  SOCKET_BOOLEAN(use_denoise, "Use Denoiser", false);
  SOCKET_ENUM(denoiser_type, "Denoiser Type", denoiser_type_enum, DENOISER_OPENIMAGEDENOISE);
  SOCKET_INT(denoise_start_sample, "Start Sample to Denoise", 0);
  SOCKET_BOOLEAN(use_denoise_pass_albedo, "Use Albedo Pass for Denoiser", true);
  SOCKET_BOOLEAN(use_denoise_pass_normal, "Use Normal Pass for Denoiser", true);
  SOCKET_ENUM(
      denoiser_prefilter, "Denoiser Type", denoiser_prefilter_enum, DENOISER_PREFILTER_ACCURATE);

  return type;
}

Integrator::Integrator() : Node(get_node_type())
{
}

Integrator::~Integrator()
{
}

void Integrator::device_update(Device *device, DeviceScene *dscene, Scene *scene)
{
  if (!is_modified())
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->update_stats) {
      scene->update_stats->integrator.times.add_entry({"device_update", time});
    }
  });

  KernelIntegrator *kintegrator = &dscene->data.integrator;

  /* Adaptive sampling requires PMJ samples.
   *
   * This also makes detection of sampling pattern a bit more involved: can not rely on the changed
   * state of socket, since its value might be different from the effective value used here. So
   * instead compare with previous value in the KernelIntegrator. Only do it if the device was
   * updated once (in which case the `sample_pattern_lut` will be allocated to a non-zero size). */
  const SamplingPattern new_sampling_pattern = (use_adaptive_sampling) ? SAMPLING_PATTERN_PMJ :
                                                                         sampling_pattern;

  const bool need_update_lut = max_bounce_is_modified() || max_transmission_bounce_is_modified() ||
                               dscene->sample_pattern_lut.size() == 0 ||
                               kintegrator->sampling_pattern != new_sampling_pattern;

  if (need_update_lut) {
    dscene->sample_pattern_lut.tag_realloc();
  }

  device_free(device, dscene);

  /* integrator parameters */
  kintegrator->min_bounce = min_bounce + 1;
  kintegrator->max_bounce = max_bounce + 1;

  kintegrator->max_diffuse_bounce = max_diffuse_bounce + 1;
  kintegrator->max_glossy_bounce = max_glossy_bounce + 1;
  kintegrator->max_transmission_bounce = max_transmission_bounce + 1;
  kintegrator->max_volume_bounce = max_volume_bounce + 1;

  kintegrator->transparent_min_bounce = transparent_min_bounce + 1;
  kintegrator->transparent_max_bounce = transparent_max_bounce + 1;

  kintegrator->ao_bounces = ao_bounces;
  kintegrator->ao_bounces_distance = ao_distance;
  kintegrator->ao_bounces_factor = ao_factor;
  kintegrator->ao_additive_factor = ao_additive_factor;

  /* Transparent Shadows
   * We only need to enable transparent shadows, if we actually have
   * transparent shaders in the scene. Otherwise we can disable it
   * to improve performance a bit. */
  kintegrator->transparent_shadows = false;
  foreach (Shader *shader, scene->shaders) {
    /* keep this in sync with SD_HAS_TRANSPARENT_SHADOW in shader.cpp */
    if ((shader->has_surface_transparent && shader->get_use_transparent_shadow()) ||
        shader->has_volume) {
      kintegrator->transparent_shadows = true;
      break;
    }
  }

  kintegrator->volume_max_steps = volume_max_steps;
  kintegrator->volume_step_rate = volume_step_rate;

  kintegrator->caustics_reflective = caustics_reflective;
  kintegrator->caustics_refractive = caustics_refractive;
  kintegrator->filter_glossy = (filter_glossy == 0.0f) ? FLT_MAX : 1.0f / filter_glossy;

  kintegrator->seed = seed;

  kintegrator->sample_clamp_direct = (sample_clamp_direct == 0.0f) ? FLT_MAX :
                                                                     sample_clamp_direct * 3.0f;
  kintegrator->sample_clamp_indirect = (sample_clamp_indirect == 0.0f) ?
                                           FLT_MAX :
                                           sample_clamp_indirect * 3.0f;

  kintegrator->sampling_pattern = new_sampling_pattern;
  kintegrator->scrambling_distance = scrambling_distance;

  if (light_sampling_threshold > 0.0f) {
    kintegrator->light_inv_rr_threshold = 1.0f / light_sampling_threshold;
  }
  else {
    kintegrator->light_inv_rr_threshold = 0.0f;
  }

  /* sobol directions table */
  int max_samples = max_bounce + transparent_max_bounce + 3 + VOLUME_BOUNDS_MAX +
                    max(BSSRDF_MAX_HITS, BSSRDF_MAX_BOUNCES);

  int dimensions = PRNG_BASE_NUM + max_samples * PRNG_BOUNCE_NUM;
  dimensions = min(dimensions, SOBOL_MAX_DIMENSIONS);

  if (need_update_lut) {
    if (kintegrator->sampling_pattern == SAMPLING_PATTERN_SOBOL) {
      uint *directions = (uint *)dscene->sample_pattern_lut.alloc(SOBOL_BITS * dimensions);

      sobol_generate_direction_vectors((uint(*)[SOBOL_BITS])directions, dimensions);

      dscene->sample_pattern_lut.copy_to_device();
    }
    else {
      constexpr int sequence_size = NUM_PMJ_SAMPLES;
      constexpr int num_sequences = NUM_PMJ_PATTERNS;
      float2 *directions = (float2 *)dscene->sample_pattern_lut.alloc(sequence_size *
                                                                      num_sequences * 2);
      TaskPool pool;
      for (int j = 0; j < num_sequences; ++j) {
        float2 *sequence = directions + j * sequence_size;
        pool.push(
            function_bind(&progressive_multi_jitter_02_generate_2D, sequence, sequence_size, j));
      }
      pool.wait_work();

      dscene->sample_pattern_lut.copy_to_device();
    }
  }

  kintegrator->has_shadow_catcher = scene->has_shadow_catcher();

  dscene->sample_pattern_lut.clear_modified();
  clear_modified();
}

void Integrator::device_free(Device *, DeviceScene *dscene, bool force_free)
{
  dscene->sample_pattern_lut.free_if_need_realloc(force_free);
}

void Integrator::tag_update(Scene *scene, uint32_t flag)
{
  if (flag & UPDATE_ALL) {
    tag_modified();
  }

  if (flag & AO_PASS_MODIFIED) {
    /* tag only the ao_bounces socket as modified so we avoid updating sample_pattern_lut
     * unnecessarily */
    tag_ao_bounces_modified();
  }

  if (filter_glossy_is_modified()) {
    foreach (Shader *shader, scene->shaders) {
      if (shader->has_integrator_dependency) {
        scene->shader_manager->tag_update(scene, ShaderManager::INTEGRATOR_MODIFIED);
        break;
      }
    }
  }

  if (motion_blur_is_modified()) {
    scene->object_manager->tag_update(scene, ObjectManager::MOTION_BLUR_MODIFIED);
    scene->camera->tag_modified();
  }
}

uint Integrator::get_kernel_features() const
{
  uint kernel_features = 0;

  if (ao_additive_factor != 0.0f) {
    kernel_features |= KERNEL_FEATURE_AO_ADDITIVE;
  }

  return kernel_features;
}

AdaptiveSampling Integrator::get_adaptive_sampling() const
{
  AdaptiveSampling adaptive_sampling;

  adaptive_sampling.use = use_adaptive_sampling;

  if (!adaptive_sampling.use) {
    return adaptive_sampling;
  }

  if (aa_samples > 0 && adaptive_threshold == 0.0f) {
    adaptive_sampling.threshold = max(0.001f, 1.0f / (float)aa_samples);
    VLOG(1) << "Cycles adaptive sampling: automatic threshold = " << adaptive_sampling.threshold;
  }
  else {
    adaptive_sampling.threshold = adaptive_threshold;
  }

  if (adaptive_sampling.threshold > 0 && adaptive_min_samples == 0) {
    /* Threshold 0.1 -> 32, 0.01 -> 64, 0.001 -> 128.
     * This is highly scene dependent, we make a guess that seemed to work well
     * in various test scenes. */
    const int min_samples = (int)ceilf(16.0f / powf(adaptive_sampling.threshold, 0.3f));
    adaptive_sampling.min_samples = max(4, min_samples);
    VLOG(1) << "Cycles adaptive sampling: automatic min samples = "
            << adaptive_sampling.min_samples;
  }
  else {
    adaptive_sampling.min_samples = max(4, adaptive_min_samples);
  }

  /* Arbitrary factor that makes the threshold more similar to what is was before,
   * and gives arguably more intuitive values. */
  adaptive_sampling.threshold *= 5.0f;

  adaptive_sampling.adaptive_step = 16;

  DCHECK(is_power_of_two(adaptive_sampling.adaptive_step))
      << "Adaptive step must be a power of two for bitwise operations to work";

  return adaptive_sampling;
}

DenoiseParams Integrator::get_denoise_params() const
{
  DenoiseParams denoise_params;

  denoise_params.use = use_denoise;

  denoise_params.type = denoiser_type;

  denoise_params.start_sample = denoise_start_sample;

  denoise_params.use_pass_albedo = use_denoise_pass_albedo;
  denoise_params.use_pass_normal = use_denoise_pass_normal;

  denoise_params.prefilter = denoiser_prefilter;

  return denoise_params;
}

CCL_NAMESPACE_END
