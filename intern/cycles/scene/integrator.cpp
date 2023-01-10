/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "device/device.h"

#include "scene/background.h"
#include "scene/bake.h"
#include "scene/camera.h"
#include "scene/film.h"
#include "scene/integrator.h"
#include "scene/light.h"
#include "scene/object.h"
#include "scene/scene.h"
#include "scene/shader.h"
#include "scene/stats.h"
#include "scene/tabulated_sobol.h"

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

#ifdef WITH_CYCLES_DEBUG
  static NodeEnum direct_light_sampling_type_enum;
  direct_light_sampling_type_enum.insert("multiple_importance_sampling",
                                         DIRECT_LIGHT_SAMPLING_MIS);
  direct_light_sampling_type_enum.insert("forward_path_tracing", DIRECT_LIGHT_SAMPLING_FORWARD);
  direct_light_sampling_type_enum.insert("next_event_estimation", DIRECT_LIGHT_SAMPLING_NEE);
  SOCKET_ENUM(direct_light_sampling_type,
              "Direct Light Sampling Type",
              direct_light_sampling_type_enum,
              DIRECT_LIGHT_SAMPLING_MIS);
#endif

  SOCKET_INT(ao_bounces, "AO Bounces", 0);
  SOCKET_FLOAT(ao_factor, "AO Factor", 0.0f);
  SOCKET_FLOAT(ao_distance, "AO Distance", FLT_MAX);
  SOCKET_FLOAT(ao_additive_factor, "AO Additive Factor", 0.0f);

  SOCKET_INT(volume_max_steps, "Volume Max Steps", 1024);
  SOCKET_FLOAT(volume_step_rate, "Volume Step Rate", 1.0f);

  static NodeEnum guiding_ditribution_enum;
  guiding_ditribution_enum.insert("PARALLAX_AWARE_VMM", GUIDING_TYPE_PARALLAX_AWARE_VMM);
  guiding_ditribution_enum.insert("DIRECTIONAL_QUAD_TREE", GUIDING_TYPE_DIRECTIONAL_QUAD_TREE);
  guiding_ditribution_enum.insert("VMM", GUIDING_TYPE_VMM);

  SOCKET_BOOLEAN(use_guiding, "Guiding", false);
  SOCKET_BOOLEAN(deterministic_guiding, "Deterministic Guiding", true);
  SOCKET_BOOLEAN(use_surface_guiding, "Surface Guiding", true);
  SOCKET_FLOAT(surface_guiding_probability, "Surface Guiding Probability", 0.5f);
  SOCKET_BOOLEAN(use_volume_guiding, "Volume Guiding", true);
  SOCKET_FLOAT(volume_guiding_probability, "Volume Guiding Probability", 0.5f);
  SOCKET_INT(guiding_training_samples, "Training Samples", 128);
  SOCKET_BOOLEAN(use_guiding_direct_light, "Guide Direct Light", true);
  SOCKET_BOOLEAN(use_guiding_mis_weights, "Use MIS Weights", true);
  SOCKET_ENUM(guiding_distribution_type,
              "Guiding Distribution Type",
              guiding_ditribution_enum,
              GUIDING_TYPE_PARALLAX_AWARE_VMM);

  SOCKET_BOOLEAN(caustics_reflective, "Reflective Caustics", true);
  SOCKET_BOOLEAN(caustics_refractive, "Refractive Caustics", true);
  SOCKET_FLOAT(filter_glossy, "Filter Glossy", 0.0f);

  SOCKET_BOOLEAN(use_direct_light, "Use Direct Light", true);
  SOCKET_BOOLEAN(use_indirect_light, "Use Indirect Light", true);
  SOCKET_BOOLEAN(use_diffuse, "Use Diffuse", true);
  SOCKET_BOOLEAN(use_glossy, "Use Glossy", true);
  SOCKET_BOOLEAN(use_transmission, "Use Transmission", true);
  SOCKET_BOOLEAN(use_emission, "Use Emission", true);

  SOCKET_INT(seed, "Seed", 0);
  SOCKET_FLOAT(sample_clamp_direct, "Sample Clamp Direct", 0.0f);
  SOCKET_FLOAT(sample_clamp_indirect, "Sample Clamp Indirect", 10.0f);
  SOCKET_BOOLEAN(motion_blur, "Motion Blur", false);

  SOCKET_INT(aa_samples, "AA Samples", 0);
  SOCKET_INT(start_sample, "Start Sample", 0);

  SOCKET_BOOLEAN(use_adaptive_sampling, "Use Adaptive Sampling", true);
  SOCKET_FLOAT(adaptive_threshold, "Adaptive Threshold", 0.01f);
  SOCKET_INT(adaptive_min_samples, "Adaptive Min Samples", 0);

  SOCKET_BOOLEAN(use_light_tree, "Use light tree to optimize many light sampling", true);
  SOCKET_FLOAT(light_sampling_threshold, "Light Sampling Threshold", 0.0f);

  static NodeEnum sampling_pattern_enum;
  sampling_pattern_enum.insert("sobol_burley", SAMPLING_PATTERN_SOBOL_BURLEY);
  sampling_pattern_enum.insert("tabulated_sobol", SAMPLING_PATTERN_TABULATED_SOBOL);
  SOCKET_ENUM(sampling_pattern,
              "Sampling Pattern",
              sampling_pattern_enum,
              SAMPLING_PATTERN_TABULATED_SOBOL);
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
  SOCKET_ENUM(denoiser_prefilter,
              "Denoiser Prefilter",
              denoiser_prefilter_enum,
              DENOISER_PREFILTER_ACCURATE);

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

  kintegrator->ao_bounces = (ao_factor != 0.0f) ? ao_bounces : 0;
  kintegrator->ao_bounces_distance = ao_distance;
  kintegrator->ao_bounces_factor = ao_factor;
  kintegrator->ao_additive_factor = ao_additive_factor;

