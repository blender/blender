/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "vk_samplers.hh"

namespace blender::gpu {

void VKSamplers::init()
{
  if (custom_sampler_cache_[0].is_initialized()) {
    return;
  }
  custom_sampler_cache_[GPU_SAMPLER_CUSTOM_COMPARE].create(GPUSamplerState::compare_sampler());
  custom_sampler_cache_[GPU_SAMPLER_CUSTOM_ICON].create(GPUSamplerState::icon_sampler());

  GPUSamplerState state = {};
  for (int extend_yz_i = 0; extend_yz_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_yz_i++) {
    state.extend_yz = static_cast<GPUSamplerExtendMode>(extend_yz_i);
    for (int extend_x_i = 0; extend_x_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_x_i++) {
      state.extend_x = static_cast<GPUSamplerExtendMode>(extend_x_i);
      for (int filtering_i = 0; filtering_i < GPU_SAMPLER_FILTERING_TYPES_COUNT; filtering_i++) {
        state.filtering = GPUSamplerFiltering(filtering_i);
        sampler_cache_[extend_yz_i][extend_x_i][filtering_i].create(state);
      }
    }
  }
}

void VKSamplers::free()
{
  custom_sampler_cache_[GPU_SAMPLER_CUSTOM_COMPARE].free();
  custom_sampler_cache_[GPU_SAMPLER_CUSTOM_ICON].free();

  for (int extend_yz_i = 0; extend_yz_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_yz_i++) {
    for (int extend_x_i = 0; extend_x_i < GPU_SAMPLER_EXTEND_MODES_COUNT; extend_x_i++) {
      for (int filtering_i = 0; filtering_i < GPU_SAMPLER_FILTERING_TYPES_COUNT; filtering_i++) {
        sampler_cache_[extend_yz_i][extend_x_i][filtering_i].free();
      }
    }
  }
}

const VKSampler &VKSamplers::get(const GPUSamplerState &sampler_state) const
{
  BLI_assert(sampler_state.type != GPU_SAMPLER_STATE_TYPE_INTERNAL);

  if (sampler_state.type == GPU_SAMPLER_STATE_TYPE_CUSTOM) {
    return custom_sampler_cache_[sampler_state.custom_type];
  }
  return sampler_cache_[sampler_state.extend_yz][sampler_state.extend_x][sampler_state.filtering];
}

}  // namespace blender::gpu
