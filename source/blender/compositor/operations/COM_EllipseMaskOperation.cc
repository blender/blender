/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_EllipseMaskOperation.h"

namespace blender::compositor {

EllipseMaskOperation::EllipseMaskOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  cosine_ = 0.0f;
  sine_ = 0.0f;
}
void EllipseMaskOperation::init_execution()
{
  const double rad = double(data_->rotation);
  cosine_ = cos(rad);
  sine_ = sin(rad);
  aspect_ratio_ = float(this->get_width()) / this->get_height();
}

void EllipseMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                        const rcti &area,
                                                        Span<MemoryBuffer *> inputs)
{
  MaskFunc mask_func;
  switch (mask_type_) {
    case CMP_NODE_MASKTYPE_ADD:
      mask_func = [](const bool is_inside, const float *mask, const float *value) {
        return is_inside ? std::max(mask[0], value[0]) : mask[0];
      };
      break;
    case CMP_NODE_MASKTYPE_SUBTRACT:
      mask_func = [](const bool is_inside, const float *mask, const float *value) {
        return is_inside ? std::clamp(mask[0] - value[0], 0.0f, 1.0f) : mask[0];
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
  const float op_last_x = std::max(this->get_width() - 1.0f, FLT_EPSILON);
  const float op_last_y = std::max(this->get_height() - 1.0f, FLT_EPSILON);
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

}  // namespace blender::compositor
