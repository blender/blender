/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "MEM_guardedalloc.h"

#include "BLI_vector.hh"

#include "OCIO_config.hh"

#include "fallback_colorspace.hh"
#include "fallback_default_display.hh"
#include "fallback_default_look.hh"
#include "fallback_gpu_shader_binder.hh"
#include "fallback_processor_cache.hh"

namespace blender::ocio {

class ColorSpace;
class Display;
class View;

class FallbackConfig : public Config {
  /* Color spaces in this configuration. */
  FallbackColorSpace colorspace_linear_{0, "Linear", FallbackColorSpace::Type::LINEAR};
  FallbackColorSpace colorspace_data_{1, "Non-Color", FallbackColorSpace::Type::DATA};
  FallbackColorSpace colorspace_srgb_{2, "sRGB", FallbackColorSpace::Type::SRGB};

  FallbackDefaultDisplay default_display_{&colorspace_srgb_};
  FallbackDefaultLook default_look_;

  /* Vectors that contain non-owning pointers to the color spaces and display. */
  Vector<const ColorSpace *> color_spaces_{
      &colorspace_linear_, &colorspace_data_, &colorspace_srgb_};

  FallbackProcessorCache processor_cache_;
  FallbackGPUShaderBinder gpu_shader_binder_{*this};

 public:
  /* Color space information. */
  float3 get_default_luma_coefs() const override;
  float3x3 get_xyz_to_scene_linear_matrix() const override;
  const char *get_color_space_from_filepath(const char *filepath) const override;

  /* Color space API. */
  const ColorSpace *get_color_space(StringRefNull name) const override;
  int get_num_color_spaces() const override;
  const ColorSpace *get_color_space_by_index(int index) const override;
  const ColorSpace *get_sorted_color_space_by_index(int index) const override;
  const ColorSpace *get_color_space_by_interop_id(StringRefNull interop_id) const override;
  const ColorSpace *get_color_space_for_hdr_image(StringRefNull name) const override;

  /* Working space API. */
  void set_scene_linear_role(StringRefNull name) override;

  /* Display API. */
  const Display *get_default_display() const override;
  const Display *get_display_by_name(StringRefNull name) const override;
  int get_num_displays() const override;
  const Display *get_display_by_index(int index) const override;

  /* Display colorspace API. */
  const ColorSpace *get_display_view_color_space(StringRefNull display,
                                                 StringRefNull view) const override;

  /* Look API. */
  const Look *get_look_by_name(StringRefNull name) const override;
  int get_num_looks() const override;
  const Look *get_look_by_index(int index) const override;

  /* Processor API. */
  std::shared_ptr<const CPUProcessor> get_display_cpu_processor(
      const DisplayParameters &display_parameters) const override;
  std::shared_ptr<const CPUProcessor> get_cpu_processor(
      StringRefNull from_colorspace, StringRefNull to_colorspace) const override;

  /* Processor API. */
  const GPUShaderBinder &get_gpu_shader_binder() const override;

  MEM_CXX_CLASS_ALLOC_FUNCS("FallbackConfig");
};

}  // namespace blender::ocio
