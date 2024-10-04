/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "vk_sampler.hh"
#include "vk_samplers.hh"

#include "BLI_map.hh"

namespace blender::gpu {

/**
 * Collection of samplers.
 *
 * Samplers are device owned and can be shared between contexts.
 */
class VKSamplers : NonCopyable {
  VKSampler sampler_cache_[GPU_SAMPLER_EXTEND_MODES_COUNT][GPU_SAMPLER_EXTEND_MODES_COUNT]
                          [GPU_SAMPLER_FILTERING_TYPES_COUNT];
  VKSampler custom_sampler_cache_[GPU_SAMPLER_CUSTOM_TYPES_COUNT];

 public:
  void init();
  void free();

  const VKSampler &get(const GPUSamplerState &sampler_state) const;
};

}  // namespace blender::gpu