#ifdef WITH_CYCLES_DEBUG
  kintegrator->direct_light_sampling_type = direct_light_sampling_type;
#else
  kintegrator->direct_light_sampling_type = DIRECT_LIGHT_SAMPLING_MIS;
#endif

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

  kintegrator->filter_closures = 0;
  if (!use_direct_light) {
    kintegrator->filter_closures |= FILTER_CLOSURE_DIRECT_LIGHT;
  }
  if (!use_indirect_light) {
    kintegrator->min_bounce = 1;
    kintegrator->max_bounce = 1;
  }
  if (!use_diffuse) {
    kintegrator->filter_closures |= FILTER_CLOSURE_DIFFUSE;
  }
  if (!use_glossy) {
    kintegrator->filter_closures |= FILTER_CLOSURE_GLOSSY;
  }
  if (!use_transmission) {
    kintegrator->filter_closures |= FILTER_CLOSURE_TRANSMISSION;
  }
  if (!use_emission) {
    kintegrator->filter_closures |= FILTER_CLOSURE_EMISSION;
  }
  if (scene->bake_manager->get_baking()) {
    /* Baking does not need to trace through transparency, we only want to bake
     * the object itself. */
    kintegrator->filter_closures |= FILTER_CLOSURE_TRANSPARENT;
  }

  GuidingParams guiding_params = get_guiding_params(device);
  kintegrator->use_guiding = guiding_params.use;
  kintegrator->train_guiding = kintegrator->use_guiding;
  kintegrator->use_surface_guiding = guiding_params.use_surface_guiding;
  kintegrator->use_volume_guiding = guiding_params.use_volume_guiding;
  kintegrator->surface_guiding_probability = surface_guiding_probability;
  kintegrator->volume_guiding_probability = volume_guiding_probability;
  kintegrator->use_guiding_direct_light = use_guiding_direct_light;
  kintegrator->use_guiding_mis_weights = use_guiding_mis_weights;
  kintegrator->guiding_distribution_type = guiding_params.type;

  kintegrator->seed = seed;

  kintegrator->sample_clamp_direct = (sample_clamp_direct == 0.0f) ? FLT_MAX :
                                                                     sample_clamp_direct * 3.0f;
  kintegrator->sample_clamp_indirect = (sample_clamp_indirect == 0.0f) ?
                                           FLT_MAX :
                                           sample_clamp_indirect * 3.0f;

  kintegrator->sampling_pattern = sampling_pattern;
  kintegrator->scrambling_distance = scrambling_distance;
  kintegrator->sobol_index_mask = reverse_integer_bits(next_power_of_two(aa_samples - 1) - 1);

  kintegrator->use_light_tree = scene->integrator->use_light_tree;
  if (light_sampling_threshold > 0.0f) {
    kintegrator->light_inv_rr_threshold = scene->film->get_exposure() / light_sampling_threshold;
  }
  else {
    kintegrator->light_inv_rr_threshold = 0.0f;
  }

  /* Build pre-tabulated Sobol samples if needed. */
  int sequence_size = clamp(
      next_power_of_two(aa_samples - 1), MIN_TAB_SOBOL_SAMPLES, MAX_TAB_SOBOL_SAMPLES);
  if (kintegrator->sampling_pattern == SAMPLING_PATTERN_TABULATED_SOBOL &&
      dscene->sample_pattern_lut.size() !=
          (sequence_size * NUM_TAB_SOBOL_PATTERNS * NUM_TAB_SOBOL_DIMENSIONS)) {
    kintegrator->tabulated_sobol_sequence_size = sequence_size;

    if (dscene->sample_pattern_lut.size() != 0) {
      dscene->sample_pattern_lut.free();
    }
    float4 *directions = (float4 *)dscene->sample_pattern_lut.alloc(
        sequence_size * NUM_TAB_SOBOL_PATTERNS * NUM_TAB_SOBOL_DIMENSIONS);
    TaskPool pool;
    for (int j = 0; j < NUM_TAB_SOBOL_PATTERNS; ++j) {
      float4 *sequence = directions + j * sequence_size;
      pool.push(function_bind(&tabulated_sobol_generate_4D, sequence, sequence_size, j));
    }
    pool.wait_work();

    dscene->sample_pattern_lut.copy_to_device();
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
    VLOG_INFO << "Cycles adaptive sampling: automatic threshold = " << adaptive_sampling.threshold;
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
    VLOG_INFO << "Cycles adaptive sampling: automatic min samples = "
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

GuidingParams Integrator::get_guiding_params(const Device *device) const
{
  const bool use = use_guiding && device->info.has_guiding;

  GuidingParams guiding_params;
  guiding_params.use_surface_guiding = use && use_surface_guiding &&
                                       surface_guiding_probability > 0.0f;
  guiding_params.use_volume_guiding = use && use_volume_guiding &&
                                      volume_guiding_probability > 0.0f;
  guiding_params.use = guiding_params.use_surface_guiding || guiding_params.use_volume_guiding;
  guiding_params.type = guiding_distribution_type;
  guiding_params.training_samples = guiding_training_samples;
  guiding_params.deterministic = deterministic_guiding;

  return guiding_params;
}
CCL_NAMESPACE_END
