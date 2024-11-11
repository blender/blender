/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_smaa_textures.h"

#include "GPU_shader.hh"
#include "GPU_texture.hh"

#include "COM_context.hh"
#include "COM_result.hh"
#include "COM_smaa_precomputed_textures.hh"
#include "COM_utilities.hh"

namespace blender::realtime_compositor {

/* ------------------------------------------------------------------------------------------------
 * SMAA Precomputed Textures.
 */

SMAAPrecomputedTextures::SMAAPrecomputedTextures(Context &context)
    : search_texture(context.create_result(ResultType::Float)),
      area_texture(context.create_result(ResultType::Float2))
{
  if (context.use_gpu()) {
    this->compute_gpu();
  }
  else {
    this->compute_cpu();
  }
}

SMAAPrecomputedTextures::~SMAAPrecomputedTextures()
{
  GPU_TEXTURE_FREE_SAFE(search_texture_);
  GPU_TEXTURE_FREE_SAFE(area_texture_);
  this->search_texture.release();
  this->area_texture.release();
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

void SMAAPrecomputedTextures::compute_gpu()
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

void SMAAPrecomputedTextures::compute_cpu()
{
  const int2 search_texture_size = int2(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT);
  search_texture.allocate_texture(Domain(search_texture_size));
  parallel_for(search_texture_size, [&](const int2 texel) {
    const float value = searchTexBytes[int64_t(texel.y) * search_texture_size.x + texel.x] /
                        255.0f;
    search_texture.store_pixel(texel, float4(value));
  });

  const int2 area_texture_size = int2(AREATEX_WIDTH, AREATEX_HEIGHT);
  area_texture.allocate_texture(Domain(area_texture_size));
  parallel_for(area_texture_size, [&](const int2 texel) {
    const float2 value = float2(uchar2(areaTexBytes +
                                       (int64_t(texel.y) * area_texture_size.x + texel.x) * 2)) /
                         255.0f;
    area_texture.store_pixel(texel, float4(value, 0.0f, 0.0f));
  });
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

SMAAPrecomputedTextures &SMAAPrecomputedTexturesContainer::get(Context &context)
{
  if (!textures_) {
    textures_ = std::make_unique<SMAAPrecomputedTextures>(context);
  }

  textures_->needed = true;
  return *textures_;
}

}  // namespace blender::realtime_compositor
