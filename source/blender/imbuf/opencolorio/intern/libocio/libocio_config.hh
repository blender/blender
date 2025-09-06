/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "MEM_guardedalloc.h"

#  include "BLI_vector.hh"

#  include "OCIO_config.hh"

#  include "libocio_colorspace.hh"
#  include "libocio_display.hh"
#  include "libocio_gpu_shader_binder.hh"
#  include "libocio_look.hh"

#  include "../opencolorio.hh"

namespace blender::ocio {

class ColorSpace;
class Display;
class View;

class LibOCIOConfig : public Config {
  OCIO_NAMESPACE::ConstConfigRcPtr ocio_config_;

  /* Storage of for Blender-side representation of OpenColorIO configuration.
   * Note that the color spaces correspond to color spaces from OpenColorIO configuration: this
   * array does not contain aliases or roles. If role or alias is to be resolved OpenColorIO is to
   * be used first to provide color space name which then can be looked up in this array. */
  Vector<LibOCIOColorSpace> color_spaces_;
  Vector<LibOCIOColorSpace> inactive_color_spaces_;
  Vector<LibOCIOLook> looks_;
  Vector<LibOCIODisplay> displays_;

  /* Array with indices into color_spaces_.
   * color_spaces_[sorted_color_space_index_[i]] provides alphabetically sorted access. */
  Vector<int> sorted_color_space_index_;

  LibOCIOGPUShaderBinder gpu_shader_binder_{*this};

 public:
  ~LibOCIOConfig();

  static std::unique_ptr<Config> create_from_environment();
  static std::unique_ptr<Config> create_from_file(StringRefNull filename);

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

  /* Integration with the OpenColorIO specific routines. */
  const OCIO_NAMESPACE::ConstConfigRcPtr &get_ocio_config() const
  {
    return ocio_config_;
  }

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOConfig");

 private:
  explicit LibOCIOConfig(const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config);

  /* Initialize BLender-side representation of color spaces, displays, etc. from the current
   * OpenColorIO configuration. */
  void initialize_active_color_spaces();
  void initialize_inactive_color_spaces();
  void initialize_hdr_color_spaces();
  void initialize_looks();
  void initialize_displays();
};

}  // namespace blender::ocio

#endif
