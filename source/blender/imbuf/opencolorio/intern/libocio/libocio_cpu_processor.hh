/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#if defined(WITH_OPENCOLORIO)

#  include "MEM_guardedalloc.h"

#  include "OCIO_cpu_processor.hh"

#  include "../opencolorio.hh"

namespace blender::ocio {

class LibOCIOCPUProcessor : public CPUProcessor {
  OCIO_NAMESPACE::ConstCPUProcessorRcPtr ocio_cpu_processor_;

 public:
  explicit LibOCIOCPUProcessor(const OCIO_NAMESPACE::ConstCPUProcessorRcPtr &ocio_cpu_processor);

  bool is_noop() const override
  {
    return false;
  }

  void apply_rgb(float rgb[3]) const override;
  void apply_rgba(float rgba[4]) const override;

  void apply_rgba_predivide(float rgba[4]) const override;

  void apply(const PackedImage &image) const override;
  void apply_predivide(const PackedImage &image) const override;

  MEM_CXX_CLASS_ALLOC_FUNCS("LibOCIOCPUProcessor");
};

}  // namespace blender::ocio

#endif
