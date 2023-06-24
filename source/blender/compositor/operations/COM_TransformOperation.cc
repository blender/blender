/* SPDX-FileCopyrightText: 2021 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_TransformOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"

namespace blender::compositor {

TransformOperation::TransformOperation()
{
  add_input_socket(DataType::Color, ResizeMode::None);
  add_input_socket(DataType::Value, ResizeMode::None);
  add_input_socket(DataType::Value, ResizeMode::None);
  add_input_socket(DataType::Value, ResizeMode::None);
  add_input_socket(DataType::Value, ResizeMode::None);
  add_output_socket(DataType::Color);
  translate_factor_x_ = 1.0f;
  translate_factor_y_ = 1.0f;
  convert_degree_to_rad_ = false;
  sampler_ = PixelSampler::Bilinear;
  invert_ = false;
  max_scale_canvas_size_ = {ScaleOperation::DEFAULT_MAX_SCALE_CANVAS_SIZE,
                            ScaleOperation::DEFAULT_MAX_SCALE_CANVAS_SIZE};
}

void TransformOperation::set_scale_canvas_max_size(Size2f size)
{
  max_scale_canvas_size_ = size;
}

void TransformOperation::init_data()
{

  translate_x_ = get_input_operation(X_INPUT_INDEX)->get_constant_value_default(0.0f) *
                 translate_factor_x_;
  translate_y_ = get_input_operation(Y_INPUT_INDEX)->get_constant_value_default(0.0f) *
                 translate_factor_y_;

  const float degree = get_input_operation(DEGREE_INPUT_INDEX)->get_constant_value_default(0.0f);
  const double rad = convert_degree_to_rad_ ? DEG2RAD(double(degree)) : degree;
  rotate_cosine_ = cos(rad);
  rotate_sine_ = sin(rad);

  scale_ = get_input_operation(SCALE_INPUT_INDEX)->get_constant_value_default(1.0f);
}

void TransformOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX: {
      NodeOperation *image_op = get_input_operation(IMAGE_INPUT_INDEX);
      const rcti &image_canvas = image_op->get_canvas();
      if (invert_) {
        /* Scale -> Rotate -> Translate. */
        r_input_area = output_area;
        BLI_rcti_translate(&r_input_area, -translate_x_, -translate_y_);
        RotateOperation::get_rotation_area_of_interest(scale_canvas_,
                                                       rotate_canvas_,
                                                       rotate_sine_,
                                                       rotate_cosine_,
                                                       r_input_area,
                                                       r_input_area);
        ScaleOperation::get_scale_area_of_interest(
            image_canvas, scale_canvas_, scale_, scale_, r_input_area, r_input_area);
      }
      else {
        /* Translate -> Rotate -> Scale. */
        ScaleOperation::get_scale_area_of_interest(
            rotate_canvas_, scale_canvas_, scale_, scale_, output_area, r_input_area);
        RotateOperation::get_rotation_area_of_interest(translate_canvas_,
                                                       rotate_canvas_,
                                                       rotate_sine_,
                                                       rotate_cosine_,
                                                       r_input_area,
                                                       r_input_area);
        BLI_rcti_translate(&r_input_area, -translate_x_, -translate_y_);
      }
      expand_area_for_sampler(r_input_area, sampler_);
      break;
    }
    case X_INPUT_INDEX:
    case Y_INPUT_INDEX:
    case DEGREE_INPUT_INDEX:
    case SCALE_INPUT_INDEX: {
      r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
      break;
    }
  }
}

void TransformOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[IMAGE_INPUT_INDEX];
  BuffersIterator<float> it = output->iterate_with({}, area);
  if (invert_) {
    transform_inverted(it, input_img);
  }
  else {
    transform(it, input_img);
  }
}

void TransformOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  const bool image_determined =
      get_input_socket(IMAGE_INPUT_INDEX)->determine_canvas(preferred_area, r_area);
  if (image_determined) {
    rcti image_canvas = r_area;
    rcti unused = COM_AREA_NONE;
    get_input_socket(X_INPUT_INDEX)->determine_canvas(image_canvas, unused);
    get_input_socket(Y_INPUT_INDEX)->determine_canvas(image_canvas, unused);
    get_input_socket(DEGREE_INPUT_INDEX)->determine_canvas(image_canvas, unused);
    get_input_socket(SCALE_INPUT_INDEX)->determine_canvas(image_canvas, unused);

    init_data();
    if (invert_) {
      /* Scale -> Rotate -> Translate. */
      scale_canvas_ = image_canvas;
      ScaleOperation::scale_area(scale_canvas_, scale_, scale_);
      const Size2f max_scale_size = {
          MAX2(BLI_rcti_size_x(&image_canvas), max_scale_canvas_size_.x),
          MAX2(BLI_rcti_size_y(&image_canvas), max_scale_canvas_size_.y)};
      ScaleOperation::clamp_area_size_max(scale_canvas_, max_scale_size);

      RotateOperation::get_rotation_canvas(
          scale_canvas_, rotate_sine_, rotate_cosine_, rotate_canvas_);

      translate_canvas_ = rotate_canvas_;
      BLI_rcti_translate(&translate_canvas_, translate_x_, translate_y_);

      r_area = translate_canvas_;
    }
    else {
      /* Translate -> Rotate -> Scale. */
      translate_canvas_ = image_canvas;
      BLI_rcti_translate(&translate_canvas_, translate_x_, translate_y_);

      RotateOperation::get_rotation_canvas(
          translate_canvas_, rotate_sine_, rotate_cosine_, rotate_canvas_);

      scale_canvas_ = rotate_canvas_;
      ScaleOperation::scale_area(scale_canvas_, scale_, scale_);

      const Size2f max_scale_size = {
          MAX2(BLI_rcti_size_x(&rotate_canvas_), max_scale_canvas_size_.x),
          MAX2(BLI_rcti_size_y(&rotate_canvas_), max_scale_canvas_size_.y)};
      ScaleOperation::clamp_area_size_max(scale_canvas_, max_scale_size);

      r_area = scale_canvas_;
    }
  }
}

void TransformOperation::transform(BuffersIterator<float> &it, const MemoryBuffer *input_img)
{
  float rotate_center_x, rotate_center_y;
  RotateOperation::get_rotation_center(translate_canvas_, rotate_center_x, rotate_center_y);
  float rotate_offset_x, rotate_offset_y;
  RotateOperation::get_rotation_offset(
      translate_canvas_, rotate_canvas_, rotate_offset_x, rotate_offset_y);

  const float scale_center_x = BLI_rcti_size_x(&rotate_canvas_) / 2.0f;
  const float scale_center_y = BLI_rcti_size_y(&rotate_canvas_) / 2.0f;
  float scale_offset_x, scale_offset_y;
  ScaleOperation::get_scale_offset(rotate_canvas_, scale_canvas_, scale_offset_x, scale_offset_y);

  for (; !it.is_end(); ++it) {
    float x = ScaleOperation::scale_coord_inverted(it.x + scale_offset_x, scale_center_x, scale_);
    float y = ScaleOperation::scale_coord_inverted(it.y + scale_offset_y, scale_center_y, scale_);

    x = rotate_offset_x + x;
    y = rotate_offset_y + y;
    RotateOperation::rotate_coords(
        x, y, rotate_center_x, rotate_center_y, rotate_sine_, rotate_cosine_);

    input_img->read_elem_sampled(x - translate_x_, y - translate_y_, sampler_, it.out);
  }
}

void TransformOperation::transform_inverted(BuffersIterator<float> &it,
                                            const MemoryBuffer *input_img)
{
  const rcti &image_canvas = get_input_operation(IMAGE_INPUT_INDEX)->get_canvas();
  const float scale_center_x = BLI_rcti_size_x(&image_canvas) / 2.0f - translate_x_;
  const float scale_center_y = BLI_rcti_size_y(&image_canvas) / 2.0f - translate_y_;
  float scale_offset_x, scale_offset_y;
  ScaleOperation::get_scale_offset(image_canvas, scale_canvas_, scale_offset_x, scale_offset_y);

  float rotate_center_x, rotate_center_y;
  RotateOperation::get_rotation_center(translate_canvas_, rotate_center_x, rotate_center_y);
  rotate_center_x -= translate_x_;
  rotate_center_y -= translate_y_;
  float rotate_offset_x, rotate_offset_y;
  RotateOperation::get_rotation_offset(
      scale_canvas_, rotate_canvas_, rotate_offset_x, rotate_offset_y);

  for (; !it.is_end(); ++it) {
    float x = rotate_offset_x + (it.x - translate_x_);
    float y = rotate_offset_y + (it.y - translate_y_);
    RotateOperation::rotate_coords(
        x, y, rotate_center_x, rotate_center_y, rotate_sine_, rotate_cosine_);

    x = ScaleOperation::scale_coord_inverted(x + scale_offset_x, scale_center_x, scale_);
    y = ScaleOperation::scale_coord_inverted(y + scale_offset_y, scale_center_y, scale_);

    input_img->read_elem_sampled(x, y, sampler_, it.out);
  }
}

}  // namespace blender::compositor
