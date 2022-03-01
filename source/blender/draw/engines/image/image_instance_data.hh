/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2021, Blender Foundation.
 */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "image_batches.hh"
#include "image_buffer_cache.hh"
#include "image_partial_updater.hh"
#include "image_private.hh"
#include "image_shader_params.hh"
#include "image_texture_info.hh"
#include "image_usage.hh"
#include "image_wrappers.hh"

#include "DRW_render.h"

/**
 * \brief max allowed textures to use by the ScreenSpaceDrawingMode.
 *
 * 4 textures are used to reduce uploading screen space textures when translating the image.
 */
constexpr int SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN = 1;

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
  TextureInfo texture_infos[SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN];

 public:
  virtual ~IMAGE_InstanceData() = default;

  void clear_dirty_flag()
  {
    reset_dirty_flag(false);
  }
  void mark_all_texture_slots_dirty()
  {
    reset_dirty_flag(true);
  }

  void update_gpu_texture_allocations()
  {
    for (int i = 0; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
      TextureInfo &info = texture_infos[i];
      const bool is_allocated = info.texture != nullptr;
      const bool is_visible = info.visible;
      const bool should_be_freed = !is_visible && is_allocated;
      const bool should_be_created = is_visible && !is_allocated;

      if (should_be_freed) {
        GPU_texture_free(info.texture);
        info.texture = nullptr;
      }

      if (should_be_created) {
        DRW_texture_ensure_fullscreen_2d(
            &info.texture, GPU_RGBA16F, static_cast<DRWTextureFlag>(0));
      }
      info.dirty |= should_be_created;
    }
  }

  void update_batches()
  {
    for (int i = 0; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
      TextureInfo &info = texture_infos[i];
      if (!info.dirty) {
        continue;
      }
      BatchUpdater batch_updater(info);
      batch_updater.update_batch();
    }
  }

  void update_image_usage(const ImageUser *image_user)
  {
    ImageUsage usage(image, image_user, flags.do_tile_drawing);
    if (last_usage != usage) {
      last_usage = usage;
      reset_dirty_flag(true);
      float_buffers.clear();
    }
  }

 private:
  /** \brief Set dirty flag of all texture slots to the given value. */
  void reset_dirty_flag(bool new_value)
  {
    for (int i = 0; i < SCREEN_SPACE_DRAWING_MODE_TEXTURE_LEN; i++) {
      texture_infos[i].dirty = new_value;
    }
  }
};
