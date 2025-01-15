/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "ED_image.hh"

#include "DNA_screen_types.h"

#include "image_private.hh"

namespace blender::image_engine {

class SpaceImageAccessor : public AbstractSpaceAccessor {
  SpaceImage *sima;

 public:
  SpaceImageAccessor(SpaceImage *sima) : sima(sima) {}

  ::Image *get_image(Main * /*bmain*/) override
  {
    return ED_space_image(sima);
  }

  ImageUser *get_image_user() override
  {
    return &sima->iuser;
  }

  ImBuf *acquire_image_buffer(::Image * /*image*/, void **lock) override
  {
    return ED_space_image_acquire_buffer(sima, lock, 0);
  }

  void release_buffer(::Image * /*image*/, ImBuf *image_buffer, void *lock) override
  {
    ED_space_image_release_buffer(sima, image_buffer, lock);
  }

  void get_shader_parameters(ShaderParameters &r_shader_parameters, ImBuf *image_buffer) override
  {
    const int sima_flag = sima->flag & ED_space_image_get_display_channel_mask(image_buffer);
    if ((sima_flag & SI_USE_ALPHA) != 0) {
      /* Show RGBA */
      r_shader_parameters.flags |= ImageDrawFlags::SHOW_ALPHA | ImageDrawFlags::APPLY_ALPHA;
    }
    else if ((sima_flag & SI_SHOW_ALPHA) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::SHUFFLING;
      r_shader_parameters.shuffle = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else if ((sima_flag & SI_SHOW_ZBUF) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::DEPTH | ImageDrawFlags::SHUFFLING;
      r_shader_parameters.shuffle = float4(1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_R) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= ImageDrawFlags::APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_G) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= ImageDrawFlags::APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(0.0f, 1.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_B) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= ImageDrawFlags::APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(0.0f, 0.0f, 1.0f, 0.0f);
    }
    else /* RGB */ {
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= ImageDrawFlags::APPLY_ALPHA;
      }
    }
  }

  bool use_tile_drawing() const override
  {
    return (sima->flag & SI_DRAW_TILE) != 0;
  }

  void init_ss_to_texture_matrix(const ARegion *region,
                                 const float image_offset[2],
                                 const float image_resolution[2],
                                 float r_uv_to_texture[4][4]) const override
  {
    unit_m4(r_uv_to_texture);
    float scale_x = 1.0 / BLI_rctf_size_x(&region->v2d.cur);
    float scale_y = 1.0 / BLI_rctf_size_y(&region->v2d.cur);

    float display_offset_x = scale_x * image_offset[0] / image_resolution[0];
    float display_offset_y = scale_y * image_offset[1] / image_resolution[1];

    float translate_x = scale_x * -region->v2d.cur.xmin + display_offset_x;
    float translate_y = scale_y * -region->v2d.cur.ymin + display_offset_y;

    r_uv_to_texture[0][0] = scale_x;
    r_uv_to_texture[1][1] = scale_y;
    r_uv_to_texture[3][0] = translate_x;
    r_uv_to_texture[3][1] = translate_y;
  }
};

}  // namespace blender::image_engine
