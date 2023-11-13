/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_smaa_textures.h"

#include "GPU_shader.h"
#include "GPU_texture.h"

#include "COM_smaa_precomputed_textures.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * SMAA Precomputed Textures.
 */

SMAAPrecomputedTextures::SMAAPrecomputedTextures()
{
  search_texture_ = GPU_texture_create_2d("SMAA Search",
                                          SEARCHTEX_WIDTH,
                                          SEARCHTEX_HEIGHT,
                                          1,
                                          GPU_R8,
                                          GPU_TEXTURE_USAGE_SHADER_READ,
                                          nullptr);
  GPU_texture_update(search_texture_, GPU_DATA_UBYTE, searchTexBytes);
  GPU_texture_filter_mode(search_texture_, true);

  area_texture_ = GPU_texture_create_2d("SMAA Area",
                                        AREATEX_WIDTH,
                                        AREATEX_HEIGHT,
                                        1,
                                        GPU_RG8,
                                        GPU_TEXTURE_USAGE_SHADER_READ,
                                        nullptr);
  GPU_texture_update(area_texture_, GPU_DATA_UBYTE, areaTexBytes);
  GPU_texture_filter_mode(area_texture_, true);
}

SMAAPrecomputedTextures::~SMAAPrecomputedTextures()
{
  GPU_texture_free(search_texture_);
  GPU_texture_free(area_texture_);
}

void SMAAPrecomputedTextures::bind_search_texture(GPUShader *shader,
                                                  const char *sampler_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, sampler_name);
  GPU_texture_bind(search_texture_, texture_image_unit);
}

void SMAAPrecomputedTextures::unbind_search_texture() const
{
  GPU_texture_unbind(search_texture_);
}

void SMAAPrecomputedTextures::bind_area_texture(GPUShader *shader, const char *sampler_name) const
{
  const int texture_image_unit = GPU_shader_get_sampler_binding(shader, sampler_name);
  GPU_texture_bind(area_texture_, texture_image_unit);
}

void SMAAPrecomputedTextures::unbind_area_texture() const
{
  GPU_texture_unbind(area_texture_);
}

/* ------------------------------------------------------------------------------------------------
 * SMAA Precomputed Textures Container.
 */

void SMAAPrecomputedTexturesContainer::reset()
{
  /* First, delete the textures if they are no longer needed. */
  if (textures_ && !textures_->needed) {
    textures_.reset();
  }

  /* Second, if they were not deleted, reset their needed status to false to ready them to track
   * their needed status for the next evaluation. */
  if (textures_) {
    textures_->needed = false;
  }
}

SMAAPrecomputedTextures &SMAAPrecomputedTexturesContainer::get()
{
  if (!textures_) {
    textures_ = std::make_unique<SMAAPrecomputedTextures>();
  }

  textures_->needed = true;
  return *textures_;
}

}  // namespace blender::realtime_compositor
