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

#include "COM_EllipseMaskOperation.h"

namespace blender::compositor {

EllipseMaskOperation::EllipseMaskOperation()
{
  this->addInputSocket(DataType::Value);
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);
  inputMask_ = nullptr;
  inputValue_ = nullptr;
  cosine_ = 0.0f;
  sine_ = 0.0f;
}
void EllipseMaskOperation::initExecution()
{
  inputMask_ = this->getInputSocketReader(0);
  inputValue_ = this->getInputSocketReader(1);
  const double rad = (double)data_->rotation;
  cosine_ = cos(rad);
  sine_ = sin(rad);
  aspectRatio_ = ((float)this->getWidth()) / this->getHeight();
}

void EllipseMaskOperation::executePixelSampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler sampler)
{
  float inputMask[4];
  float inputValue[4];

  float rx = x / this->getWidth();
  float ry = y / this->getHeight();

  const float dy = (ry - data_->y) / aspectRatio_;
  const float dx = rx - data_->x;
  rx = data_->x + (cosine_ * dx + sine_ * dy);
  ry = data_->y + (-sine_ * dx + cosine_ * dy);

  inputMask_->readSampled(inputMask, x, y, sampler);
  inputValue_->readSampled(inputValue, x, y, sampler);

  const float halfHeight = (data_->height) / 2.0f;
  const float halfWidth = data_->width / 2.0f;
  float sx = rx - data_->x;
  sx *= sx;
  const float tx = halfWidth * halfWidth;
  float sy = ry - data_->y;
  sy *= sy;
  const float ty = halfHeight * halfHeight;

  bool inside = ((sx / tx) + (sy / ty)) < 1.0f;

  switch (maskType_) {
    case CMP_NODE_MASKTYPE_ADD:
      if (inside) {
        output[0] = MAX2(inputMask[0], inputValue[0]);
      }
      else {
        output[0] = inputMask[0];
      }
      break;
    case CMP_NODE_MASKTYPE_SUBTRACT:
      if (inside) {
        output[0] = inputMask[0] - inputValue[0];
        CLAMP(output[0], 0, 1);
      }
      else {
        output[0] = inputMask[0];
      }
      break;
    case CMP_NODE_MASKTYPE_MULTIPLY:
      if (inside) {
        output[0] = inputMask[0] * inputValue[0];
      }
      else {
        output[0] = 0;
      }
      break;
    case CMP_NODE_MASKTYPE_NOT:
      if (inside) {
        if (inputMask[0] > 0.0f) {
          output[0] = 0;
        }
        else {
          output[0] = inputValue[0];
        }
      }
      else {
        output[0] = inputMask[0];
      }
      break;
  }
}

void EllipseMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  MaskFunc mask_func;
  switch (maskType_) {
    case CMP_NODE_MASKTYPE_ADD:
      mask_func = [](const bool is_inside, const float *mask, const float *value) {
        return is_inside ? MAX2(mask[0], value[0]) : mask[0];
      };
      break;
    case CMP_NODE_MASKTYPE_SUBTRACT:
      mask_func = [](const bool is_inside, const float *mask, const float *value) {
        return is_inside ? CLAMPIS(mask[0] - value[0], 0, 1) : mask[0];
      };
      break;
    case CMP_NODE_MASKTYPE_MULTIPLY:
      mask_func = [](const bool is_inside, const float *mask, const float *value) {
        return is_inside ? mask[0] * value[0] : 0;
      };
      break;
    case CMP_NODE_MASKTYPE_NOT:
      mask_func = [](const bool is_inside, const float *mask, const float *value) {
        if (is_inside) {
          return mask[0] > 0.0f ? 0.0f : value[0];
        }
        return mask[0];
      };
      break;
  }
  apply_mask(output, area, inputs, mask_func);
}

void EllipseMaskOperation::apply_mask(MemoryBuffer *output,
                                      const rcti &area,
                                      Span<MemoryBuffer *> inputs,
                                      MaskFunc mask_func)
{
  const MemoryBuffer *input_mask = inputs[0];
  const MemoryBuffer *input_value = inputs[1];
  const float op_w = this->getWidth();
  const float op_h = this->getHeight();
  const float half_w = data_->width / 2.0f;
  const float half_h = data_->height / 2.0f;
  const float tx = half_w * half_w;
  const float ty = half_h * half_h;
  for (int y = area.ymin; y < area.ymax; y++) {
    const float op_ry = y / op_h;
    const float dy = (op_ry - data_->y) / aspectRatio_;
    float *out = output->get_elem(area.xmin, y);
    const float *mask = input_mask->get_elem(area.xmin, y);
    const float *value = input_value->get_elem(area.xmin, y);
    for (int x = area.xmin; x < area.xmax; x++) {
      const float op_rx = x / op_w;
      const float dx = op_rx - data_->x;
      const float rx = data_->x + (cosine_ * dx + sine_ * dy);
      const float ry = data_->y + (-sine_ * dx + cosine_ * dy);
      float sx = rx - data_->x;
      sx *= sx;
      float sy = ry - data_->y;
      sy *= sy;
      const bool inside = ((sx / tx) + (sy / ty)) < 1.0f;
      out[0] = mask_func(inside, mask, value);

      mask += input_mask->elem_stride;
      value += input_value->elem_stride;
      out += output->elem_stride;
    }
  }
}

void EllipseMaskOperation::deinitExecution()
{
  inputMask_ = nullptr;
  inputValue_ = nullptr;
}

}  // namespace blender::compositor
