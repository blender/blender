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

void ImageSpaceDrawingMode::begin_sync() const {}

void ImageSpaceDrawingMode::image_sync(blender::Image *image, ImageUser *iuser) const
{
  PassSimple &pass = instance_.state.image_ps;
  pass.init();
  pass.state_set(DRW_STATE_WRITE_COLOR | DRW_STATE_WRITE_DEPTH | DRW_STATE_DEPTH_ALWAYS);
  pass.shader_set(image->source == IMA_SRC_TILED ? ShaderModule::module_get().image_tiled.get() :
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
  switch (image->source) {
    case IMA_SRC_VIEWER: {
      pass.push_constant("is_repeated", instance_.state.flags.do_tile_drawing);
      pass.bind_texture("image_tx", BKE_image_get_gpu_viewer_texture(image, iuser), sampler);
      break;
    }
    case IMA_SRC_TILED: {
      ImageGPUTextures gpu_tiles_textures = BKE_image_get_gpu_material_texture(image, iuser, true);
      pass.bind_texture("image_tile_array", *gpu_tiles_textures.texture, sampler);
      pass.bind_texture("image_tile_data", *gpu_tiles_textures.tile_mapping, sampler);
      break;
    }
    default: {
      pass.push_constant("is_repeated", instance_.state.flags.do_tile_drawing);
      pass.bind_texture("image_tx", BKE_image_get_gpu_texture(image, iuser), sampler);
      break;
    }
  }
  pass.draw_procedural(GPU_PRIM_TRIS, 1, 3);
}

void ImageSpaceDrawingMode::draw_viewport() const
{
  instance_.manager->submit(instance_.state.image_ps, instance_.state.view);
}

void ImageSpaceDrawingMode::draw_finish() const {}

};  // namespace blender::image_engine
