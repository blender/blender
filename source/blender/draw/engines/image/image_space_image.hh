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
#include "image_shader_shared.hh"

namespace blender::image_engine {

class SpaceImageAccessor : public AbstractSpaceAccessor {
  SpaceImage *sima;

 public:
  SpaceImageAccessor(SpaceImage *sima) : sima(sima) {}

  blender::Image *get_image(Main * /*bmain*/) override
  {
    return ED_space_image(sima);
  }

  ImageUser *get_image_user() override
  {
    return &sima->iuser;
  }

  ImBuf *acquire_image_buffer(blender::Image * /*image*/, void **lock) override
  {
    return ED_space_image_acquire_buffer(sima, lock, 0, false);
  }

  void release_buffer(blender::Image * /*image*/, ImBuf *image_buffer, void *lock) override
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
      r_shader_parameters.shuffle = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else if ((sima_flag & SI_SHOW_ZBUF) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_DEPTH | IMAGE_DRAW_FLAG_SHUFFLING;
      r_shader_parameters.shuffle = float4(1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_R) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_G) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(0.0f, 1.0f, 0.0f, 0.0f);
    }
    else if ((sima_flag & SI_SHOW_B) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(0.0f, 0.0f, 1.0f, 0.0f);
    }
    else /* RGB */ {
      if (IMB_alpha_affects_rgb(image_buffer)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
    }
  }

  bool use_tile_drawing() const override
  {
    return (sima->flag & SI_DRAW_TILE) != 0;
  }

  bool use_display_window() const override
  {
    return sima->mode == SI_MODE_VIEW;
  }

  float get_zoom() const override
  {
    return this->sima->zoom;
  }

  float get_aspect_ratio() const override
  {
    float2 aspect_ratio;
    ED_space_image_get_aspect(this->sima, &aspect_ratio.x, &aspect_ratio.y);
    return aspect_ratio.y;
  }

  float2 get_pan_offset() const override
  {
    /* The offsets are stored with zooming, so retrieve original offsets by multiplying the zoom.
     * Furthermore, take the negatives since we want the offset of the image, not the space. */
    return -float2(sima->xof, sima->yof) * sima->zoom;
  }
};

}  // namespace blender::image_engine
