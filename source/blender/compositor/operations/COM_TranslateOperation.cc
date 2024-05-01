/* SPDX-FileCopyrightText: 2011 Blender Authors
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
  is_delta_set_ = false;
  is_relative_ = false;
  this->x_extend_mode_ = MemoryBufferExtend::Clip;
  this->y_extend_mode_ = MemoryBufferExtend::Clip;
  this->sampler_ = PixelSampler::Nearest;

  this->flags_.can_be_constant = true;
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
      const int delta_x = this->get_delta_x();
      BLI_rcti_translate(&r_input_area, -delta_x, 0);
    }
    else if (x_extend_mode_ == MemoryBufferExtend::Repeat) {
      /* The region of interest should consider the whole input image to avoid cropping effects,
       * e.g. by prior scaling or rotating. Note: this is still consistent with immediate
       * realization of transform nodes in GPU compositor, where nodes are to be evaluated from
       * left to right. */
      const int in_width = get_width();
      BLI_rcti_resize_x(&r_input_area, in_width);
    }

    if (y_extend_mode_ == MemoryBufferExtend::Clip) {
      const int delta_y = this->get_delta_y();
      BLI_rcti_translate(&r_input_area, 0, -delta_y);
    }
    else if (y_extend_mode_ == MemoryBufferExtend::Repeat) {
      const int in_height = get_height();
      BLI_rcti_resize_y(&r_input_area, in_height);
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
  if (input->is_a_single_elem()) {
    copy_v4_v4(output->get_elem(0, 0), input->get_elem(0, 0));
    return;
  }

  /* Some compositor operations produce an empty output buffer by specifying a COM_AREA_NONE canvas
   * to indicate an invalid output, for instance, when the Mask operation reference an invalid
   * mask. The intention is that this buffer would signal that a fallback value would fill the
   * canvas of consumer operations. Since the aforementioned filling is achieved through the
   * Translate operation as part of canvas conversion in COM_convert_canvas, we handle the empty
   * buffer case here and fill the output using a fallback black color. */
  if (BLI_rcti_is_empty(&input->get_rect())) {
    const float value[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    output->fill(area, value);
    return;
  }

  float delta_x = this->get_delta_x();
  float delta_y = this->get_delta_y();
  if (sampler_ == PixelSampler::Nearest) {
    /* Use same rounding convention for GPU compositor. */
    delta_x = round(delta_x);
    delta_y = round(delta_y);
  }

  for (int y = area.ymin; y < area.ymax; y++) {
    float *out = output->get_elem(area.xmin, y);
    for (int x = area.xmin; x < area.xmax; x++) {
      const float input_x = x - delta_x;
      const float input_y = y - delta_y;
      input->read(out, input_x, input_y, sampler_, x_extend_mode_, y_extend_mode_);
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
    const float delta_x = x_extend_mode_ == MemoryBufferExtend::Clip ? get_delta_x() : 0.0f;
    const float delta_y = y_extend_mode_ == MemoryBufferExtend::Clip ? get_delta_y() : 0.0f;
    BLI_rcti_translate(&r_area, delta_x, delta_y);
  }
}

}  // namespace blender::compositor
