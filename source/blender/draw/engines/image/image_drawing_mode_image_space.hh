/* SPDX-FileCopyrightText: 2026 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#pragma once

#include "image_private.hh"

namespace blender::image_engine {

class Instance;

/**
 * Drawing mode optimized for textures that fits within the GPU specifications.
 *
 * Each GPU has a max texture size. Textures larger than this size aren't able to be allocated on
 * the GPU. For large textures use #ScreenSpaceDrawingMode.
 */
class ImageSpaceDrawingMode : public AbstractDrawingMode {
 private:
  Instance &instance_;
  gpu::Texture *texture_;
  gpu::Texture *tile_mapping_texture_ = nullptr;

 public:
  ImageSpaceDrawingMode(Instance &instance,
                        gpu::Texture *texture,
                        gpu::Texture *tile_mapping_texture = nullptr);
  ~ImageSpaceDrawingMode() override;
  void begin_sync() const override;
  void image_sync(blender::Image *image, blender::ImageUser *iuser) const override;
  void draw_finish() const override;
  void draw_viewport() const override;
};
};  // namespace blender::image_engine
