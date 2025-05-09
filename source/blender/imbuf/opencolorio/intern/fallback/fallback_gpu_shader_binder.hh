/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "OCIO_gpu_shader_binder.hh"

namespace blender::ocio {

class FallbackGPUShaderBinder : public GPUShaderBinder {
 protected:
  using GPUShaderBinder::GPUShaderBinder;

  void construct_display_shader(internal::GPUDisplayShader &display_shader) const override;
  void construct_scene_linear_shader(internal::GPUDisplayShader &display_shader) const override;
};

}  // namespace blender::ocio
