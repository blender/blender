/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ImageOperation.h"

#include "BKE_scene.hh"

#include "IMB_colormanagement.hh"
#include "IMB_interp.hh"

namespace blender::compositor {

BaseImageOperation::BaseImageOperation()
{
  image_ = nullptr;
  buffer_ = nullptr;
  image_user_ = {};
  imagewidth_ = 0;
  imageheight_ = 0;
  framenumber_ = 0;
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

ImBuf *BaseImageOperation::get_im_buf()
{
  if (rd_ == nullptr || image_ == nullptr) {
    return nullptr;
  }

  ImBuf *ibuf = BKE_image_acquire_multilayer_view_ibuf(*rd_, *image_, image_user_, "", view_name_);
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
    imagewidth_ = stackbuf->x;
    imageheight_ = stackbuf->y;
    number_of_channels_ = stackbuf->channels;
  }
}

void BaseImageOperation::deinit_execution()
{
  BKE_image_release_ibuf(image_, buffer_, nullptr);
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

void ImageOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                  const rcti &area,
                                                  Span<MemoryBuffer *> /*inputs*/)
{
  const bool ensure_premultiplied = !ELEM(
      image_->alpha_mode, IMA_ALPHA_CHANNEL_PACKED, IMA_ALPHA_IGNORE);
  output->copy_from(buffer_, area, ensure_premultiplied, true);
}

void ImageAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> /*inputs*/)
{
  output->copy_from(buffer_, area, 3, COM_DATA_TYPE_VALUE_CHANNELS, 0);
}

}  // namespace blender::compositor
