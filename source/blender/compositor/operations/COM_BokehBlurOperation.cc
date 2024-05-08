/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"

#include "COM_BokehBlurOperation.h"
#include "COM_ConstantOperation.h"

namespace blender::compositor {

constexpr int IMAGE_INPUT_INDEX = 0;
constexpr int BOKEH_INPUT_INDEX = 1;
constexpr int BOUNDING_BOX_INPUT_INDEX = 2;
constexpr int SIZE_INPUT_INDEX = 3;

BokehBlurOperation::BokehBlurOperation()
{
  this->add_input_socket(DataType::Color);
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Color);

  flags_.can_be_constant = true;

  size_ = 1.0f;
  sizeavailable_ = false;

  extend_bounds_ = false;
}

void BokehBlurOperation::init_data()
{
  update_size();
}

void BokehBlurOperation::update_size()
{
  if (sizeavailable_) {
    return;
  }

  NodeOperation *size_input = get_input_operation(SIZE_INPUT_INDEX);
  if (size_input->get_flags().is_constant_operation) {
    size_ = *static_cast<ConstantOperation *>(size_input)->get_constant_elem();
    CLAMP(size_, 0.0f, 10.0f);
  } /* Else use default. */
  sizeavailable_ = true;
}

void BokehBlurOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (!extend_bounds_) {
    NodeOperation::determine_canvas(preferred_area, r_area);
    return;
  }

  set_determined_canvas_modifier([=](rcti &canvas) {
    const float max_dim = std::max(BLI_rcti_size_x(&canvas), BLI_rcti_size_y(&canvas));
    /* Rounding to even prevents image jiggling in backdrop while switching size values. */
    float add_size = round_to_even(2 * size_ * max_dim / 100.0f);
    canvas.xmax += add_size;
    canvas.ymax += add_size;
  });
  NodeOperation::determine_canvas(preferred_area, r_area);
}

void BokehBlurOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX: {
      const float max_dim = std::max(this->get_width(), this->get_height());
      const float add_size = size_ * max_dim / 100.0f;
      r_input_area.xmin = output_area.xmin - add_size;
      r_input_area.xmax = output_area.xmax + add_size;
      r_input_area.ymin = output_area.ymin - add_size;
      r_input_area.ymax = output_area.ymax + add_size;
      break;
    }
    case BOKEH_INPUT_INDEX: {
      NodeOperation *bokeh_input = get_input_operation(BOKEH_INPUT_INDEX);
      r_input_area = bokeh_input->get_canvas();
      break;
    }
    case BOUNDING_BOX_INPUT_INDEX:
      r_input_area = output_area;
      break;
    case SIZE_INPUT_INDEX: {
      r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
      break;
    }
  }
}

void BokehBlurOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  const float max_dim = std::max(this->get_width(), this->get_height());
  const int radius = size_ * max_dim / 100.0f;

  const MemoryBuffer *image_input = inputs[IMAGE_INPUT_INDEX];
  const MemoryBuffer *bokeh_input = inputs[BOKEH_INPUT_INDEX];
  const int2 bokeh_size = int2(bokeh_input->get_width(), bokeh_input->get_height());
  MemoryBuffer *bounding_input = inputs[BOUNDING_BOX_INPUT_INDEX];
  BuffersIterator<float> it = output->iterate_with({bounding_input}, area);
  for (; !it.is_end(); ++it) {
    const int x = it.x;
    const int y = it.y;
    const float bounding_box = *it.in(0);
    if (bounding_box <= 0.0f) {
      image_input->read_elem(x, y, it.out);
      continue;
    }

    float4 accumulated_color = float4(0.0f);
    float4 accumulated_weight = float4(0.0f);
    for (int yi = -radius; yi <= radius; ++yi) {
      for (int xi = -radius; xi <= radius; ++xi) {
        const float2 normalized_texel = (float2(xi, yi) + radius + 0.5f) / (radius * 2.0f + 1.0f);
        const float2 weight_texel = (1.0f - normalized_texel) * float2(bokeh_size - 1);
        const float4 weight = bokeh_input->get_elem(int(weight_texel.x), int(weight_texel.y));
        const float4 color = float4(image_input->get_elem_clamped(x + xi, y + yi)) * weight;
        accumulated_color += color;
        accumulated_weight += weight;
      }
    }

    const float4 final_color = math::safe_divide(accumulated_color, accumulated_weight);
    copy_v4_v4(it.out, final_color);
  }
}

}  // namespace blender::compositor
