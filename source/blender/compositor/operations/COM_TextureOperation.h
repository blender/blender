/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_listbase.h"
#include "COM_MultiThreadedOperation.h"
#include "DNA_texture_types.h"
#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"
#include "RE_texture.h"

namespace blender::compositor {

/**
 * Base class for all renderlayeroperations
 *
 * \todo Rename to operation.
 */
class TextureBaseOperation : public MultiThreadedOperation {
 private:
  Tex *texture_;
  const RenderData *rd_;
  SocketReader *input_size_;
  SocketReader *input_offset_;
  struct ImagePool *pool_;
  bool scene_color_manage_;

 protected:
  /**
   * Determine the output resolution.
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  /**
   * Constructor
   */
  TextureBaseOperation();

 public:
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void set_texture(Tex *texture)
  {
    texture_ = texture;
  }
  void init_execution() override;
  void deinit_execution() override;
  void set_render_data(const RenderData *rd)
  {
    rd_ = rd;
  }
  void set_scene_color_manage(bool scene_color_manage)
  {
    scene_color_manage_ = scene_color_manage;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class TextureOperation : public TextureBaseOperation {
 public:
  TextureOperation();
};
class TextureAlphaOperation : public TextureBaseOperation {
 public:
  TextureAlphaOperation();
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
