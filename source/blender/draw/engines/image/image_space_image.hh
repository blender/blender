/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2021 Blender Foundation. */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "image_private.hh"

namespace blender::draw::image_engine {

class SpaceImageAccessor : public AbstractSpaceAccessor {
  SpaceImage *sima;

 public:
  SpaceImageAccessor(SpaceImage *sima) : sima(sima)
  {
  }

  Image *get_image(Main *UNUSED(bmain)) override
  {
    return ED_space_image(sima);
  }

  ImageUser *get_image_user() override
  {
    return &sima->iuser;
  }

  ImBuf *acquire_image_buffer(Image *UNUSED(image), void **lock) override
  {
    return ED_space_image_acquire_buffer(sima, lock, 0);
  }

  void release_buffer(Image *UNUSED(image), ImBuf *image_buffer, void *lock) override
  {
    ED_space_image_release_buffer(sima, image_buffer, lock);
  }

  void get_shader_parameters(ShaderParameters &r_shader_parameters, ImBuf *image_buffer) override
  {
    const int sima_flag = sima->flag & ED_space_image_get_display_channel_mask(image_buffer);
    if ((sima_flag & SI_USE_ALPHA) != 0) {
      /* Show RGBA */
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHOW_ALPHA | IMAGE_DRAW_FLAG_APPLY_ALPHA;
    }
    else if ((sima_flag & SI_SHOW_ALPHA) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      copy_v4_fl4(r_shader_parameters.shuffle, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    else if ((sima_flag & SI_SHOW_ZBUF) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_DEPTH | IMAGE_DRAW_FLAG_SHUFFLING;
      copy_v4_fl4(r_shader_parameters.shuffle, 1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_R) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      copy_v4_fl4(r_shader_parameters.shuffle, 1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_G) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      copy_v4_fl4(r_shader_parameters.shuffle, 0.0f, 1.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_B) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      copy_v4_fl4(r_shader_parameters.shuffle, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    else /* RGB */ {
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
    }
  }

  void get_gpu_textures(Image *image,
                        ImageUser *iuser,
                        ImBuf *image_buffer,
                        GPUTexture **r_gpu_texture,
                        bool *r_owns_texture,
                        GPUTexture **r_tex_tile_data) override
  {
    if (image->rr != nullptr) {
      /* Update multi-index and pass for the current eye. */
      BKE_image_multilayer_index(image->rr, iuser);
    }
    else {
      BKE_image_multiview_index(image, iuser);
    }

    if (image_buffer == nullptr) {
      return;
    }

    if (image_buffer->rect == nullptr && image_buffer->rect_float == nullptr) {
      /* This code-path is only supposed to happen when drawing a lazily-allocatable render result.
       * In all the other cases the `ED_space_image_acquire_buffer()` is expected to return nullptr
       * as an image buffer when it has no pixels. */

      BLI_assert(image->type == IMA_TYPE_R_RESULT);

      float zero[4] = {0, 0, 0, 0};
      *r_gpu_texture = GPU_texture_create_2d(__func__, 1, 1, 0, GPU_RGBA16F, zero);
      *r_owns_texture = true;
      return;
    }

    const int sima_flag = sima->flag & ED_space_image_get_display_channel_mask(image_buffer);
    if (sima_flag & SI_SHOW_ZBUF &&
        (image_buffer->zbuf || image_buffer->zbuf_float || (image_buffer->channels == 1))) {
      if (image_buffer->zbuf) {
        BLI_assert_msg(0, "Integer based depth buffers not supported");
      }
      else if (image_buffer->zbuf_float) {
        *r_gpu_texture = GPU_texture_create_2d(
            __func__, image_buffer->x, image_buffer->y, 0, GPU_R16F, image_buffer->zbuf_float);
        *r_owns_texture = true;
      }
      else if (image_buffer->rect_float && image_buffer->channels == 1) {
        *r_gpu_texture = GPU_texture_create_2d(
            __func__, image_buffer->x, image_buffer->y, 0, GPU_R16F, image_buffer->rect_float);
        *r_owns_texture = true;
      }
    }
    else if (image->source == IMA_SRC_TILED) {
      *r_gpu_texture = BKE_image_get_gpu_tiles(image, iuser, image_buffer);
      *r_tex_tile_data = BKE_image_get_gpu_tilemap(image, iuser, nullptr);
      *r_owns_texture = false;
    }
    else {
      *r_gpu_texture = BKE_image_get_gpu_texture(image, iuser, image_buffer);
      *r_owns_texture = false;
    }
  }

  bool use_tile_drawing() const override
  {
    return (sima->flag & SI_DRAW_TILE) != 0;
  }

  void init_ss_to_texture_matrix(const ARegion *region,
                                 const float image_display_offset[2],
                                 const float image_resolution[2],
                                 float r_uv_to_texture[4][4]) const override
  {
     float zoom_x = image_resolution[0] * sima->zoom;
     float zoom_y = image_resolution[1] * sima ->zoom;
     float image_offset_x = (region->winx - zoom_x) / 2 + sima->xof + image_display_offset[0];
     float image_offset_y = (region->winy - zoom_y) / 2 + sima->yof + image_display_offset[1];

     unit_m4(r_uv_to_texture);
     float scale_x = 1.0 / BLI_rctf_size_x(&region->v2d.cur);
     float scale_y = 1.0 / BLI_rctf_size_y(&region->v2d.cur);
     float offset_x = 1.0 / image_resolution[0] * image_offset_x;
     float offset_y = 1.0 / image_resolution[1] * image_offset_y;

     float translate_x = scale_x * (-region->v2d.cur.xmin + offset_x);
     float translate_y = scale_y * (-region->v2d.cur.ymin + offset_y);

     r_uv_to_texture[0][0] = scale_x;
     r_uv_to_texture[1][1] = scale_y;
     r_uv_to_texture[3][0] = translate_x;
     r_uv_to_texture[3][1] = translate_y;
   }
};

}  // namespace blender::draw::image_engine
