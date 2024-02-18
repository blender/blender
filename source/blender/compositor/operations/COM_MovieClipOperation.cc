/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_MovieClipOperation.h"

#include "BKE_image.h"
#include "BKE_movieclip.h"

#include "IMB_imbuf.hh"
#include "IMB_interp.hh"

namespace blender::compositor {

MovieClipBaseOperation::MovieClipBaseOperation()
{
  movie_clip_ = nullptr;
  movie_clip_buffer_ = nullptr;
  movie_clip_user_ = nullptr;
  movie_clipwidth_ = 0;
  movie_clipheight_ = 0;
  framenumber_ = 0;
}

void MovieClipBaseOperation::init_execution()
{
  if (movie_clip_) {
    BKE_movieclip_user_set_frame(movie_clip_user_, framenumber_);
    ImBuf *ibuf;

    if (cache_frame_) {
      ibuf = BKE_movieclip_get_ibuf(movie_clip_, movie_clip_user_);
    }
    else {
      ibuf = BKE_movieclip_get_ibuf_flag(
          movie_clip_, movie_clip_user_, movie_clip_->flag, MOVIECLIP_CACHE_SKIP);
    }

    if (ibuf) {
      movie_clip_buffer_ = ibuf;
      if (ibuf->float_buffer.data == nullptr || ibuf->userflags & IB_RECT_INVALID) {
        IMB_float_from_rect(ibuf);
        ibuf->userflags &= ~IB_RECT_INVALID;
      }
    }
  }
}

void MovieClipBaseOperation::deinit_execution()
{
  if (movie_clip_buffer_) {
    IMB_freeImBuf(movie_clip_buffer_);

    movie_clip_buffer_ = nullptr;
  }
}

void MovieClipBaseOperation::determine_canvas(const rcti & /*preferred_area*/, rcti &r_area)
{
  r_area = COM_AREA_NONE;
  if (movie_clip_) {
    int width, height;
    BKE_movieclip_get_size(movie_clip_, movie_clip_user_, &width, &height);
    BLI_rcti_init(&r_area, 0, width, 0, height);
  }
}

void MovieClipBaseOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  ImBuf *ibuf = movie_clip_buffer_;

  if (ibuf == nullptr) {
    zero_v4(output);
  }
  else if (ibuf->byte_buffer.data == nullptr && ibuf->float_buffer.data == nullptr) {
    /* Happens for multi-layer EXR, i.e. */
    zero_v4(output);
  }
  else {
    switch (sampler) {
      case PixelSampler::Nearest:
        imbuf::interpolate_nearest_fl(ibuf, output, x, y);
        break;
      case PixelSampler::Bilinear:
        imbuf::interpolate_bilinear_border_fl(ibuf, output, x, y);
        break;
      case PixelSampler::Bicubic:
        imbuf::interpolate_cubic_bspline_fl(ibuf, output, x, y);
        break;
    }
  }
}

void MovieClipBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> /*inputs*/)
{
  if (movie_clip_buffer_) {
    output->copy_from(movie_clip_buffer_, area);
  }
  else {
    output->fill(area, COM_COLOR_TRANSPARENT);
  }
}

MovieClipOperation::MovieClipOperation() : MovieClipBaseOperation()
{
  this->add_output_socket(DataType::Color);
}

MovieClipAlphaOperation::MovieClipAlphaOperation() : MovieClipBaseOperation()
{
  this->add_output_socket(DataType::Value);
}

void MovieClipAlphaOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  float result[4];
  MovieClipBaseOperation::execute_pixel_sampled(result, x, y, sampler);
  output[0] = result[3];
}

void MovieClipAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> /*inputs*/)
{
  if (movie_clip_buffer_) {
    output->copy_from(movie_clip_buffer_, area, 3, COM_DATA_TYPE_VALUE_CHANNELS, 0);
  }
  else {
    output->fill(area, COM_VALUE_ZERO);
  }
}

}  // namespace blender::compositor
