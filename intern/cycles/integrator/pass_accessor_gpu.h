/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "integrator/pass_accessor.h"
#include "kernel/types.h"

CCL_NAMESPACE_BEGIN

class DeviceQueue;

/* Pass accessor implementation for GPU side. */
class PassAccessorGPU : public PassAccessor {
 public:
  PassAccessorGPU(DeviceQueue *queue,
                  const PassAccessInfo &pass_access_info,
                  float exposure,
                  int num_samples);

 protected:
  void run_film_convert_kernels(DeviceKernel kernel,
                                const RenderBuffers *render_buffers,
                                const BufferParams &buffer_params,
                                const Destination &destination) const;

#define DECLARE_PASS_ACCESSOR(pass) \
  virtual void get_pass_##pass(const RenderBuffers *render_buffers, \
                               const BufferParams &buffer_params, \
                               const Destination &destination) const override;

  /* Float (scalar) passes. */
  DECLARE_PASS_ACCESSOR(depth);
  DECLARE_PASS_ACCESSOR(mist);
  DECLARE_PASS_ACCESSOR(sample_count);
  DECLARE_PASS_ACCESSOR(float);

  /* Float3 passes. */
  DECLARE_PASS_ACCESSOR(light_path);
  DECLARE_PASS_ACCESSOR(float3);

  /* Float4 passes. */
  DECLARE_PASS_ACCESSOR(motion);
  DECLARE_PASS_ACCESSOR(cryptomatte);
  DECLARE_PASS_ACCESSOR(shadow_catcher);
  DECLARE_PASS_ACCESSOR(shadow_catcher_matte_with_shadow);
  DECLARE_PASS_ACCESSOR(combined);
  DECLARE_PASS_ACCESSOR(float4);

#undef DECLARE_PASS_ACCESSOR

  DeviceQueue *queue_;
};

CCL_NAMESPACE_END
