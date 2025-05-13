/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "MEM_guardedalloc.h"

#  include "OCIO_gpu_shader_binder.hh"

#  include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOGPUShaderBinder : public GPUShaderBinder {
 public:
  using GPUShaderBinder::GPUShaderBinder;

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOGPUShaderBinder");

 private:
  void construct_shader_for_processors(
      internal::GPUDisplayShader &display_shader,
      const OCIO_NAMESPACE::ConstProcessorRcPtr &processor_to_scene_linear,
      const OCIO_NAMESPACE::ConstProcessorRcPtr &processor_to_display,
      Span<std::array<StringRefNull, 2>> additional_defines) const;

 protected:
  void construct_display_shader(internal::GPUDisplayShader &display_shader) const override;
  void construct_scene_linear_shader(internal::GPUDisplayShader &display_shader) const override;
};

}  // namespace blender::ocio

#endif
