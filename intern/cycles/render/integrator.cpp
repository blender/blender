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

#include "render/integrator.h"
#include "device/device.h"
#include "render/background.h"
#include "render/camera.h"
#include "render/film.h"
#include "render/jitter.h"
#include "render/light.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/shader.h"
#include "render/sobol.h"
#include "render/stats.h"

#include "kernel/kernel_types.h"

#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"
#include "util/util_task.h"
#include "util/util_time.h"

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
  SOCKET_INT(diffuse_samples, "Diffuse Samples", 1);
  SOCKET_INT(glossy_samples, "Glossy Samples", 1);
  SOCKET_INT(transmission_samples, "Transmission Samples", 1);
  SOCKET_INT(ao_samples, "AO Samples", 1);
  SOCKET_INT(mesh_light_samples, "Mesh Light Samples", 1);
  SOCKET_INT(subsurface_samples, "Subsurface Samples", 1);
  SOCKET_INT(volume_samples, "Volume Samples", 1);
  SOCKET_INT(start_sample, "Start Sample", 0);

  SOCKET_FLOAT(adaptive_threshold, "Adaptive Threshold", 0.0f);
  SOCKET_INT(adaptive_min_samples, "Adaptive Min Samples", 0);

  SOCKET_BOOLEAN(sample_all_lights_direct, "Sample All Lights Direct", true);
  SOCKET_BOOLEAN(sample_all_lights_indirect, "Sample All Lights Indirect", true);
  SOCKET_FLOAT(light_sampling_threshold, "Light Sampling Threshold", 0.05f);

  static NodeEnum method_enum;
  method_enum.insert("path", PATH);
  method_enum.insert("branched_path", BRANCHED_PATH);
  SOCKET_ENUM(method, "Method", method_enum, PATH);

  static NodeEnum sampling_pattern_enum;
  sampling_pattern_enum.insert("sobol", SAMPLING_PATTERN_SOBOL);
  sampling_pattern_enum.insert("cmj", SAMPLING_PATTERN_CMJ);
  sampling_pattern_enum.insert("pmj", SAMPLING_PATTERN_PMJ);
  SOCKET_ENUM(sampling_pattern, "Sampling Pattern", sampling_pattern_enum, SAMPLING_PATTERN_SOBOL);

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

  const bool need_update_lut = ao_samples_is_modified() || diffuse_samples_is_modified() ||
                               glossy_samples_is_modified() || max_bounce_is_modified() ||
                               max_transmission_bounce_is_modified() ||
                               mesh_light_samples_is_modified() || method_is_modified() ||
                               sampling_pattern_is_modified() ||
                               subsurface_samples_is_modified() ||
                               transmission_samples_is_modified() || volume_samples_is_modified();

  if (need_update_lut) {
    dscene->sample_pattern_lut.tag_realloc();
  }

  device_free(device, dscene);

  KernelIntegrator *kintegrator = &dscene->data.integrator;

  /* integrator parameters */
  kintegrator->min_bounce = min_bounce + 1;
  kintegrator->max_bounce = max_bounce + 1;

  kintegrator->max_diffuse_bounce = max_diffuse_bounce + 1;
  kintegrator->max_glossy_bounce = max_glossy_bounce + 1;
  kintegrator->max_transmission_bounce = max_transmission_bounce + 1;
  kintegrator->max_volume_bounce = max_volume_bounce + 1;

  kintegrator->transparent_min_bounce = transparent_min_bounce + 1;
  kintegrator->transparent_max_bounce = transparent_max_bounce + 1;

  if (ao_bounces == 0) {
    kintegrator->ao_bounces = INT_MAX;
  }
  else {
    kintegrator->ao_bounces = ao_bounces - 1;
  }

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

  kintegrator->seed = hash_uint2(seed, 0);

  kintegrator->use_ambient_occlusion = ((Pass::contains(scene->passes, PASS_AO)) ||
                                        dscene->data.background.ao_factor != 0.0f);

  kintegrator->sample_clamp_direct = (sample_clamp_direct == 0.0f) ? FLT_MAX :
                                                                     sample_clamp_direct * 3.0f;
  kintegrator->sample_clamp_indirect = (sample_clamp_indirect == 0.0f) ?
                                           FLT_MAX :
                                           sample_clamp_indirect * 3.0f;

  kintegrator->branched = (method == BRANCHED_PATH) && device->info.has_branched_path;
  kintegrator->volume_decoupled = device->info.has_volume_decoupled;
  kintegrator->diffuse_samples = diffuse_samples;
  kintegrator->glossy_samples = glossy_samples;
  kintegrator->transmission_samples = transmission_samples;
  kintegrator->ao_samples = ao_samples;
  kintegrator->mesh_light_samples = mesh_light_samples;
  kintegrator->subsurface_samples = subsurface_samples;
  kintegrator->volume_samples = volume_samples;
  kintegrator->start_sample = start_sample;

  if (kintegrator->branched) {
    kintegrator->sample_all_lights_direct = sample_all_lights_direct;
    kintegrator->sample_all_lights_indirect = sample_all_lights_indirect;
  }
  else {
    kintegrator->sample_all_lights_direct = false;
    kintegrator->sample_all_lights_indirect = false;
  }

  kintegrator->sampling_pattern = sampling_pattern;
  kintegrator->aa_samples = aa_samples;
  if (aa_samples > 0 && adaptive_min_samples == 0) {
    kintegrator->adaptive_min_samples = max(4, (int)sqrtf(aa_samples));
    VLOG(1) << "Cycles adaptive sampling: automatic min samples = "
            << kintegrator->adaptive_min_samples;
  }
  else {
    kintegrator->adaptive_min_samples = max(4, adaptive_min_samples);
  }

  kintegrator->adaptive_step = 4;
  kintegrator->adaptive_stop_per_sample = device->info.has_adaptive_stop_per_sample;

  /* Adaptive step must be a power of two for bitwise operations to work. */
  assert((kintegrator->adaptive_step & (kintegrator->adaptive_step - 1)) == 0);

  if (aa_samples > 0 && adaptive_threshold == 0.0f) {
    kintegrator->adaptive_threshold = max(0.001f, 1.0f / (float)aa_samples);
    VLOG(1) << "Cycles adaptive sampling: automatic threshold = "
            << kintegrator->adaptive_threshold;
  }
  else {
    kintegrator->adaptive_threshold = adaptive_threshold;
  }

  if (light_sampling_threshold > 0.0f) {
    kintegrator->light_inv_rr_threshold = 1.0f / light_sampling_threshold;
  }
  else {
    kintegrator->light_inv_rr_threshold = 0.0f;
  }

  /* sobol directions table */
  int max_samples = 1;

  if (kintegrator->branched) {
    foreach (Light *light, scene->lights)
      max_samples = max(max_samples, light->get_samples());

    max_samples = max(max_samples,
                      max(diffuse_samples, max(glossy_samples, transmission_samples)));
    max_samples = max(max_samples, max(ao_samples, max(mesh_light_samples, subsurface_samples)));
    max_samples = max(max_samples, volume_samples);
  }

  uint total_bounces = max_bounce + transparent_max_bounce + 3 + VOLUME_BOUNDS_MAX +
                       max(BSSRDF_MAX_HITS, BSSRDF_MAX_BOUNCES);

  max_samples *= total_bounces;

  int dimensions = PRNG_BASE_NUM + max_samples * PRNG_BOUNCE_NUM;
  dimensions = min(dimensions, SOBOL_MAX_DIMENSIONS);

  if (need_update_lut) {
    if (sampling_pattern == SAMPLING_PATTERN_SOBOL) {
      uint *directions = dscene->sample_pattern_lut.alloc(SOBOL_BITS * dimensions);

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

  if (flag & (AO_PASS_MODIFIED | BACKGROUND_AO_MODIFIED)) {
    /* tag only the ao_bounces socket as modified so we avoid updating sample_pattern_lut
     * unnecessarily */
    tag_ao_bounces_modified();
  }

  if ((flag & LIGHT_SAMPLES_MODIFIED) && (method == BRANCHED_PATH)) {
    /* the number of light samples may affect the size of the sample_pattern_lut */
    tag_sampling_pattern_modified();
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

CCL_NAMESPACE_END
