/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "fallback_processor_cache.hh"

#include "fallback_cpu_processor.hh"

namespace blender::ocio {

std::shared_ptr<const CPUProcessor> FallbackProcessorCache::get(
    const StringRefNull from_colorspace, const StringRefNull to_colorspace) const
{
  if (from_colorspace == to_colorspace) {
    static auto noop_cpu_processor = std::make_shared<FallbackNOOPCPUProcessor>();
    return noop_cpu_processor;
  }

  if (from_colorspace == "sRGB" && to_colorspace == "Linear") {
    static auto srgb_to_linear_cpu_processor =
        std::make_shared<FallbackSRGBToLinearRGBCPUProcessor>();
    return srgb_to_linear_cpu_processor;
  }

  if (from_colorspace == "Linear" && to_colorspace == "sRGB") {
    static auto linear_to_srgb_cpu_processor =
        std::make_shared<FallbackLinearRGBToSRGBCPUProcessor>();
    return linear_to_srgb_cpu_processor;
  }

  return nullptr;
}

}  // namespace blender::ocio
