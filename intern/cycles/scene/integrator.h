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

#ifndef __INTEGRATOR_H__
#define __INTEGRATOR_H__

#include "kernel/types.h"

#include "device/denoise.h" /* For the parameters and type enum. */
#include "graph/node.h"
#include "integrator/adaptive_sampling.h"

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

  NODE_SOCKET_API(int, transparent_min_bounce)
  NODE_SOCKET_API(int, transparent_max_bounce)

  NODE_SOCKET_API(int, ao_bounces)
  NODE_SOCKET_API(float, ao_factor)
  NODE_SOCKET_API(float, ao_distance)
  NODE_SOCKET_API(float, ao_additive_factor)

  NODE_SOCKET_API(int, volume_max_steps)
  NODE_SOCKET_API(float, volume_step_rate)

  NODE_SOCKET_API(bool, caustics_reflective)
  NODE_SOCKET_API(bool, caustics_refractive)
  NODE_SOCKET_API(float, filter_glossy)

  NODE_SOCKET_API(int, seed)

  NODE_SOCKET_API(float, sample_clamp_direct)
  NODE_SOCKET_API(float, sample_clamp_indirect)
  NODE_SOCKET_API(bool, motion_blur)

  /* Maximum number of samples, beyond which we are likely to run into
   * precision issues for sampling patterns. */
  static const int MAX_SAMPLES = (1 << 24);

  NODE_SOCKET_API(int, aa_samples)
  NODE_SOCKET_API(int, start_sample)

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

  enum : uint32_t {
    AO_PASS_MODIFIED = (1 << 0),
    OBJECT_MANAGER = (1 << 1),

    /* tag everything in the manager for an update */
    UPDATE_ALL = ~0u,

    UPDATE_NONE = 0u,
  };

  Integrator();
  ~Integrator();

  void device_update(Device *device, DeviceScene *dscene, Scene *scene);
  void device_free(Device *device, DeviceScene *dscene, bool force_free = false);

  void tag_update(Scene *scene, uint32_t flag);

  uint get_kernel_features() const;

  AdaptiveSampling get_adaptive_sampling() const;
  DenoiseParams get_denoise_params() const;
};

CCL_NAMESPACE_END

#endif /* __INTEGRATOR_H__ */
