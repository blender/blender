/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include <memory>

#  include "MEM_guardedalloc.h"

#  include "BLI_vector.hh"

#  include "OCIO_display.hh"

#  include "../cpu_processor_cache.hh"

#  include "libocio_view.hh"

namespace blender::ocio {

class LibOCIOConfig;

class LibOCIODisplay : public Display {
  /* Store by pointer to allow move semantic.
   * In practice this must never be nullable. */
  const LibOCIOConfig *config_ = nullptr;

  StringRefNull name_;
  Vector<LibOCIOView> views_;

  CPUProcessorCache to_scene_linear_cpu_processor_;
  CPUProcessorCache from_scene_linear_cpu_processor_;

 public:
  LibOCIODisplay(int index, const LibOCIOConfig &config);
  LibOCIODisplay(const LibOCIODisplay &other) = delete;
  LibOCIODisplay(LibOCIODisplay &&other) noexcept = default;

  ~LibOCIODisplay() = default;

  LibOCIODisplay &operator=(const LibOCIODisplay &other) = delete;
  LibOCIODisplay &operator=(LibOCIODisplay &&other) = default;

  StringRefNull name() const override
  {
    return name_;
  }

  const View *get_default_view() const override
  {
    /* Matches the behavior of OpenColorIO, but avoids using API which potentially throws exception
     * and requires string lookups. */
    return get_view_by_index(0);
  }

  const View *get_view_by_name(StringRefNull name) const override;
  int get_num_views() const override;
  const View *get_view_by_index(int index) const override;

  const CPUProcessor *get_to_scene_linear_cpu_processor() const override;
  const CPUProcessor *get_from_scene_linear_cpu_processor() const override;

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOConfig");
};

}  // namespace blender::ocio

#endif
