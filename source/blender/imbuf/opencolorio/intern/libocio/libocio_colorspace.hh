/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_set.hh"

#include "OCIO_colorspace.hh"

#include "../cpu_processor_cache.hh"
#include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOColorSpace : public ColorSpace {
  OCIO_NAMESPACE::ConstConfigRcPtr ocio_config_;
  OCIO_NAMESPACE::ConstColorSpaceRcPtr ocio_color_space_;

  std::string clean_description_;
  std::string family_;
  StringRefNull interop_id_;
  bool is_primary_interop_id_ = false;
  std::string alternate_interop_id_;

  CPUProcessorCache to_scene_linear_cpu_processor_;
  CPUProcessorCache from_scene_linear_cpu_processor_;

  /* Configuration whose scene linear role is the target of the to/from scene linear processors.
   * Usually this is #ocio_config_, but can also be something else if the color space is not
   * part of a new config but still preserved. */
  OCIO_NAMESPACE::ConstConfigRcPtr scene_linear_config_;

  void initialize_alternate_interop_id();

 public:
  LibOCIOColorSpace(int index,
                    const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config,
                    const OCIO_NAMESPACE::ConstColorSpaceRcPtr &ocio_color_space,
                    Set<StringRef> &primary_interop_ids);

  StringRefNull name() const override
  {
    /* TODO(sergey): Avoid construction StringRefNull on every call? */
    return ocio_color_space_->getName();
  }
  StringRefNull description() const override
  {
    return clean_description_;
  }
  StringRefNull family() const override
  {
    return family_;
  }

  StringRefNull interop_id() const override
  {
    return interop_id_;
  }
  bool is_primary_interop_id() const override;
  StringRefNull alternate_interop_id() const override
  {
    return alternate_interop_id_;
  }

  std::string icc_profile_path() const override;

  bool is_scene_linear() const override;
  bool is_srgb() const override;

  bool is_data() const override
  {
    return ocio_color_space_->isData();
  }

  bool is_display_referred() const override
  {
    return ocio_color_space_->getReferenceSpaceType() == OCIO_NAMESPACE::REFERENCE_SPACE_DISPLAY;
  }

  const CPUProcessor *get_to_scene_linear_cpu_processor() const override;
  const CPUProcessor *get_from_scene_linear_cpu_processor() const override;

  void switch_scene_linear_config(const OCIO_NAMESPACE::ConstConfigRcPtr &ocio_config);

  const OCIO_NAMESPACE::ConstColorSpaceRcPtr &ocio_color_space() const
  {
    return ocio_color_space_;
  }

  void clear_caches();

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOColorSpace");
};

}  // namespace blender::ocio
