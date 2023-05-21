/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. */

#include "COM_ImageOperation.h"

#include "BKE_scene.h"

#include "IMB_colormanagement.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::compositor {

BaseImageOperation::BaseImageOperation()
{
  image_ = nullptr;
  buffer_ = nullptr;
  image_float_buffer_ = nullptr;
  image_byte_buffer_ = nullptr;
  image_user_ = nullptr;
  imagewidth_ = 0;
  imageheight_ = 0;
  framenumber_ = 0;
  image_depth_buffer_ = nullptr;
  depth_buffer_ = nullptr;
  number_of_channels_ = 0;
  rd_ = nullptr;
  view_name_ = nullptr;
}
ImageOperation::ImageOperation() : BaseImageOperation()
{
  this->add_output_socket(DataType::Color);
}
ImageAlphaOperation::ImageAlphaOperation() : BaseImageOperation()
{
  this->add_output_socket(DataType::Value);
}
ImageDepthOperation::ImageDepthOperation() : BaseImageOperation()
{
  this->add_output_socket(DataType::Value);
}

ImBuf *BaseImageOperation::get_im_buf()
{
  ImBuf *ibuf;
  ImageUser iuser = *image_user_;

  if (image_ == nullptr) {
    return nullptr;
  }

  /* local changes to the original ImageUser */
  if (BKE_image_is_multilayer(image_) == false) {
    iuser.multi_index = BKE_scene_multiview_view_id_get(rd_, view_name_);
  }

  ibuf = BKE_image_acquire_ibuf(image_, &iuser, nullptr);
  if (ibuf == nullptr || (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr))
  {
    BKE_image_release_ibuf(image_, ibuf, nullptr);
    return nullptr;
  }
  return ibuf;
}

void BaseImageOperation::init_execution()
{
  ImBuf *stackbuf = get_im_buf();
  buffer_ = stackbuf;
  if (stackbuf) {
    image_float_buffer_ = stackbuf->float_buffer.data;
    image_byte_buffer_ = stackbuf->byte_buffer.data;
    image_depth_buffer_ = stackbuf->float_z_buffer.data;
    if (stackbuf->float_z_buffer.data) {
      depth_buffer_ = new MemoryBuffer(stackbuf->float_z_buffer.data, 1, stackbuf->x, stackbuf->y);
    }
    imagewidth_ = stackbuf->x;
    imageheight_ = stackbuf->y;
    number_of_channels_ = stackbuf->channels;
  }
}

void BaseImageOperation::deinit_execution()
{
  image_float_buffer_ = nullptr;
  image_byte_buffer_ = nullptr;
  BKE_image_release_ibuf(image_, buffer_, nullptr);
  if (depth_buffer_) {
    delete depth_buffer_;
    depth_buffer_ = nullptr;
  }
}

void BaseImageOperation::determine_canvas(const rcti & /*preferred_area*/, rcti &r_area)
{
  ImBuf *stackbuf = get_im_buf();

  r_area = COM_AREA_NONE;

  if (stackbuf) {
    BLI_rcti_init(&r_area, 0, stackbuf->x, 0, stackbuf->y);
  }

  BKE_image_release_ibuf(image_, stackbuf, nullptr);
}

static void sample_image_at_location(
    ImBuf *ibuf, float x, float y, PixelSampler sampler, bool make_linear_rgb, float color[4])
{
  if (ibuf->float_buffer.data) {
    switch (sampler) {
      case PixelSampler::Nearest:
        nearest_interpolation_color(ibuf, nullptr, color, x, y);
        break;
      case PixelSampler::Bilinear:
        bilinear_interpolation_color(ibuf, nullptr, color, x, y);
        break;
      case PixelSampler::Bicubic:
        bicubic_interpolation_color(ibuf, nullptr, color, x, y);
        break;
    }
  }
  else {
    uchar byte_color[4];
    switch (sampler) {
      case PixelSampler::Nearest:
        nearest_interpolation_color(ibuf, byte_color, nullptr, x, y);
        break;
      case PixelSampler::Bilinear:
        bilinear_interpolation_color(ibuf, byte_color, nullptr, x, y);
        break;
      case PixelSampler::Bicubic:
        bicubic_interpolation_color(ibuf, byte_color, nullptr, x, y);
        break;
    }
    rgba_uchar_to_float(color, byte_color);
    if (make_linear_rgb) {
      IMB_colormanagement_colorspace_to_scene_linear_v4(color, false, ibuf->rect_colorspace);
    }
  }
}

void ImageOperation::execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler)
{
  int ix = x, iy = y;
  if (image_float_buffer_ == nullptr && image_byte_buffer_ == nullptr) {
    zero_v4(output);
  }
  else if (ix < 0 || iy < 0 || ix >= buffer_->x || iy >= buffer_->y) {
    zero_v4(output);
  }
  else {
    sample_image_at_location(buffer_, x, y, sampler, true, output);
  }
}

void ImageOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                  const rcti &area,
                                                  Span<MemoryBuffer *> /*inputs*/)
{
  output->copy_from(buffer_, area, true);
}

void ImageAlphaOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler sampler)
{
  float tempcolor[4];

  if (image_float_buffer_ == nullptr && image_byte_buffer_ == nullptr) {
    output[0] = 0.0f;
  }
  else {
    tempcolor[3] = 1.0f;
    sample_image_at_location(buffer_, x, y, sampler, false, tempcolor);
    output[0] = tempcolor[3];
  }
}

void ImageAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> /*inputs*/)
{
  output->copy_from(buffer_, area, 3, COM_DATA_TYPE_VALUE_CHANNELS, 0);
}

void ImageDepthOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler /*sampler*/)
{
  if (image_depth_buffer_ == nullptr) {
    output[0] = 0.0f;
  }
  else {
    if (x < 0 || y < 0 || x >= this->get_width() || y >= this->get_height()) {
      output[0] = 0.0f;
    }
    else {
      int offset = y * get_width() + x;
      output[0] = image_depth_buffer_[offset];
    }
  }
}

void ImageDepthOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> /*inputs*/)
{
  if (depth_buffer_) {
    output->copy_from(depth_buffer_, area);
  }
  else {
    output->fill(area, COM_VALUE_ZERO);
  }
}

}  // namespace blender::compositor
