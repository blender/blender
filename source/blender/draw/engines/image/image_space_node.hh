/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "image_private.hh"

namespace blender::draw::image_engine {

class SpaceNodeAccessor : public AbstractSpaceAccessor {
  SpaceNode *snode;

 public:
  SpaceNodeAccessor(SpaceNode *snode) : snode(snode) {}

  Image *get_image(Main *bmain) override
  {
    return BKE_image_ensure_viewer(bmain, IMA_TYPE_COMPOSITE, "Viewer Node");
  }

  ImageUser *get_image_user() override
  {
    return nullptr;
  }

  ImBuf *acquire_image_buffer(Image *image, void **lock) override
  {
    return BKE_image_acquire_ibuf(image, nullptr, lock);
  }

  void release_buffer(Image *image, ImBuf *ibuf, void *lock) override
  {
    BKE_image_release_ibuf(image, ibuf, lock);
  }

  void get_shader_parameters(ShaderParameters &r_shader_parameters, ImBuf *ibuf) override
  {
    if ((snode->flag & SNODE_USE_ALPHA) != 0) {
      /* Show RGBA */
      r_shader_parameters.flags |= ImageDrawFlags::ShowAlpha | ImageDrawFlags::ApplyAlpha;
    }
    else if ((snode->flag & SNODE_SHOW_ALPHA) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::Shuffling;
      copy_v4_fl4(r_shader_parameters.shuffle, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    else if ((snode->flag & SNODE_SHOW_R) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::Shuffling;
      if (IMB_alpha_affects_rgb(ibuf)) {
        r_shader_parameters.flags |= ImageDrawFlags::ApplyAlpha;
      }
      copy_v4_fl4(r_shader_parameters.shuffle, 1.0f, 0.0f, 0.0f, 0.0f);
    }
    else if ((snode->flag & SNODE_SHOW_G) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::Shuffling;
      if (IMB_alpha_affects_rgb(ibuf)) {
        r_shader_parameters.flags |= ImageDrawFlags::ApplyAlpha;
      }
      copy_v4_fl4(r_shader_parameters.shuffle, 0.0f, 1.0f, 0.0f, 0.0f);
    }
    else if ((snode->flag & SNODE_SHOW_B) != 0) {
      r_shader_parameters.flags |= ImageDrawFlags::Shuffling;
      if (IMB_alpha_affects_rgb(ibuf)) {
        r_shader_parameters.flags |= ImageDrawFlags::ApplyAlpha;
      }
      copy_v4_fl4(r_shader_parameters.shuffle, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    else /* RGB */ {
      if (IMB_alpha_affects_rgb(ibuf)) {
        r_shader_parameters.flags |= ImageDrawFlags::ApplyAlpha;
      }
    }
  }

  bool use_tile_drawing() const override
  {
    return false;
  }

  /**
   * The backdrop of the node editor isn't drawn in screen space UV space. But is locked with the
   * screen.
   */
  void init_ss_to_texture_matrix(const ARegion *region,
                                 const float image_offset[2],
                                 const float image_resolution[2],
                                 float r_uv_to_texture[4][4]) const override
  {
    unit_m4(r_uv_to_texture);
    float display_resolution[2];
    float image_display_offset[2];
    mul_v2_v2fl(display_resolution, image_resolution, snode->zoom);
    mul_v2_v2fl(image_display_offset, image_offset, snode->zoom);
    const float scale_x = display_resolution[0] / region->winx;
    const float scale_y = display_resolution[1] / region->winy;
    const float translate_x = ((region->winx - display_resolution[0]) * 0.5f + snode->xof +
                               image_display_offset[0]) /
                              region->winx;
    const float translate_y = ((region->winy - display_resolution[1]) * 0.5f + snode->yof +
                               image_display_offset[1]) /
                              region->winy;

    r_uv_to_texture[0][0] = scale_x;
    r_uv_to_texture[1][1] = scale_y;
    r_uv_to_texture[3][0] = translate_x;
    r_uv_to_texture[3][1] = translate_y;
  }
};

}  // namespace blender::draw::image_engine
