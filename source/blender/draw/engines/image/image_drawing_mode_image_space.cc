/* SPDX-FileCopyrightText: 2026 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#include "image_drawing_mode_image_space.hh"
#include "image_instance.hh"
#include "image_shader.hh"

namespace blender::image_engine {

ImageSpaceDrawingMode::ImageSpaceDrawingMode(Instance &instance,
                                             gpu::Texture *texture,
                                             gpu::Texture *tile_mapping_texture)
    : instance_(instance), texture_(texture), tile_mapping_texture_(tile_mapping_texture)
{
  GPU_texture_ref(texture_);
  if (tile_mapping_texture_) {
    GPU_texture_ref(tile_mapping_texture_);
  }
}

ImageSpaceDrawingMode::~ImageSpaceDrawingMode()
{
  GPU_texture_free(texture_);
  if (tile_mapping_texture_) {
    GPU_texture_free(tile_mapping_texture_);
  }
}

void ImageSpaceDrawingMode::begin_sync() const {}

void ImageSpaceDrawingMode::image_sync(blender::Image * /*image*/, ImageUser * /*iuser*/) const {}

void ImageSpaceDrawingMode::draw_viewport() const
{
  PassSimple &pass = instance_.state.image_ps;
  pass.init();
  pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
  pass.shader_set(tile_mapping_texture_ ? ShaderModule::module_get().image_tiled.get() :
                                          ShaderModule::module_get().image.get());
  pass.push_constant("image_matrix", math::invert(float4x4(instance_.state.ss_to_texture)));
  pass.push_constant("far_near_distances", instance_.state.sh_params.far_near);
  pass.push_constant("shuffle", instance_.state.sh_params.shuffle);
  pass.push_constant("draw_flags", int32_t(instance_.state.sh_params.flags));
  pass.push_constant("is_image_premultiplied", instance_.state.sh_params.use_premul_alpha);

  /* The shader will discard fragments that are outside of the image if repeating is disabled, so
   * we just always have repeat mode enabled. */
  const GPUSamplerState sampler = {.filtering = GPU_SAMPLER_FILTERING_DEFAULT,
                                   .extend_x = GPU_SAMPLER_EXTEND_MODE_REPEAT,
                                   .extend_yz = GPU_SAMPLER_EXTEND_MODE_REPEAT};

  if (tile_mapping_texture_) {
    pass.bind_texture("image_tile_array", texture_, sampler);
    pass.bind_texture("image_tile_data", tile_mapping_texture_, sampler);
  }
  else {
    pass.push_constant("is_repeated", instance_.state.flags.do_tile_drawing);
    pass.bind_texture("image_tx", texture_, sampler);
  }
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
  instance_.manager->submit(instance_.state.image_ps, instance_.state.view);
}

void ImageSpaceDrawingMode::draw_finish() const {}

};  // namespace blender::image_engine
