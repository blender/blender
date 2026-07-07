/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "OCIO_display.hh"

#include "fallback_cpu_processor.hh"
#include "fallback_default_view.hh"

namespace blender::ocio {

class ColorSpace;

class FallbackDefaultDisplay : public Display {
  std::string name_;
  FallbackDefaultView default_view_;

 public:
  FallbackDefaultDisplay(const ColorSpace *display_colorspace) : default_view_(display_colorspace)
  {
    this->index = 0;
    name_ = "sRGB";
  }

  StringRefNull name() const override
  {
    return name_;
  }

  StringRefNull ui_name() const override
  {
    return name();
  }

  StringRefNull description() const override
  {
    return "";
  }

  const View *get_default_view() const override
  {
    return &default_view_;
  }

  const View *get_untonemapped_view() const override
  {
    return &default_view_;
  }

  const View *get_view_by_name(const StringRefNull name) const override
  {
    if (name == default_view_.name()) {
      return &default_view_;
    }
    return nullptr;
  }

  int get_num_views() const override
  {
    return 1;
  }

  const View *get_view_by_index(const int index) const override
  {
    if (index != 0) {
      return nullptr;
    }
    return &default_view_;
  }

  const CPUProcessor *get_to_scene_linear_cpu_processor(
      bool /*use_display_emulation*/) const override
  {
    static FallbackSRGBToLinearRGBCPUProcessor processor;
    return &processor;
  }

  const CPUProcessor *get_from_scene_linear_cpu_processor(
      bool /*use_display_emulation*/) const override
  {
    static FallbackLinearRGBToSRGBCPUProcessor processor;
    return &processor;
  }

  bool is_hdr() const override
  {
    return false;
  }
};

}  // namespace blender::ocio
