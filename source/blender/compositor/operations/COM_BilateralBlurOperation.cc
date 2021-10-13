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

#include "COM_BilateralBlurOperation.h"

namespace blender::compositor {

BilateralBlurOperation::BilateralBlurOperation()
{
  this->addInputSocket(DataType::Color);
  this->addInputSocket(DataType::Color);
  this->addOutputSocket(DataType::Color);
  this->flags.complex = true;

  inputColorProgram_ = nullptr;
  inputDeterminatorProgram_ = nullptr;
}

void BilateralBlurOperation::initExecution()
{
  inputColorProgram_ = getInputSocketReader(0);
  inputDeterminatorProgram_ = getInputSocketReader(1);
  QualityStepHelper::initExecution(COM_QH_INCREASE);
}

void BilateralBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
  /* Read the determinator color at x, y,
   * this will be used as the reference color for the determinator. */
  float determinatorReferenceColor[4];
  float determinator[4];
  float tempColor[4];
  float blurColor[4];
  float blurDivider;
  float space = space_;
  float sigmacolor = data_->sigma_color;
  int minx = floor(x - space);
  int maxx = ceil(x + space);
  int miny = floor(y - space);
  int maxy = ceil(y + space);
  float deltaColor;
  inputDeterminatorProgram_->read(determinatorReferenceColor, x, y, data);

  zero_v4(blurColor);
  blurDivider = 0.0f;
  /* TODO(sergey): This isn't really good bilateral filter, it should be
   * using gaussian bell for weights. Also sigma_color doesn't seem to be
   * used correct at all.
   */
  for (int yi = miny; yi < maxy; yi += QualityStepHelper::getStep()) {
    for (int xi = minx; xi < maxx; xi += QualityStepHelper::getStep()) {
      /* Read determinator. */
      inputDeterminatorProgram_->read(determinator, xi, yi, data);
      deltaColor = (fabsf(determinatorReferenceColor[0] - determinator[0]) +
                    fabsf(determinatorReferenceColor[1] - determinator[1]) +
                    /* Do not take the alpha channel into account. */
                    fabsf(determinatorReferenceColor[2] - determinator[2]));
      if (deltaColor < sigmacolor) {
        /* Add this to the blur. */
        inputColorProgram_->read(tempColor, xi, yi, data);
        add_v4_v4(blurColor, tempColor);
        blurDivider += 1.0f;
      }
    }
  }

  if (blurDivider > 0.0f) {
    mul_v4_v4fl(output, blurColor, 1.0f / blurDivider);
  }
  else {
    output[0] = 0.0f;
    output[1] = 0.0f;
    output[2] = 0.0f;
    output[3] = 1.0f;
  }
}

void BilateralBlurOperation::deinitExecution()
{
  inputColorProgram_ = nullptr;
  inputDeterminatorProgram_ = nullptr;
}

bool BilateralBlurOperation::determineDependingAreaOfInterest(rcti *input,
                                                              ReadBufferOperation *readOperation,
                                                              rcti *output)
{
  rcti newInput;
  int add = ceil(space_) + 1;

  newInput.xmax = input->xmax + (add);
  newInput.xmin = input->xmin - (add);
  newInput.ymax = input->ymax + (add);
  newInput.ymin = input->ymin - (add);

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

void BilateralBlurOperation::get_area_of_interest(const int UNUSED(input_idx),
                                                  const rcti &output_area,
                                                  rcti &r_input_area)
{
  const int add = ceil(space_) + 1;

  r_input_area.xmax = output_area.xmax + (add);
  r_input_area.xmin = output_area.xmin - (add);
  r_input_area.ymax = output_area.ymax + (add);
  r_input_area.ymin = output_area.ymin - (add);
}

struct PixelCursor {
  MemoryBuffer *input_determinator;
  MemoryBuffer *input_color;
  int step;
  float sigma_color;
  const float *determ_reference_color;
  float temp_color[4];
  float *out;
  int min_x, max_x;
  int min_y, max_y;
};

static void blur_pixel(PixelCursor &p)
{
  float blur_divider = 0.0f;
  zero_v4(p.out);

  /* TODO(sergey): This isn't really good bilateral filter, it should be
   * using gaussian bell for weights. Also sigma_color doesn't seem to be
   * used correct at all.
   */
  for (int yi = p.min_y; yi < p.max_y; yi += p.step) {
    for (int xi = p.min_x; xi < p.max_x; xi += p.step) {
      p.input_determinator->read(p.temp_color, xi, yi);
      /* Do not take the alpha channel into account. */
      const float delta_color = (fabsf(p.determ_reference_color[0] - p.temp_color[0]) +
                                 fabsf(p.determ_reference_color[1] - p.temp_color[1]) +
                                 fabsf(p.determ_reference_color[2] - p.temp_color[2]));
      if (delta_color < p.sigma_color) {
        /* Add this to the blur. */
        p.input_color->read(p.temp_color, xi, yi);
        add_v4_v4(p.out, p.temp_color);
        blur_divider += 1.0f;
      }
    }
  }

  if (blur_divider > 0.0f) {
    mul_v4_fl(p.out, 1.0f / blur_divider);
  }
  else {
    copy_v4_v4(p.out, COM_COLOR_BLACK);
  }
}

void BilateralBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                          const rcti &area,
                                                          Span<MemoryBuffer *> inputs)
{
  PixelCursor p = {};
  p.step = QualityStepHelper::getStep();
  p.sigma_color = data_->sigma_color;
  p.input_color = inputs[0];
  p.input_determinator = inputs[1];
  const float space = space_;
  for (int y = area.ymin; y < area.ymax; y++) {
    p.out = output->get_elem(area.xmin, y);
    /* This will be used as the reference color for the determinator. */
    p.determ_reference_color = p.input_determinator->get_elem(area.xmin, y);
    p.min_y = floor(y - space);
    p.max_y = ceil(y + space);
    for (int x = area.xmin; x < area.xmax; x++) {
      p.min_x = floor(x - space);
      p.max_x = ceil(x + space);

      blur_pixel(p);

      p.determ_reference_color += p.input_determinator->elem_stride;
      p.out += output->elem_stride;
    }
  }
}

}  // namespace blender::compositor
