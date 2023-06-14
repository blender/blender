/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "device/cpu/kernel.h"

#include "integrator/pass_accessor.h"

CCL_NAMESPACE_BEGIN

struct KernelFilmConvert;

/* Pass accessor implementation for CPU side. */
class PassAccessorCPU : public PassAccessor {
 public:
  using PassAccessor::PassAccessor;

 protected:
  inline void run_get_pass_kernel_processor_float(
      const KernelFilmConvert *kfilm_convert,
      const RenderBuffers *render_buffers,
      const BufferParams &buffer_params,
      const Destination &destination,
      const CPUKernels::FilmConvertFunction func) const;

  inline void run_get_pass_kernel_processor_half_rgba(
      const KernelFilmConvert *kfilm_convert,
      const RenderBuffers *render_buffers,
      const BufferParams &buffer_params,
      const Destination &destination,
      const CPUKernels::FilmConvertHalfRGBAFunction func) const;

#define DECLARE_PASS_ACCESSOR(pass) \
  virtual void get_pass_##pass(const RenderBuffers *render_buffers, \
                               const BufferParams &buffer_params, \
                               const Destination &destination) const override;

  /* Float (scalar) passes. */
  DECLARE_PASS_ACCESSOR(depth)
  DECLARE_PASS_ACCESSOR(mist)
  DECLARE_PASS_ACCESSOR(sample_count)
  DECLARE_PASS_ACCESSOR(float)

  /* Float3 passes. */
  DECLARE_PASS_ACCESSOR(light_path)
  DECLARE_PASS_ACCESSOR(shadow_catcher)
  DECLARE_PASS_ACCESSOR(float3)

  /* Float4 passes. */
  DECLARE_PASS_ACCESSOR(motion)
  DECLARE_PASS_ACCESSOR(cryptomatte)
  DECLARE_PASS_ACCESSOR(shadow_catcher_matte_with_shadow)
  DECLARE_PASS_ACCESSOR(combined)
  DECLARE_PASS_ACCESSOR(float4)

#undef DECLARE_PASS_ACCESSOR
};

CCL_NAMESPACE_END
