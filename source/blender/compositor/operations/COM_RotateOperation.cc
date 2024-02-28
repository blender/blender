/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_RotateOperation.h"

#include "BLI_math_rotation.h"

namespace blender::compositor {

RotateOperation::RotateOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::None);
  this->add_input_socket(DataType::Value, ResizeMode::None);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
  do_degree2_rad_conversion_ = false;
  is_degree_set_ = false;
  sampler_ = PixelSampler::Bilinear;
  flags_.can_be_constant = true;
}

void RotateOperation::get_rotation_center(const rcti &area, float &r_x, float &r_y)
{
  r_x = (BLI_rcti_size_x(&area) - 1) / 2.0;
  r_y = (BLI_rcti_size_y(&area) - 1) / 2.0;
}

void RotateOperation::get_rotation_offset(const rcti &input_canvas,
                                          const rcti &rotate_canvas,
                                          float &r_offset_x,
                                          float &r_offset_y)
{
  r_offset_x = (BLI_rcti_size_x(&input_canvas) - BLI_rcti_size_x(&rotate_canvas)) / 2.0f;
  r_offset_y = (BLI_rcti_size_y(&input_canvas) - BLI_rcti_size_y(&rotate_canvas)) / 2.0f;
}

void RotateOperation::get_area_rotation_bounds(const rcti &area,
                                               const float center_x,
                                               const float center_y,
                                               const float sine,
                                               const float cosine,
                                               rcti &r_bounds)
{
  const float dxmin = area.xmin - center_x;
  const float dymin = area.ymin - center_y;
  const float dxmax = area.xmax - center_x;
  const float dymax = area.ymax - center_y;

  const float x1 = center_x + (cosine * dxmin + (-sine) * dymin);
  const float x2 = center_x + (cosine * dxmax + (-sine) * dymin);
  const float x3 = center_x + (cosine * dxmin + (-sine) * dymax);
  const float x4 = center_x + (cosine * dxmax + (-sine) * dymax);
  const float y1 = center_y + (sine * dxmin + cosine * dymin);
  const float y2 = center_y + (sine * dxmax + cosine * dymin);
  const float y3 = center_y + (sine * dxmin + cosine * dymax);
  const float y4 = center_y + (sine * dxmax + cosine * dymax);
  const float minx = std::min(x1, std::min(x2, std::min(x3, x4)));
  const float maxx = std::max(x1, std::max(x2, std::max(x3, x4)));
  const float miny = std::min(y1, std::min(y2, std::min(y3, y4)));
  const float maxy = std::max(y1, std::max(y2, std::max(y3, y4)));

  r_bounds.xmin = floor(minx);
  r_bounds.xmax = ceil(maxx);
  r_bounds.ymin = floor(miny);
  r_bounds.ymax = ceil(maxy);
}

void RotateOperation::get_area_rotation_bounds_inverted(const rcti &area,
                                                        const float center_x,
                                                        const float center_y,
                                                        const float sine,
                                                        const float cosine,
                                                        rcti &r_bounds)
{
  get_area_rotation_bounds(area, center_x, center_y, -sine, cosine, r_bounds);
}

void RotateOperation::get_rotation_area_of_interest(const rcti &input_canvas,
                                                    const rcti &rotate_canvas,
                                                    const float sine,
                                                    const float cosine,
                                                    const rcti &output_area,
                                                    rcti &r_input_area)
{
  float center_x, center_y;
  get_rotation_center(input_canvas, center_x, center_y);

  float rotate_offset_x, rotate_offset_y;
  get_rotation_offset(input_canvas, rotate_canvas, rotate_offset_x, rotate_offset_y);

  r_input_area = output_area;
  BLI_rcti_translate(&r_input_area, rotate_offset_x, rotate_offset_y);
  get_area_rotation_bounds_inverted(r_input_area, center_x, center_y, sine, cosine, r_input_area);
}

void RotateOperation::get_rotation_canvas(const rcti &input_canvas,
                                          const float sine,
                                          const float cosine,
                                          rcti &r_canvas)
{
  float center_x, center_y;
  get_rotation_center(input_canvas, center_x, center_y);

  rcti rot_bounds;
  get_area_rotation_bounds(input_canvas, center_x, center_y, sine, cosine, rot_bounds);

  float offset_x, offset_y;
  get_rotation_offset(input_canvas, rot_bounds, offset_x, offset_y);
  r_canvas = rot_bounds;
  BLI_rcti_translate(&r_canvas, -offset_x, -offset_y);
}

void RotateOperation::init_data() {}

inline void RotateOperation::ensure_degree()
{
  if (!is_degree_set_) {
    float degree = get_input_operation(DEGREE_INPUT_INDEX)->get_constant_value_default(0.0f);

    double rad;
    if (do_degree2_rad_conversion_) {
      rad = DEG2RAD(double(degree));
    }
    else {
      rad = degree;
    }
    cosine_ = cos(rad);
    sine_ = sin(rad);

    is_degree_set_ = true;
  }
}

void RotateOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  const bool image_determined =
      get_input_socket(IMAGE_INPUT_INDEX)->determine_canvas(preferred_area, r_area);
  if (image_determined) {
    rcti input_canvas = r_area;
    rcti unused = COM_AREA_NONE;
    get_input_socket(DEGREE_INPUT_INDEX)->determine_canvas(input_canvas, unused);

    ensure_degree();

    get_rotation_canvas(input_canvas, sine_, cosine_, r_area);
  }
}

void RotateOperation::get_area_of_interest(const int input_idx,
                                           const rcti &output_area,
                                           rcti &r_input_area)
{
  if (input_idx == DEGREE_INPUT_INDEX) {
    r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
    return;
  }

  ensure_degree();

  const rcti &input_image_canvas = get_input_operation(IMAGE_INPUT_INDEX)->get_canvas();
  get_rotation_area_of_interest(
      input_image_canvas, this->get_canvas(), sine_, cosine_, output_area, r_input_area);
  expand_area_for_sampler(r_input_area, sampler_);
}

void RotateOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                   const rcti &area,
                                                   Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[IMAGE_INPUT_INDEX];

  NodeOperation *image_op = get_input_operation(IMAGE_INPUT_INDEX);
  float center_x, center_y;
  get_rotation_center(image_op->get_canvas(), center_x, center_y);
  float rotate_offset_x, rotate_offset_y;
  get_rotation_offset(
      image_op->get_canvas(), this->get_canvas(), rotate_offset_x, rotate_offset_y);

  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float x = rotate_offset_x + it.x + canvas_.xmin;
    float y = rotate_offset_y + it.y + canvas_.ymin;
    rotate_coords(x, y, center_x, center_y, sine_, cosine_);
    input_img->read_elem_sampled(x - canvas_.xmin, y - canvas_.ymin, sampler_, it.out);
  }
}

}  // namespace blender::compositor
