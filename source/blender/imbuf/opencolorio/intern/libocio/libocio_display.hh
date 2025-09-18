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
class LibOCIOCPUProcessor;

class LibOCIODisplay : public Display {
  /* Store by pointer to allow move semantic.
   * In practice this must never be nullable. */
  const LibOCIOConfig *config_ = nullptr;

  StringRefNull name_;
  std::string ui_name_;
  StringRefNull description_;
  Vector<LibOCIOView> views_;
  const LibOCIOView *untonemapped_view_ = nullptr;
  bool is_hdr_ = false;

  CPUProcessorCache to_scene_linear_cpu_processor_;
  CPUProcessorCache to_scene_linear_emulation_cpu_processor_;
  CPUProcessorCache from_scene_linear_cpu_processor_;
  CPUProcessorCache from_scene_linear_emulation_cpu_processor_;

 public:
  LibOCIODisplay(int index, const LibOCIOConfig &config);
  LibOCIODisplay(const LibOCIODisplay &other) = delete;
  LibOCIODisplay(LibOCIODisplay &&other) noexcept = default;

  ~LibOCIODisplay() override = default;

  LibOCIODisplay &operator=(const LibOCIODisplay &other) = delete;
  LibOCIODisplay &operator=(LibOCIODisplay &&other) = default;

  StringRefNull name() const override
  {
    return name_;
  }

  StringRefNull ui_name() const override
  {
    return (ui_name_.empty()) ? name_ : ui_name_.c_str();
  }

  StringRefNull description() const override
  {
    return description_;
  }

  const View *get_default_view() const override
  {
    /* Matches the behavior of OpenColorIO, but avoids using API which potentially throws exception
     * and requires string lookups. */
    return get_view_by_index(0);
  }

  const View *get_untonemapped_view() const override;

  const View *get_view_by_name(StringRefNull name) const override;
  int get_num_views() const override;
  const View *get_view_by_index(int index) const override;

  const CPUProcessor *get_to_scene_linear_cpu_processor(bool use_display_emulation) const override;
  const CPUProcessor *get_from_scene_linear_cpu_processor(
      bool use_display_emulation) const override;

  bool is_hdr() const override
  {
    return is_hdr_;
  }

  void clear_caches();

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOConfig");

 protected:
  std::unique_ptr<LibOCIOCPUProcessor> create_scene_linear_cpu_processor(
      const bool use_display_emulation, const bool inverse) const;
};

}  // namespace blender::ocio

#endif
