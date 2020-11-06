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

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_image.h"
#include "BKE_movieclip.h"

#include "IMB_imbuf.h"

MovieClipBaseOperation::MovieClipBaseOperation()
{
  this->m_movieClip = nullptr;
  this->m_movieClipBuffer = nullptr;
  this->m_movieClipUser = nullptr;
  this->m_movieClipwidth = 0;
  this->m_movieClipheight = 0;
  this->m_framenumber = 0;
}

void MovieClipBaseOperation::initExecution()
{
  if (this->m_movieClip) {
    BKE_movieclip_user_set_frame(this->m_movieClipUser, this->m_framenumber);
    ImBuf *ibuf;

    if (this->m_cacheFrame) {
      ibuf = BKE_movieclip_get_ibuf(this->m_movieClip, this->m_movieClipUser);
    }
    else {
      ibuf = BKE_movieclip_get_ibuf_flag(
          this->m_movieClip, this->m_movieClipUser, this->m_movieClip->flag, MOVIECLIP_CACHE_SKIP);
    }

    if (ibuf) {
      this->m_movieClipBuffer = ibuf;
      if (ibuf->rect_float == nullptr || ibuf->userflags & IB_RECT_INVALID) {
        IMB_float_from_rect(ibuf);
        ibuf->userflags &= ~IB_RECT_INVALID;
      }
    }
  }
}

void MovieClipBaseOperation::deinitExecution()
{
  if (this->m_movieClipBuffer) {
    IMB_freeImBuf(this->m_movieClipBuffer);

    this->m_movieClipBuffer = nullptr;
  }
}

void MovieClipBaseOperation::determineResolution(unsigned int resolution[2],
                                                 unsigned int /*preferredResolution*/[2])
{
  resolution[0] = 0;
  resolution[1] = 0;

  if (this->m_movieClip) {
    int width, height;

    BKE_movieclip_get_size(this->m_movieClip, this->m_movieClipUser, &width, &height);

    resolution[0] = width;
    resolution[1] = height;
  }
}

void MovieClipBaseOperation::executePixelSampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  ImBuf *ibuf = this->m_movieClipBuffer;

  if (ibuf == nullptr) {
    zero_v4(output);
  }
  else if (ibuf->rect == nullptr && ibuf->rect_float == nullptr) {
    /* Happens for multilayer exr, i.e. */
    zero_v4(output);
  }
  else {
    switch (sampler) {
      case COM_PS_NEAREST:
        nearest_interpolation_color(ibuf, nullptr, output, x, y);
        break;
      case COM_PS_BILINEAR:
        bilinear_interpolation_color(ibuf, nullptr, output, x, y);
        break;
      case COM_PS_BICUBIC:
        bicubic_interpolation_color(ibuf, nullptr, output, x, y);
        break;
    }
  }
}

MovieClipOperation::MovieClipOperation() : MovieClipBaseOperation()
{
  this->addOutputSocket(COM_DT_COLOR);
}

MovieClipAlphaOperation::MovieClipAlphaOperation() : MovieClipBaseOperation()
{
  this->addOutputSocket(COM_DT_VALUE);
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
