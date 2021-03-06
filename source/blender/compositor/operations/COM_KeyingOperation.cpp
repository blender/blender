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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_KeyingOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

static float get_pixel_saturation(const float pixelColor[4],
                                  float screen_balance,
                                  int primary_channel)
{
  const int other_1 = (primary_channel + 1) % 3;
  const int other_2 = (primary_channel + 2) % 3;

  const int min_channel = MIN2(other_1, other_2);
  const int max_channel = MAX2(other_1, other_2);

  const float val = screen_balance * pixelColor[min_channel] +
                    (1.0f - screen_balance) * pixelColor[max_channel];

  return (pixelColor[primary_channel] - val) * fabsf(1.0f - val);
}

KeyingOperation::KeyingOperation()
{
  this->addInputSocket(COM_DT_COLOR);
  this->addInputSocket(COM_DT_COLOR);
  this->addOutputSocket(COM_DT_VALUE);

  this->m_screenBalance = 0.5f;

  this->m_pixelReader = nullptr;
  this->m_screenReader = nullptr;
}

void KeyingOperation::initExecution()
{
  this->m_pixelReader = this->getInputSocketReader(0);
  this->m_screenReader = this->getInputSocketReader(1);
}

void KeyingOperation::deinitExecution()
{
  this->m_pixelReader = nullptr;
  this->m_screenReader = nullptr;
}

void KeyingOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
  float pixel_color[4];
  float screen_color[4];

  this->m_pixelReader->readSampled(pixel_color, x, y, sampler);
  this->m_screenReader->readSampled(screen_color, x, y, sampler);

  const int primary_channel = max_axis_v3(screen_color);
  const float min_pixel_color = min_fff(pixel_color[0], pixel_color[1], pixel_color[2]);

  if (min_pixel_color > 1.0f) {
    /* overexposure doesn't happen on screen itself and usually happens
     * on light sources in the shot, this need to be checked separately
     * because saturation and falloff calculation is based on the fact
     * that pixels are not overexposed
     */
    output[0] = 1.0f;
  }
  else {
    float saturation = get_pixel_saturation(pixel_color, this->m_screenBalance, primary_channel);
    float screen_saturation = get_pixel_saturation(
        screen_color, this->m_screenBalance, primary_channel);

    if (saturation < 0) {
      /* means main channel of pixel is different from screen,
       * assume this is completely a foreground
       */
      output[0] = 1.0f;
    }
    else if (saturation >= screen_saturation) {
      /* matched main channels and higher saturation on pixel
       * is treated as completely background
       */
      output[0] = 0.0f;
    }
    else {
      /* nice alpha falloff on edges */
      float distance = 1.0f - saturation / screen_saturation;

      output[0] = distance;
    }
  }
}
