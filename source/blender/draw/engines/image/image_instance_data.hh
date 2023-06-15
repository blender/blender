/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "BKE_image_wrappers.hh"

#include "image_batches.hh"
#include "image_buffer_cache.hh"
#include "image_partial_updater.hh"
#include "image_private.hh"
#include "image_shader_params.hh"
#include "image_texture_info.hh"
#include "image_usage.hh"

#include "DRW_render.h"

namespace blender::draw::image_engine {

struct IMAGE_InstanceData {
  struct Image *image;
  /** Usage data of the previous time, to identify changes that require a full update. */
  ImageUsage last_usage;

  PartialImageUpdater partial_update;

  struct DRWView *view;
  ShaderParameters sh_params;
  struct {
    /**
     * \brief should we perform tiled drawing (wrap repeat).
     *
     * Option is true when image is capable of tile drawing (image is not tile) and the tiled
     * option is set in the space.
     */
    bool do_tile_drawing : 1;
  } flags;

  struct {
    DRWPass *image_pass;
    DRWPass *depth_pass;
  } passes;

  /**
   * Cache containing the float buffers when drawing byte images.
   */
  FloatBufferCache float_buffers;

  /** \brief Transform matrix to convert a normalized screen space coordinates to texture space. */
  float ss_to_texture[4][4];

  Vector<TextureInfo> texture_infos;

 public:
  virtual ~IMAGE_InstanceData() = default;

  void clear_need_full_update_flag()
  {
    reset_need_full_update(false);
  }
  void mark_all_texture_slots_dirty()
  {
    reset_need_full_update(true);
  }

  void update_batches()
  {
    for (TextureInfo &info : texture_infos) {
      BatchUpdater batch_updater(info);
      batch_updater.update_batch();
    }
  }

  void update_image_usage(const ImageUser *image_user)
  {
    ImageUsage usage(image, image_user, flags.do_tile_drawing);
    if (last_usage != usage) {
      last_usage = usage;
      reset_need_full_update(true);
      float_buffers.clear();
    }
  }

 private:
  /** \brief Set dirty flag of all texture slots to the given value. */
  void reset_need_full_update(bool new_value)
  {
    for (TextureInfo &info : texture_infos) {
      info.need_full_update = new_value;
    }
  }
};

}  // namespace blender::draw::image_engine
