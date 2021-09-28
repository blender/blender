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
 * Copyright 2021, Blender Foundation.
 */

#include "COM_TransformOperation.h"
#include "COM_ConstantOperation.h"
#include "COM_RotateOperation.h"
#include "COM_ScaleOperation.h"

#include "BLI_math.h"

namespace blender::compositor {

TransformOperation::TransformOperation()
{
  addInputSocket(DataType::Color);
  addInputSocket(DataType::Value);
  addInputSocket(DataType::Value);
  addInputSocket(DataType::Value);
  addInputSocket(DataType::Value);
  addOutputSocket(DataType::Color);
  translate_factor_x_ = 1.0f;
  translate_factor_y_ = 1.0f;
  convert_degree_to_rad_ = false;
  sampler_ = PixelSampler::Bilinear;
  invert_ = false;
}

void TransformOperation::init_data()
{
  /* Translation. */
  translate_x_ = 0;
  NodeOperation *x_op = getInputOperation(X_INPUT_INDEX);
  if (x_op->get_flags().is_constant_operation) {
    translate_x_ = static_cast<ConstantOperation *>(x_op)->get_constant_elem()[0] *
                   translate_factor_x_;
  }
  translate_y_ = 0;
  NodeOperation *y_op = getInputOperation(Y_INPUT_INDEX);
  if (y_op->get_flags().is_constant_operation) {
    translate_y_ = static_cast<ConstantOperation *>(y_op)->get_constant_elem()[0] *
                   translate_factor_y_;
  }

  /* Scaling. */
  scale_center_x_ = getWidth() / 2.0;
  scale_center_y_ = getHeight() / 2.0;
  constant_scale_ = 1.0f;
  NodeOperation *scale_op = getInputOperation(SCALE_INPUT_INDEX);
  if (scale_op->get_flags().is_constant_operation) {
    constant_scale_ = static_cast<ConstantOperation *>(scale_op)->get_constant_elem()[0];
  }

  /* Rotation. */
  rotate_center_x_ = (getWidth() - 1.0) / 2.0;
  rotate_center_y_ = (getHeight() - 1.0) / 2.0;
  NodeOperation *degree_op = getInputOperation(DEGREE_INPUT_INDEX);
  const bool is_constant_degree = degree_op->get_flags().is_constant_operation;
  const float degree = is_constant_degree ?
                           static_cast<ConstantOperation *>(degree_op)->get_constant_elem()[0] :
                           0.0f;
  const double rad = convert_degree_to_rad_ ? DEG2RAD((double)degree) : degree;
  rotate_cosine_ = cos(rad);
  rotate_sine_ = sin(rad);
}

void TransformOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  switch (input_idx) {
    case IMAGE_INPUT_INDEX: {
      BLI_rcti_translate(&r_input_area, translate_x_, translate_y_);
      ScaleOperation::scale_area(
          r_input_area, scale_center_x_, scale_center_y_, constant_scale_, constant_scale_);
      RotateOperation::get_area_rotation_bounds(r_input_area,
                                                rotate_center_x_,
                                                rotate_center_y_,
                                                rotate_sine_,
                                                rotate_cosine_,
                                                r_input_area);
      expand_area_for_sampler(r_input_area, sampler_);
      break;
    }
    case X_INPUT_INDEX:
    case Y_INPUT_INDEX:
    case DEGREE_INPUT_INDEX: {
      r_input_area = COM_CONSTANT_INPUT_AREA_OF_INTEREST;
      break;
    }
    case SCALE_INPUT_INDEX: {
      r_input_area = output_area;
      break;
    }
  }
}

void TransformOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[IMAGE_INPUT_INDEX];
  MemoryBuffer *input_scale = inputs[SCALE_INPUT_INDEX];
  BuffersIterator<float> it = output->iterate_with({input_scale}, area);
  if (invert_) {
    transform_inverted(it, input_img);
  }
  else {
    transform(it, input_img);
  }
}

void TransformOperation::transform(BuffersIterator<float> &it, const MemoryBuffer *input_img)
{
  for (; !it.is_end(); ++it) {
    const float scale = *it.in(0);
    float x = it.x - translate_x_;
    float y = it.y - translate_y_;
    RotateOperation::rotate_coords(
        x, y, rotate_center_x_, rotate_center_y_, rotate_sine_, rotate_cosine_);
    x = ScaleOperation::scale_coord(x, scale_center_x_, scale);
    y = ScaleOperation::scale_coord(y, scale_center_y_, scale);
    input_img->read_elem_sampled(x, y, sampler_, it.out);
  }
}

void TransformOperation::transform_inverted(BuffersIterator<float> &it,
                                            const MemoryBuffer *input_img)
{
  for (; !it.is_end(); ++it) {
    const float scale = *it.in(0);
    float x = ScaleOperation::scale_coord(it.x, scale_center_x_, scale);
    float y = ScaleOperation::scale_coord(it.y, scale_center_y_, scale);
    RotateOperation::rotate_coords(
        x, y, rotate_center_x_, rotate_center_y_, rotate_sine_, rotate_cosine_);
    x -= translate_x_;
    y -= translate_y_;
    input_img->read_elem_sampled(x, y, sampler_, it.out);
  }
}

}  // namespace blender::compositor
