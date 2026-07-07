/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "fallback_config.hh"

#include "OCIO_display.hh"
#include "OCIO_matrix.hh"

#include "fallback_colorspace.hh"
#include "fallback_display_cpu_processor.hh"

namespace blender::ocio {
/* -------------------------------------------------------------------- */
/** \name Color space information
 * \{ */

float3 FallbackConfig::get_default_luma_coefs() const
{
  /* Here we simply use the older Blender assumed primaries of ITU-BT.709 / sRGB, or
   * 0.2126729 0.7151522 0.0721750. Brute force stupid, but only plausible option given no color
   * management system in place. */

  return float3(0.2126f, 0.7152f, 0.0722f);
}

float3x3 FallbackConfig::get_xyz_to_scene_linear_matrix() const
{
  /* Default to ITU-BT.709. */
  return XYZ_TO_REC709;
}

const char *FallbackConfig::get_color_space_from_filepath(const char * /*filepath*/) const
{
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Color space API
 * \{ */

const ColorSpace *FallbackConfig::get_color_space(const StringRefNull name) const
{
  for (const ColorSpace *color_space : color_spaces_) {
    if (color_space->name() == name) {
      return color_space;
    }
  }

  if (name == "scene_linear") {
    return &colorspace_linear_;
  }
  if (name == "color_picking") {
    return &colorspace_srgb_;
  }
  if (name == "texture_paint") {
    return &colorspace_linear_;
  }
  if (name == "default_byte") {
    return &colorspace_srgb_;
  }
  if (name == "default_float") {
    return &colorspace_linear_;
  }
  if (name == "default_sequencer") {
    return &colorspace_srgb_;
  }
  if (name == "Linear") {
    return &colorspace_linear_;
  }
  if (name == "sRGB") {
    return &colorspace_srgb_;
  }
  if (name == "data") {
    return &colorspace_data_;
  }

  return nullptr;
}

int FallbackConfig::get_num_color_spaces() const
{
  return color_spaces_.size();
}

const ColorSpace *FallbackConfig::get_color_space_by_index(const int index) const
{
  if (index < 0 || index >= color_spaces_.size()) {
    return nullptr;
  }
  return color_spaces_[index];
}

const ColorSpace *FallbackConfig::get_sorted_color_space_by_index(const int index) const
{
  return get_color_space_by_index(index);
}

const ColorSpace *FallbackConfig::get_color_space_by_interop_id(StringRefNull interop_id) const
{
  if (interop_id == "lin_rec709_scene") {
    return &colorspace_linear_;
  }
  if (interop_id == "srgb_rec709_display") {
    return &colorspace_srgb_;
  }
  if (interop_id == "data") {
    return &colorspace_data_;
  }

  return nullptr;
}

const ColorSpace *FallbackConfig::get_color_space_for_hdr_image(StringRefNull name) const
{
  return get_color_space(name);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Working space API
 * \{ */

void FallbackConfig::set_scene_linear_role(StringRefNull /*name*/) {}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display API
 * \{ */

const Display *FallbackConfig::get_default_display() const
{
  return &default_display_;
}

const Display *FallbackConfig::get_display_by_name(const StringRefNull name) const
{
  if (name == default_display_.name()) {
    return &default_display_;
  }
  return nullptr;
}

int FallbackConfig::get_num_displays() const
{
  return 1;
}

const Display *FallbackConfig::get_display_by_index(const int index) const
{
  if (index != 0) {
    return nullptr;
  }
  return &default_display_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Display colorspace API
 * \{ */

const ColorSpace *FallbackConfig::get_display_view_color_space(const StringRefNull display,
                                                               const StringRefNull view) const
{
  if (display == default_display_.name() && view == default_display_.get_default_view()->name()) {
    return &colorspace_srgb_;
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Look API
 * \{ */

const Look *FallbackConfig::get_look_by_name(const StringRefNull name) const
{
  if (name == default_look_.name()) {
    return &default_look_;
  }
  return nullptr;
}

int FallbackConfig::get_num_looks() const
{
  return 1;
}

const Look *FallbackConfig::get_look_by_index(int index) const
{
  if (index != 0) {
    return nullptr;
  }
  return &default_look_;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Processor API
 * \{ */

std::shared_ptr<const CPUProcessor> FallbackConfig::get_display_cpu_processor(
    const DisplayParameters &display_parameters) const
{
  return create_fallback_display_cpu_processor(*this, display_parameters);
}

std::shared_ptr<const CPUProcessor> FallbackConfig::get_cpu_processor(
    const StringRefNull from_colorspace, const StringRefNull to_colorspace) const
{
  return processor_cache_.get(from_colorspace, to_colorspace);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Processor API
 * \{ */

const GPUShaderBinder &FallbackConfig::get_gpu_shader_binder() const
{
  return gpu_shader_binder_;
}

/** \} */

}  // namespace blender::ocio
