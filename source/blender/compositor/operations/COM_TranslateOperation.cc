/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_TranslateOperation.h"

namespace blender::compositor {

TranslateOperation::TranslateOperation() : TranslateOperation(DataType::Color) {}
TranslateOperation::TranslateOperation(DataType data_type, ResizeMode resize_mode)
{
  this->add_input_socket(data_type, resize_mode);
  this->add_input_socket(DataType::Value, ResizeMode::None);
  this->add_input_socket(DataType::Value, ResizeMode::None);
  this->add_output_socket(data_type);
  this->set_canvas_input_index(0);
  input_operation_ = nullptr;
  input_xoperation_ = nullptr;
  input_yoperation_ = nullptr;
  is_delta_set_ = false;
  factor_x_ = 1.0f;
  factor_y_ = 1.0f;
  this->x_extend_mode_ = MemoryBufferExtend::Clip;
  this->y_extend_mode_ = MemoryBufferExtend::Clip;
}

void TranslateOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
  input_xoperation_ = this->get_input_socket_reader(1);
  input_yoperation_ = this->get_input_socket_reader(2);
}

void TranslateOperation::deinit_execution()
{
  input_operation_ = nullptr;
  input_xoperation_ = nullptr;
  input_yoperation_ = nullptr;
}

void TranslateOperation::execute_pixel_sampled(float output[4],
                                               float x,
                                               float y,
                                               PixelSampler /*sampler*/)
{
  ensure_delta();

  float original_xpos = x - this->getDeltaX();
  float original_ypos = y - this->getDeltaY();

  input_operation_->read_sampled(output, original_xpos, original_ypos, PixelSampler::Bilinear);
}

bool TranslateOperation::determine_depending_area_of_interest(rcti *input,
                                                              ReadBufferOperation *read_operation,
                                                              rcti *output)
{
  rcti new_input;

  ensure_delta();

  new_input.xmin = input->xmin - this->getDeltaX();
  new_input.xmax = input->xmax - this->getDeltaX();
  new_input.ymin = input->ymin - this->getDeltaY();
  new_input.ymax = input->ymax - this->getDeltaY();

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

void TranslateOperation::setFactorXY(float factorX, float factorY)
{
  factor_x_ = factorX;
  factor_y_ = factorY;
}

void TranslateOperation::set_wrapping(int wrapping_type)
{
  switch (wrapping_type) {
    case CMP_NODE_WRAP_X:
      x_extend_mode_ = MemoryBufferExtend::Repeat;
      break;
    case CMP_NODE_WRAP_Y:
      y_extend_mode_ = MemoryBufferExtend::Repeat;
      break;
    case CMP_NODE_WRAP_XY:
      x_extend_mode_ = MemoryBufferExtend::Repeat;
      y_extend_mode_ = MemoryBufferExtend::Repeat;
      break;
    default:
      break;
  }
}

void TranslateOperation::get_area_of_interest(const int input_idx,
                                              const rcti &output_area,
                                              rcti &r_input_area)
{
  if (input_idx == 0) {
    ensure_delta();
    r_input_area = output_area;
    if (x_extend_mode_ == MemoryBufferExtend::Clip) {
      const int delta_x = this->getDeltaX();
      BLI_rcti_translate(&r_input_area, -delta_x, 0);
    }
    if (y_extend_mode_ == MemoryBufferExtend::Clip) {
      const int delta_y = this->getDeltaY();
      BLI_rcti_translate(&r_input_area, 0, -delta_y);
    }
  }
  else {
    r_input_area = output_area;
  }
}

void TranslateOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                      const rcti &area,
                                                      Span<MemoryBuffer *> inputs)
{
  MemoryBuffer *input = inputs[0];
  const int delta_x = this->getDeltaX();
  const int delta_y = this->getDeltaY();
  for (int y = area.ymin; y < area.ymax; y++) {
    float *out = output->get_elem(area.xmin, y);
    for (int x = area.xmin; x < area.xmax; x++) {
      const int input_x = x - delta_x;
      const int input_y = y - delta_y;
      input->read(out, input_x, input_y, x_extend_mode_, y_extend_mode_);
      out += output->elem_stride;
    }
  }
}

TranslateCanvasOperation::TranslateCanvasOperation()
    : TranslateOperation(DataType::Color, ResizeMode::None)
{
}

void TranslateCanvasOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  const bool determined =
      get_input_socket(IMAGE_INPUT_INDEX)->determine_canvas(preferred_area, r_area);
  if (determined) {
    NodeOperationInput *x_socket = get_input_socket(X_INPUT_INDEX);
    NodeOperationInput *y_socket = get_input_socket(Y_INPUT_INDEX);
    rcti unused = COM_AREA_NONE;
    x_socket->determine_canvas(r_area, unused);
    y_socket->determine_canvas(r_area, unused);

    ensure_delta();
    const float delta_x = x_extend_mode_ == MemoryBufferExtend::Clip ? getDeltaX() : 0.0f;
    const float delta_y = y_extend_mode_ == MemoryBufferExtend::Clip ? getDeltaY() : 0.0f;
    BLI_rcti_translate(&r_area, delta_x, delta_y);
  }
}

}  // namespace blender::compositor
