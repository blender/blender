/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_string_ref.hh"

namespace blender::ocio {

class ColorSpace;
class Display;
class Look;
class CPUProcessor;
class GPUShaderBinder;

struct DisplayParameters {
  /* Convert from a colorspace to a display, using the view transform and look. */
  StringRefNull from_colorspace;
  StringRefNull view;
  StringRefNull display;
  StringRefNull look;
  /* Artistic controls. */
  float scale = 1.0f;
  float exponent = 1.0f;
  float temperature = 6500.0f;
  float tint = 10.0f;
  bool use_white_balance = false;
  /* Writing to a HDR windows buffer. */
  bool use_hdr_buffer = false;
  /* Chosen display is HDR. */
  bool use_hdr_display = false;
  /* Display transform is being used for image output. */
  bool is_image_output = false;
  /* Rather than outputting colors for the specified display, output extended
   * sRGB colors emulating the specified display. */
  bool use_display_emulation = false;
  /* Invert the entire transform. */
  bool inverse = false;
};

class Config {
 public:
  /* -------------------------------------------------------------------- */
  /** \name Construction
   * \{ */

  virtual ~Config() = default;

  /**
   * Create OpenColorIO configuration using configuration from the environment variables.
   * If there is an error creating the configuration nullptr is returned.
   */
  static std::unique_ptr<Config> create_from_environment();

  /**
   * Create OpenColorIO configuration using configuration from the given configuration file.
   * If there is an error creating the configuration nullptr is returned.
   */
  static std::unique_ptr<Config> create_from_file(StringRefNull filename);

  /**
   * Create fallback implementation which is always guaranteed to work.
   *
   * It is used in cases actual OpenColorIO configuration has failed to be created so that Blender
   * interface can be displayed.
   *
   * The fallback implementation is also used implicitly when BLender is compiled without
   * OpenColorIO support.
   */
  static std::unique_ptr<Config> create_fallback();

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Color space information
   * \{ */

  /**
   * Get the default coefficients for computing luma.
   */
  virtual float3 get_default_luma_coefs() const = 0;

  /**
   * Get conversion matrix from XYZ space to the scene linear.
   * TODO(sergey): Specialize which exactly XYZ space it is.
   */
  virtual float3x3 get_xyz_to_scene_linear_matrix() const = 0;

  /**
   * Get the color space of the first rule that matched filepath.
   * If there is no such color space nullptr is returned.
   */
  virtual const char *get_color_space_from_filepath(const char *filepath) const = 0;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Color space API
   * \{ */

  /**
   * Get color space with the given name, role name, or alias. Color space names take precedence
   * over roles.
   * If the color space does not exist nullptr is returned.
   */
  virtual const ColorSpace *get_color_space(StringRefNull name) const = 0;

  /**
   * Get the number of color spaces in this configuration.
   */
  virtual int get_num_color_spaces() const = 0;

  /**
   * Get color space with the given index within the configuration.
   * If the index is invalid nullptr is returned.
   */
  virtual const ColorSpace *get_color_space_by_index(int index) const = 0;

  /**
   * Get color space with the given index within the sorted array.
   * This function allows to iterate color spaces in their alphabetical order.
   *
   * If the index is invalid nullptr is returned.
   */
  virtual const ColorSpace *get_sorted_color_space_by_index(int index) const = 0;

  /**
   * Get color space for the given interop ID.
   * If not found a nullptr is returned.
   */
  virtual const ColorSpace *get_color_space_by_interop_id(StringRefNull interop_id) const = 0;

  /**
   * Get colorspace to be used for saving and loading HDR image files, which
   * may need adjustments compared to the colorspace as chosen by the user.
   **/
  virtual const ColorSpace *get_color_space_for_hdr_image(StringRefNull name) const = 0;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Working colorspace API
   * \{ */

  virtual void set_scene_linear_role(StringRefNull name) = 0;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Display API
   * \{ */

  /**
   * Get the default display in this configuration.
   */
  virtual const Display *get_default_display() const = 0;

  /**
   * Get display with the given name.
   * If the display does not exist nullptr is returned.
   */
  virtual const Display *get_display_by_name(StringRefNull name) const = 0;

  /**
   * Get the number of displays in this configuration.
   */
  virtual int get_num_displays() const = 0;

  /**
   * Get display with the given index within the configuration.
   * If the index is invalid nullptr is returned.
   */
  virtual const Display *get_display_by_index(int index) const = 0;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Display colorspace API
   * \{ */

  /**
   * Returns the colorspace of the (display, view) pair.
   * Note that this may be either a color space or a display color space.
   */
  virtual const ColorSpace *get_display_view_color_space(StringRefNull display,
                                                         StringRefNull view) const = 0;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Look API
   * \{ */

  /**
   * Get look with the given name.
   * If the look does not exist nullptr is returned.
   */
  virtual const Look *get_look_by_name(StringRefNull name) const = 0;

  /**
   * Get the number of looks in this configuration.
   */
  virtual int get_num_looks() const = 0;

  /**
   * Get look with the given index within the configuration.
   * If the index is invalid nullptr is returned.
   */
  virtual const Look *get_look_by_index(int index) const = 0;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name Processor API
   * \{ */

  /**
   * Get processor which converts color space from the given from_colorspace to the display
   * space.
   */
  virtual std::shared_ptr<const CPUProcessor> get_display_cpu_processor(
      const DisplayParameters &display_parameters) const = 0;

  /**
   * Get processor which converts color between given color spaces.
   */
  virtual std::shared_ptr<const CPUProcessor> get_cpu_processor(
      StringRefNull from_colorspace, StringRefNull to_colorspace) const = 0;

  /** \} */

  /* -------------------------------------------------------------------- */
  /** \name GPU-side processing
   * \{ */

  /**
   * Get API which can be used to bind GPU shaders for color space conversion.
   */
  virtual const GPUShaderBinder &get_gpu_shader_binder() const = 0;

  /** \} */
};

}  // namespace blender::ocio
