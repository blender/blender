/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/types.h"

#include "device/denoise.h" /* For the parameters and type enum. */
#include "graph/node.h"
#include "integrator/adaptive_sampling.h"
#include "integrator/guiding.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

class Integrator : public Node {
 public:
  NODE_DECLARE

  NODE_SOCKET_API(int, min_bounce)
  NODE_SOCKET_API(int, max_bounce)

  NODE_SOCKET_API(int, max_diffuse_bounce)
  NODE_SOCKET_API(int, max_glossy_bounce)
  NODE_SOCKET_API(int, max_transmission_bounce)
  NODE_SOCKET_API(int, max_volume_bounce)

#ifdef WITH_CYCLES_DEBUG
  NODE_SOCKET_API(DirectLightSamplingType, direct_light_sampling_type)
#endif

  NODE_SOCKET_API(int, transparent_min_bounce)
  NODE_SOCKET_API(int, transparent_max_bounce)

  NODE_SOCKET_API(int, ao_bounces)
  NODE_SOCKET_API(float, ao_factor)
  NODE_SOCKET_API(float, ao_distance)
  NODE_SOCKET_API(float, ao_additive_factor)

  NODE_SOCKET_API(bool, volume_ray_marching)
  NODE_SOCKET_API(int, volume_max_steps)
  NODE_SOCKET_API(float, volume_step_rate)

  NODE_SOCKET_API(bool, use_guiding);
  NODE_SOCKET_API(bool, deterministic_guiding);
  NODE_SOCKET_API(bool, use_surface_guiding);
  NODE_SOCKET_API(float, surface_guiding_probability);
  NODE_SOCKET_API(bool, use_volume_guiding);
  NODE_SOCKET_API(float, volume_guiding_probability);
  NODE_SOCKET_API(int, guiding_training_samples);
  NODE_SOCKET_API(bool, use_guiding_direct_light);
  NODE_SOCKET_API(bool, use_guiding_mis_weights);
  NODE_SOCKET_API(GuidingDistributionType, guiding_distribution_type);
  NODE_SOCKET_API(GuidingDirectionalSamplingType, guiding_directional_sampling_type);
  NODE_SOCKET_API(float, guiding_roughness_threshold);

  NODE_SOCKET_API(bool, caustics_reflective)
  NODE_SOCKET_API(bool, caustics_refractive)
  NODE_SOCKET_API(float, filter_glossy)

  NODE_SOCKET_API(bool, use_direct_light);
  NODE_SOCKET_API(bool, use_indirect_light);
  NODE_SOCKET_API(bool, use_diffuse);
  NODE_SOCKET_API(bool, use_glossy);
  NODE_SOCKET_API(bool, use_transmission);
  NODE_SOCKET_API(bool, use_emission);

  NODE_SOCKET_API(int, seed)

  NODE_SOCKET_API(float, sample_clamp_direct)
  NODE_SOCKET_API(float, sample_clamp_indirect)
  NODE_SOCKET_API(bool, motion_blur)

  /* Maximum number of samples, beyond which we are likely to run into
   * precision issues for sampling patterns. */
  static const int MAX_SAMPLES = (1 << 24);

  NODE_SOCKET_API(int, aa_samples)

  NODE_SOCKET_API(bool, use_sample_subset)
  NODE_SOCKET_API(int, sample_subset_offset)
  NODE_SOCKET_API(int, sample_subset_length)

  NODE_SOCKET_API(bool, use_light_tree)
  NODE_SOCKET_API(float, light_sampling_threshold)

  NODE_SOCKET_API(bool, use_adaptive_sampling)
  NODE_SOCKET_API(int, adaptive_min_samples)
  NODE_SOCKET_API(float, adaptive_threshold)

  NODE_SOCKET_API(SamplingPattern, sampling_pattern)
  NODE_SOCKET_API(float, scrambling_distance)

  NODE_SOCKET_API(bool, use_denoise);
  NODE_SOCKET_API(DenoiserType, denoiser_type);
  NODE_SOCKET_API(int, denoise_start_sample);
  NODE_SOCKET_API(bool, use_denoise_pass_albedo);
  NODE_SOCKET_API(bool, use_denoise_pass_normal);
  NODE_SOCKET_API(DenoiserPrefilter, denoiser_prefilter);
  NODE_SOCKET_API(bool, denoise_use_gpu);
  NODE_SOCKET_API(DenoiserQuality, denoiser_quality);

  enum : uint32_t {
    AO_PASS_MODIFIED = (1 << 0),
    OBJECT_MANAGER = (1 << 1),

    /* tag everything in the manager for an update */
    UPDATE_ALL = ~0u,

    UPDATE_NONE = 0u,
  };

  Integrator();
  ~Integrator() override;

  void device_update(Device *device, DeviceScene *dscene, Scene *scene);
  void device_free(Device *device, DeviceScene *dscene, bool force_free = false);

  void tag_update(Scene *scene, const uint32_t flag);

  uint get_kernel_features() const;

  AdaptiveSampling get_adaptive_sampling() const;
  DenoiseParams get_denoise_params() const;
  GuidingParams get_guiding_params(const Device *device) const;
};

CCL_NAMESPACE_END
