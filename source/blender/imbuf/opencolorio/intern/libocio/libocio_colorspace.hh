/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include <string>

#  include "MEM_guardedalloc.h"

#  include "OCIO_colorspace.hh"

#  include "../cpu_processor_cache.hh"
#  include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOColorSpace : public ColorSpace {
  OCIO_NAMESPACE::ConstConfigRcPtr ocio_config_;
  OCIO_NAMESPACE::ConstColorSpaceRcPtr ocio_color_space_;

  std::string clean_description_;
  bool is_inveetible_ = false;

  bool is_scene_linear_ = false;
  bool is_srgb_ = false;

  CPUProcessorCache to_scene_linear_cpu_processor_;
  CPUProcessorCache from_scene_linear_cpu_processor_;

 public:
  LibOCIOColorSpace(int index,
                    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
                    const OCIO_NAMESPACE::ConstColorSpaceRcPtr &ocio_color_space);

  StringRefNull name() const override
  {
    /* TODO(sergey): Avoid construction StringRefNull on every call? */
    return ocio_color_space_->getName();
  }
  StringRefNull description() const override
  {
    return clean_description_;
  }

  bool is_invertible() const override
  {
    return is_inveetible_;
  }

  bool is_scene_linear() const override
  {
    return is_scene_linear_;
  }
  bool is_srgb() const override
  {
    return is_srgb_;
  }

  bool is_data() const override
  {
    return ocio_color_space_->isData();
  }

  const CPUProcessor *get_to_scene_linear_cpu_processor() const override;
  const CPUProcessor *get_from_scene_linear_cpu_processor() const override;

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOColorSpace");
};

}  // namespace blender::ocio

#endif
