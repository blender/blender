/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_image.h"
#include "BLI_listbase.h"
#include "BLI_sys_types.h"
#include "BLI_utildefines.h"
#include "COM_MultiThreadedOperation.h"
#include "MEM_guardedalloc.h"

#include "RE_pipeline.h"
#include "RE_texture.h"

namespace blender::compositor {

/**
 * \brief Base class for all image operations
 */
class BaseImageOperation : public MultiThreadedOperation {
 protected:
  ImBuf *buffer_;
  Image *image_;
  ImageUser *image_user_;
  /* TODO: Remove raw buffers when removing Tiled implementation. */
  float *image_float_buffer_;
  uint8_t *image_byte_buffer_;
  float *image_depth_buffer_;

  MemoryBuffer *depth_buffer_;
  int imageheight_;
  int imagewidth_;
  int framenumber_;
  int number_of_channels_;
  const RenderData *rd_;
  const char *view_name_;

  BaseImageOperation();
  /**
   * Determine the output resolution. The resolution is retrieved from the Renderer
   */
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  virtual ImBuf *get_im_buf();

 public:
  void init_execution() override;
  void deinit_execution() override;
  void set_image(Image *image)
  {
    image_ = image;
  }
  void set_image_user(ImageUser *imageuser)
  {
    image_user_ = imageuser;
  }
  void set_render_data(const RenderData *rd)
  {
    rd_ = rd;
  }
  void set_view_name(const char *view_name)
  {
    view_name_ = view_name;
  }
  void set_framenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }
};
class ImageOperation : public BaseImageOperation {
 public:
  /**
   * Constructor
   */
  ImageOperation();
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};
class ImageAlphaOperation : public BaseImageOperation {
 public:
  /**
   * Constructor
   */
  ImageAlphaOperation();
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};
class ImageDepthOperation : public BaseImageOperation {
 public:
  /**
   * Constructor
   */
  ImageDepthOperation();
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
