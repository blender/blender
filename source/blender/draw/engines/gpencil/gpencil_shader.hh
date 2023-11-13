/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "DRW_render.h"

namespace blender::draw::greasepencil {

enum eShaderType {
  /* SMAA anti-aliasing. */
  ANTIALIASING_EDGE_DETECT = 0,
  ANTIALIASING_BLEND_WEIGHT,
  ANTIALIASING_RESOLVE,
  /* GPencil Object rendering */
  GREASE_PENCIL,
  /* All layer blend types in one shader! */
  LAYER_BLEND,
  /* Merge the final object depth to the depth buffer. */
  DEPTH_MERGE,
  /* Invert the content of the mask buffer. */
  MASK_INVERT,
  /* Final Compositing over rendered background. */
  FX_COMPOSITE,
  /* Effects. */
  FX_COLORIZE,
  FX_BLUR,
  FX_GLOW,
  FX_PIXEL,
  FX_RIM,
  FX_SHADOW,
  FX_TRANSFORM,

  MAX_SHADER_TYPE,
};

/**
 * Shader module. shared between instances.
 */
class ShaderModule {
 private:
  std::array<GPUShader *, MAX_SHADER_TYPE> shaders_;

  /** Shared shader module across all engine instances. */
  static ShaderModule *g_shader_module;

 public:
  ShaderModule();
  ~ShaderModule();

  GPUShader *static_shader_get(eShaderType shader_type);

  /** Only to be used by Instance constructor. */
  static ShaderModule *module_get();
  static void module_free();

 private:
  const char *static_shader_create_info_name_get(eShaderType shader_type);
};

}  // namespace blender::draw::greasepencil
