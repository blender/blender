/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "image_private.hh"

namespace blender::image_engine {

class SpaceNodeAccessor : public AbstractSpaceAccessor {
  SpaceNode *snode;

 public:
  SpaceNodeAccessor(SpaceNode *snode) : snode(snode) {}

  blender::Image *get_image(Main *bmain) override
  {
    return BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  }

  ImageUser *get_image_user() override
  {
    return nullptr;
  }

  ImBuf *acquire_image_buffer(blender::Image *image, void **lock) override
  {
    return BKE_image_acquire_ibuf_gpu(image, nullptr, lock);
  }

  void release_buffer(blender::Image *image, ImBuf *ibuf, void *lock) override
  {
    BKE_image_release_ibuf(image, ibuf, lock);
  }

  void get_shader_parameters(ShaderParameters &r_shader_parameters, ImBuf *ibuf) override
  {
    if ((snode->flag & SNODE_USE_ALPHA) != 0) {
      /* Show RGBA */
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHOW_ALPHA | IMAGE_DRAW_FLAG_APPLY_ALPHA;
    }
    else if ((snode->flag & SNODE_SHOW_ALPHA) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      r_shader_parameters.shuffle = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else if ((snode->flag & SNODE_SHOW_R) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(ibuf)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((snode->flag & SNODE_SHOW_G) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(ibuf)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(0.0f, 1.0f, 0.0f, 0.0f);
    }
    else if ((snode->flag & SNODE_SHOW_B) != 0) {
      r_shader_parameters.flags |= IMAGE_DRAW_FLAG_SHUFFLING;
      if (IMB_alpha_affects_rgb(ibuf)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
      r_shader_parameters.shuffle = float4(0.0f, 0.0f, 1.0f, 0.0f);
    }
    else /* RGB */ {
      if (IMB_alpha_affects_rgb(ibuf)) {
        r_shader_parameters.flags |= IMAGE_DRAW_FLAG_APPLY_ALPHA;
      }
    }
  }

  bool use_tile_drawing() const override
  {
    return false;
  }

  bool use_display_window() const override
  {
    return true;
  }

  float get_zoom() const override
  {
    return this->snode->zoom;
  }

  float get_aspect_ratio() const override
  {
    return 1.0f;
  }

  float2 get_pan_offset() const override
  {
    return float2(snode->xof, snode->yof);
  }
};

}  // namespace blender::image_engine
