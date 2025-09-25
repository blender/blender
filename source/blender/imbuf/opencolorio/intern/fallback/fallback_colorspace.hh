/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "OCIO_colorspace.hh"

#include "fallback_cpu_processor.hh"

namespace blender::ocio {

class FallbackColorSpace : public ColorSpace {
  std::string name_;

 public:
  enum class Type {
    LINEAR,
    SRGB,
    DATA,
  };

  FallbackColorSpace(const int index, const StringRefNull name, const Type type) : type_(type)
  {
    this->index = index;
    name_ = name;
  }

  StringRefNull name() const override
  {
    return name_;
  }
  StringRefNull description() const override
  {
    return "";
  }
  StringRefNull interop_id() const override
  {
    switch (type_) {
      case Type::LINEAR:
        return "lin_rec709_scene";
      case Type::SRGB:
        return "srgb_rec709_display";
      case Type::DATA:
        return "data";
    }

    return "";
  }

  bool is_invertible() const override
  {
    return true;
  }

  bool is_scene_linear() const override
  {
    return type_ == Type::LINEAR;
  }
  bool is_srgb() const override
  {
    return type_ == Type::SRGB;
  }

  bool is_data() const override
  {
    return type_ == Type::DATA;
  }

  bool is_display_referred() const override
  {
    return type_ == Type::SRGB;
  }

  const CPUProcessor *get_to_scene_linear_cpu_processor() const override
  {
    if (type_ == Type::SRGB) {
      static FallbackSRGBToLinearRGBCPUProcessor processor;
      return &processor;
    }

    static FallbackNOOPCPUProcessor processor;
    return &processor;
  }

  CPUProcessor *get_from_scene_linear_cpu_processor() const override
  {
    if (type_ == Type::SRGB) {
      static FallbackLinearRGBToSRGBCPUProcessor processor;
      return &processor;
    }

    static FallbackNOOPCPUProcessor processor;
    return &processor;
  }

 private:
  Type type_;
};

}  // namespace blender::ocio
