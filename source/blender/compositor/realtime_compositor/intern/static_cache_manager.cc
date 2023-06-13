/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_static_cache_manager.hh"

namespace blender::realtime_compositor {

void StaticCacheManager::reset()
{
  symmetric_blur_weights.reset();
  symmetric_separable_blur_weights.reset();
  morphological_distance_feather_weights.reset();
  cached_textures.reset();
  cached_masks.reset();
  smaa_precomputed_textures.reset();
  ocio_color_space_conversion_shaders.reset();
}

}  // namespace blender::realtime_compositor
