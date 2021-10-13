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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_MovieClipOperation.h"

#include "BKE_image.h"
#include "BKE_movieclip.h"

#include "IMB_imbuf.h"

namespace blender::compositor {

MovieClipBaseOperation::MovieClipBaseOperation()
{
  movieClip_ = nullptr;
  movieClipBuffer_ = nullptr;
  movieClipUser_ = nullptr;
  movieClipwidth_ = 0;
  movieClipheight_ = 0;
  framenumber_ = 0;
}

void MovieClipBaseOperation::initExecution()
{
  if (movieClip_) {
    BKE_movieclip_user_set_frame(movieClipUser_, framenumber_);
    ImBuf *ibuf;

    if (cacheFrame_) {
      ibuf = BKE_movieclip_get_ibuf(movieClip_, movieClipUser_);
    }
    else {
      ibuf = BKE_movieclip_get_ibuf_flag(
          movieClip_, movieClipUser_, movieClip_->flag, MOVIECLIP_CACHE_SKIP);
    }

    if (ibuf) {
      movieClipBuffer_ = ibuf;
      if (ibuf->rect_float == nullptr || ibuf->userflags & IB_RECT_INVALID) {
        IMB_float_from_rect(ibuf);
        ibuf->userflags &= ~IB_RECT_INVALID;
      }
    }
  }
}

void MovieClipBaseOperation::deinitExecution()
{
  if (movieClipBuffer_) {
    IMB_freeImBuf(movieClipBuffer_);

    movieClipBuffer_ = nullptr;
  }
}

void MovieClipBaseOperation::determine_canvas(const rcti &UNUSED(preferred_area), rcti &r_area)
{
  r_area = COM_AREA_NONE;
  if (movieClip_) {
    int width, height;
    BKE_movieclip_get_size(movieClip_, movieClipUser_, &width, &height);
    BLI_rcti_init(&r_area, 0, width, 0, height);
  }
}

void MovieClipBaseOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  ImBuf *ibuf = movieClipBuffer_;

  if (ibuf == nullptr) {
    zero_v4(output);
  }
  else if (ibuf->rect == nullptr && ibuf->rect_float == nullptr) {
    /* Happens for multilayer exr, i.e. */
    zero_v4(output);
  }
  else {
    switch (sampler) {
      case PixelSampler::Nearest:
        nearest_interpolation_color(ibuf, nullptr, output, x, y);
        break;
      case PixelSampler::Bilinear:
        bilinear_interpolation_color(ibuf, nullptr, output, x, y);
        break;
      case PixelSampler::Bicubic:
        bicubic_interpolation_color(ibuf, nullptr, output, x, y);
        break;
    }
  }
}

void MovieClipBaseOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> UNUSED(inputs))
{
  if (movieClipBuffer_) {
    output->copy_from(movieClipBuffer_, area);
  }
  else {
    output->fill(area, COM_COLOR_TRANSPARENT);
  }
}

MovieClipOperation::MovieClipOperation() : MovieClipBaseOperation()
{
  this->addOutputSocket(DataType::Color);
}

MovieClipAlphaOperation::MovieClipAlphaOperation() : MovieClipBaseOperation()
{
  this->addOutputSocket(DataType::Value);
}

void MovieClipAlphaOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  float result[4];
  MovieClipBaseOperation::executePixelSampled(result, x, y, sampler);
  output[0] = result[3];
}

void MovieClipAlphaOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> UNUSED(inputs))
{
  if (movieClipBuffer_) {
    output->copy_from(movieClipBuffer_, area, 3, COM_DATA_TYPE_VALUE_CHANNELS, 0);
  }
  else {
    output->fill(area, COM_VALUE_ZERO);
  }
}

}  // namespace blender::compositor
