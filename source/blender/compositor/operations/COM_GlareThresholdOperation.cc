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

#include "COM_GlareThresholdOperation.h"

#include "IMB_colormanagement.h"

namespace blender::compositor {

GlareThresholdOperation::GlareThresholdOperation()
{
  this->addInputSocket(DataType::Color, ResizeMode::FitAny);
  this->addOutputSocket(DataType::Color);
  inputProgram_ = nullptr;
}

void GlareThresholdOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  NodeOperation::determine_canvas(preferred_area, r_area);
  const int width = BLI_rcti_size_x(&r_area) / (1 << settings_->quality);
  const int height = BLI_rcti_size_y(&r_area) / (1 << settings_->quality);
  r_area.xmax = r_area.xmin + width;
  r_area.ymax = r_area.ymin + height;
}

void GlareThresholdOperation::initExecution()
{
  inputProgram_ = this->getInputSocketReader(0);
}

void GlareThresholdOperation::executePixelSampled(float output[4],
                                                  float x,
                                                  float y,
                                                  PixelSampler sampler)
{
  const float threshold = settings_->threshold;

  inputProgram_->readSampled(output, x, y, sampler);
  if (IMB_colormanagement_get_luminance(output) >= threshold) {
    output[0] -= threshold;
    output[1] -= threshold;
    output[2] -= threshold;

    output[0] = MAX2(output[0], 0.0f);
    output[1] = MAX2(output[1], 0.0f);
    output[2] = MAX2(output[2], 0.0f);
  }
  else {
    zero_v3(output);
  }
}

void GlareThresholdOperation::deinitExecution()
{
  inputProgram_ = nullptr;
}

void GlareThresholdOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  const float threshold = settings_->threshold;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float *color = it.in(0);
    if (IMB_colormanagement_get_luminance(color) >= threshold) {
      it.out[0] = color[0] - threshold;
      it.out[1] = color[1] - threshold;
      it.out[2] = color[2] - threshold;

      CLAMP3_MIN(it.out, 0.0f);
    }
    else {
      zero_v3(it.out);
    }
  }
}

}  // namespace blender::compositor
