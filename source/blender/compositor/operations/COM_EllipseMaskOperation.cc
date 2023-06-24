/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_EllipseMaskOperation.h"

namespace blender::compositor {

EllipseMaskOperation::EllipseMaskOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  input_mask_ = nullptr;
  input_value_ = nullptr;
  cosine_ = 0.0f;
  sine_ = 0.0f;
}
void EllipseMaskOperation::init_execution()
{
  input_mask_ = this->get_input_socket_reader(0);
  input_value_ = this->get_input_socket_reader(1);
  const double rad = double(data_->rotation);
  cosine_ = cos(rad);
  sine_ = sin(rad);
  aspect_ratio_ = float(this->get_width()) / this->get_height();
}

void EllipseMaskOperation::execute_pixel_sampled(float output[4],
                                                 float x,
                                                 float y,
                                                 PixelSampler sampler)
{
  float input_mask[4];
  float input_value[4];

  float rx = x / MAX2(this->get_width() - 1.0f, FLT_EPSILON);
  float ry = y / MAX2(this->get_height() - 1.0f, FLT_EPSILON);

  const float dy = (ry - data_->y) / aspect_ratio_;
  const float dx = rx - data_->x;
  rx = data_->x + (cosine_ * dx + sine_ * dy);
  ry = data_->y + (-sine_ * dx + cosine_ * dy);

  input_mask_->read_sampled(input_mask, x, y, sampler);
  input_value_->read_sampled(input_value, x, y, sampler);

  const float half_height = (data_->height) / 2.0f;
  const float half_width = data_->width / 2.0f;
  float sx = rx - data_->x;
  sx *= sx;
  const float tx = half_width * half_width;
  float sy = ry - data_->y;
  sy *= sy;
  const float ty = half_height * half_height;

  bool inside = ((sx / tx) + (sy / ty)) <= (1.0f + FLT_EPSILON);

  switch (mask_type_) {
    case CMP_NODE_MASKTYPE_ADD:
      if (inside) {
        output[0] = MAX2(input_mask[0], input_value[0]);
      }
      else {
        output[0] = input_mask[0];
      }
      break;
    case CMP_NODE_MASKTYPE_SUBTRACT:
      if (inside) {
        output[0] = input_mask[0] - input_value[0];
        CLAMP(output[0], 0, 1);
      }
      else {
        output[0] = input_mask[0];
      }
      break;
    case CMP_NODE_MASKTYPE_MULTIPLY:
      if (inside) {
        output[0] = input_mask[0] * input_value[0];
      }
      else {
        output[0] = 0;
      }
      break;
    case CMP_NODE_MASKTYPE_NOT:
      if (inside) {
        if (input_mask[0] > 0.0f) {
          output[0] = 0;
        }
        else {
          output[0] = input_value[0];
        }
      }
      else {
        output[0] = input_mask[0];
      }
      break;
  }
}

void EllipseMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  MaskFunc mask_func;
  switch (mask_type_) {
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
  const float op_last_x = MAX2(this->get_width() - 1.0f, FLT_EPSILON);
  const float op_last_y = MAX2(this->get_height() - 1.0f, FLT_EPSILON);
  const float half_w = data_->width / 2.0f;
  const float half_h = data_->height / 2.0f;
  const float tx = half_w * half_w;
  const float ty = half_h * half_h;
  for (int y = area.ymin; y < area.ymax; y++) {
    const float op_ry = y / op_last_y;
    const float dy = (op_ry - data_->y) / aspect_ratio_;
    float *out = output->get_elem(area.xmin, y);
    const float *mask = input_mask->get_elem(area.xmin, y);
    const float *value = input_value->get_elem(area.xmin, y);
    for (int x = area.xmin; x < area.xmax; x++) {
      const float op_rx = x / op_last_x;
      const float dx = op_rx - data_->x;
      const float rx = data_->x + (cosine_ * dx + sine_ * dy);
      const float ry = data_->y + (-sine_ * dx + cosine_ * dy);
      float sx = rx - data_->x;
      sx *= sx;
      float sy = ry - data_->y;
      sy *= sy;
      const bool inside = ((sx / tx) + (sy / ty)) <= (1.0f + FLT_EPSILON);
      out[0] = mask_func(inside, mask, value);

      mask += input_mask->elem_stride;
      value += input_value->elem_stride;
      out += output->elem_stride;
    }
  }
}

void EllipseMaskOperation::deinit_execution()
{
  input_mask_ = nullptr;
  input_value_ = nullptr;
}

}  // namespace blender::compositor
