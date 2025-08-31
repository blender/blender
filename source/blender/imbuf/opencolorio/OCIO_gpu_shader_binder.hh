/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Helper class which takes care of GPU shader bindings used to convert color spaces as a fragments
 * shader. It defines public API to bind various color space conversion shaders, and it also takes
 * care of shader caching to avoid re-compilations.
 *
 * Implementation-wise it takes care of common steps needed to compile the display transform shader
 * (gpu_shader_display_transform_frag.glsl and gpu_shader_display_transform_vert.glsl). Subclasses
 * takes care of generation code for functions OCIO_to_scene_linear() and OCIO_to_display().
 */

#pragma once

#include <memory>
#include <string>

#include "BLI_array.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"

struct CurveMapping;
namespace blender::gpu {
class Shader;
}  // namespace blender::gpu

namespace blender::ocio {

class Config;

namespace internal {
class GPUDisplayShader;
class GPUShaderCache;
}  // namespace internal

struct GPUDisplayParameters {
  /* Convert from a colorspace to a display, using the view transform and look. */
  StringRefNull from_colorspace;
  StringRefNull view;
  StringRefNull display;
  StringRefNull look;
  /* Artistic controls. */
  CurveMapping *curve_mapping = nullptr;
  float scale = 1.0f;
  float exponent = 1.0f;
  float dither = 0.0f;
  float temperature = 6500.0f;
  float tint = 10.0f;
  bool use_white_balance = false;
  /* Divide RGB by alpha before performing the transform. */
  bool use_predivide = false;
  /* Composite an overlay buffer on top of the image. */
  bool do_overlay_merge = false;
  /* Writing to a HDR buffer. */
  bool use_hdr_buffer = false;
  /* Chosen display is a HDR display. */
  bool use_hdr_display = false;
  /* Rather than outputting colors for the specified display, output extended
   * sRGB colors emulating the specified display. */
  bool use_display_emulation = false;
};

class GPUShaderBinder {
  /* Cache of shaders used for conversion to the display space. */
  std::unique_ptr<internal::GPUShaderCache> display_cache_;

  /* Cache of shaders used for conversion to scene linear space. */
  std::unique_ptr<internal::GPUShaderCache> scene_linear_cache_;

 public:
  explicit GPUShaderBinder(const Config &config);
  virtual ~GPUShaderBinder();

  /**
   * Bind GPU shader which performs conversion from the given color space to the display space.
   * Drawing happens in the same immediate mode as when GPU_SHADER_3D_IMAGE_COLOR shader is used.
   *
   * Returns true if the GPU shader was successfully bound.
   */
  bool display_bind(const GPUDisplayParameters &display_parameters) const;

  /**
   * Configures and bind GPU shader for conversion from the given space to scene linear.
   * Drawing happens in the same immediate mode as when GPU_SHADER_3D_IMAGE_COLOR shader is used.
   *
   * Returns true if the GPU shader was successfully bound.
   */
  bool to_scene_linear_bind(StringRefNull from_colorspace, bool use_predivide) const;

  /**
   * Unbind previously bound GPU shader.
   *
   * If the shader was not bound by neither display_bind() nor
   * to_scene_linear_bind() the behavior is undefined.
   */
  void unbind() const;

  /**
   * Clear caches when configuration changes.
   */
  void clear_caches() const;

 protected:
  const Config &config_;

  /**
   * Construct display shader matching requested parameters.
   * The shader has its cache variables (input color space name, view, display, look, whether
   * curve mapping is used or not).
   */
  virtual void construct_display_shader(internal::GPUDisplayShader &display_shader) const = 0;

  /**
   * Construct display shader which will only perform the to-scene-linear part of conversion,
   * leaving the to-display a no-op function.
   */
  virtual void construct_scene_linear_shader(internal::GPUDisplayShader &display_shader) const = 0;

  /**
   * Create GPU shader for the given display shader.
   * Returns true if the shader was successfully created.
   */
  static bool create_gpu_shader(internal::GPUDisplayShader &display_shader,
                                StringRefNull fragment_source,
                                Span<std::array<StringRefNull, 2>> additional_defines);
};

}  // namespace blender::ocio
