/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

#include "BLI_string_ref.hh"

namespace blender::ocio {

class CPUProcessor;

class FallbackProcessorCache {
 public:
  /**
   * Get processor to convert color space.
   */
  std::shared_ptr<const CPUProcessor> get(StringRefNull from_colorspace,
                                          StringRefNull to_colorspace) const;
};

}  // namespace blender::ocio
