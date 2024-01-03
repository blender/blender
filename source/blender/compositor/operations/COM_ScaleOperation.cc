/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ScaleOperation.h"
#include "COM_ConstantOperation.h"

namespace blender::compositor {

#define USE_FORCE_BILINEAR
/* XXX(@ideasman42): ignore input and use default from old compositor,
 * could become an option like the transform node.
 *
 * NOTE: use bilinear because bicubic makes fuzzy even when not scaling at all (1:1)
 */

BaseScaleOperation::BaseScaleOperation()
{
#ifdef USE_FORCE_BILINEAR
  sampler_ = int(PixelSampler::Bilinear);
#else
  sampler_ = -1;
#endif
  variable_size_ = false;
}

ScaleOperation::ScaleOperation() : ScaleOperation(DataType::Color) {}

ScaleOperation::ScaleOperation(DataType data_type) : BaseScaleOperation()
{
  this->add_input_socket(data_type, ResizeMode::None);
  this->add_input_socket(DataType::Value);
  this->add_input_socket(DataType::Value);
  this->add_output_socket(data_type);
  input_operation_ = nullptr;
  input_xoperation_ = nullptr;
  input_yoperation_ = nullptr;
  flags_.can_be_constant = true;
}

float ScaleOperation::get_constant_scale(const int input_op_idx, const float factor)
{
  const bool is_constant = get_input_operation(input_op_idx)->get_flags().is_constant_operation;
  if (is_constant) {
    return ((ConstantOperation *)get_input_operation(input_op_idx))->get_constant_elem()[0] *
           factor;
  }

  return 1.0f;
}

float ScaleOperation::get_constant_scale_x(const float width)
{
  return get_constant_scale(X_INPUT_INDEX, get_relative_scale_x_factor(width));
}

float ScaleOperation::get_constant_scale_y(const float height)
{
  return get_constant_scale(Y_INPUT_INDEX, get_relative_scale_y_factor(height));
}

bool ScaleOperation::is_scaling_variable()
{
  return !get_input_operation(X_INPUT_INDEX)->get_flags().is_constant_operation ||
         !get_input_operation(Y_INPUT_INDEX)->get_flags().is_constant_operation;
}

void ScaleOperation::scale_area(rcti &area, float relative_scale_x, float relative_scale_y)
{
  const rcti src_area = area;
  const float center_x = BLI_rcti_size_x(&area) / 2.0f;
  const float center_y = BLI_rcti_size_y(&area) / 2.0f;
  area.xmin = floorf(scale_coord(area.xmin, center_x, relative_scale_x));
  area.xmax = ceilf(scale_coord(area.xmax, center_x, relative_scale_x));
  area.ymin = floorf(scale_coord(area.ymin, center_y, relative_scale_y));
  area.ymax = ceilf(scale_coord(area.ymax, center_y, relative_scale_y));

  float scale_offset_x, scale_offset_y;
  ScaleOperation::get_scale_offset(src_area, area, scale_offset_x, scale_offset_y);
  BLI_rcti_translate(&area, -scale_offset_x, -scale_offset_y);
}

void ScaleOperation::clamp_area_size_max(rcti &area, Size2f max_size)
{

  if (BLI_rcti_size_x(&area) > max_size.x) {
    area.xmax = area.xmin + max_size.x;
  }
  if (BLI_rcti_size_y(&area) > max_size.y) {
    area.ymax = area.ymin + max_size.y;
  }
}

void ScaleOperation::init_data()
{
  canvas_center_x_ = canvas_.xmin + get_width() / 2.0f;
  canvas_center_y_ = canvas_.ymin + get_height() / 2.0f;
}

void ScaleOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
  input_xoperation_ = this->get_input_socket_reader(1);
  input_yoperation_ = this->get_input_socket_reader(2);
}

void ScaleOperation::deinit_execution()
{
  input_operation_ = nullptr;
  input_xoperation_ = nullptr;
  input_yoperation_ = nullptr;
}

void ScaleOperation::get_scale_offset(const rcti &input_canvas,
                                      const rcti &scale_canvas,
                                      float &r_scale_offset_x,
                                      float &r_scale_offset_y)
{
  r_scale_offset_x = (BLI_rcti_size_x(&input_canvas) - BLI_rcti_size_x(&scale_canvas)) / 2.0f;
  r_scale_offset_y = (BLI_rcti_size_y(&input_canvas) - BLI_rcti_size_y(&scale_canvas)) / 2.0f;
}

void ScaleOperation::get_scale_area_of_interest(const rcti &input_canvas,
                                                const rcti &scale_canvas,
                                                const float relative_scale_x,
                                                const float relative_scale_y,
                                                const rcti &output_area,
                                                rcti &r_input_area)
{
  const float scale_center_x = BLI_rcti_size_x(&input_canvas) / 2.0f;
  const float scale_center_y = BLI_rcti_size_y(&input_canvas) / 2.0f;
  float scale_offset_x, scale_offset_y;
  ScaleOperation::get_scale_offset(input_canvas, scale_canvas, scale_offset_x, scale_offset_y);

  r_input_area.xmin = floorf(
      scale_coord_inverted(output_area.xmin + scale_offset_x, scale_center_x, relative_scale_x));
  r_input_area.xmax = ceilf(
      scale_coord_inverted(output_area.xmax + scale_offset_x, scale_center_x, relative_scale_x));
  r_input_area.ymin = floorf(
      scale_coord_inverted(output_area.ymin + scale_offset_y, scale_center_y, relative_scale_y));
  r_input_area.ymax = ceilf(
      scale_coord_inverted(output_area.ymax + scale_offset_y, scale_center_y, relative_scale_y));
}

void ScaleOperation::get_area_of_interest(const int input_idx,
                                          const rcti &output_area,
                                          rcti &r_input_area)
{
  r_input_area = output_area;
  if (input_idx != 0 || is_scaling_variable()) {
    return;
  }

  NodeOperation *image_op = get_input_operation(IMAGE_INPUT_INDEX);
  const float scale_x = get_constant_scale_x(image_op->get_width());
  const float scale_y = get_constant_scale_y(image_op->get_height());

  get_scale_area_of_interest(
      image_op->get_canvas(), this->get_canvas(), scale_x, scale_y, output_area, r_input_area);
  expand_area_for_sampler(r_input_area, (PixelSampler)sampler_);
}

void ScaleOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                  const rcti &area,
                                                  Span<MemoryBuffer *> inputs)
{
  NodeOperation *input_image_op = get_input_operation(IMAGE_INPUT_INDEX);
  const int input_image_width = input_image_op->get_width();
  const int input_image_height = input_image_op->get_height();
  const float scale_x_factor = get_relative_scale_x_factor(input_image_width);
  const float scale_y_factor = get_relative_scale_y_factor(input_image_height);
  const float scale_center_x = input_image_width / 2.0f;
  const float scale_center_y = input_image_height / 2.0f;
  float from_scale_offset_x, from_scale_offset_y;
  ScaleOperation::get_scale_offset(
      input_image_op->get_canvas(), this->get_canvas(), from_scale_offset_x, from_scale_offset_y);

  const MemoryBuffer *input_image = inputs[IMAGE_INPUT_INDEX];
  MemoryBuffer *input_x = inputs[X_INPUT_INDEX];
  MemoryBuffer *input_y = inputs[Y_INPUT_INDEX];
  BuffersIterator<float> it = output->iterate_with({input_x, input_y}, area);
  for (; !it.is_end(); ++it) {
    const float rel_scale_x = *it.in(0) * scale_x_factor;
    const float rel_scale_y = *it.in(1) * scale_y_factor;
    const float scaled_x = scale_coord_inverted(
        from_scale_offset_x + canvas_.xmin + it.x, scale_center_x, rel_scale_x);
    const float scaled_y = scale_coord_inverted(
        from_scale_offset_y + canvas_.ymin + it.y, scale_center_y, rel_scale_y);

    input_image->read_elem_sampled(
        scaled_x - canvas_.xmin, scaled_y - canvas_.ymin, (PixelSampler)sampler_, it.out);
  }
}

void ScaleOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  if (execution_model_ == eExecutionModel::Tiled) {
    NodeOperation::determine_canvas(preferred_area, r_area);
    return;
  }

  const bool image_determined =
      get_input_socket(IMAGE_INPUT_INDEX)->determine_canvas(preferred_area, r_area);
  if (image_determined) {
    rcti image_canvas = r_area;
    rcti unused = COM_AREA_NONE;
    NodeOperationInput *x_socket = get_input_socket(X_INPUT_INDEX);
    NodeOperationInput *y_socket = get_input_socket(Y_INPUT_INDEX);
    x_socket->determine_canvas(image_canvas, unused);
    y_socket->determine_canvas(image_canvas, unused);
    if (is_scaling_variable()) {
      /* Do not scale canvas. */
      return;
    }

    /* Determine scaled canvas. */
    const float input_width = BLI_rcti_size_x(&r_area);
    const float input_height = BLI_rcti_size_y(&r_area);
    const float scale_x = get_constant_scale_x(input_width);
    const float scale_y = get_constant_scale_y(input_height);
    scale_area(r_area, scale_x, scale_y);

    /* Re-determine canvases of x and y constant inputs with scaled canvas as preferred. */
    get_input_operation(X_INPUT_INDEX)->unset_canvas();
    get_input_operation(Y_INPUT_INDEX)->unset_canvas();
    x_socket->determine_canvas(r_area, unused);
    y_socket->determine_canvas(r_area, unused);
  }
}

ScaleRelativeOperation::ScaleRelativeOperation() : ScaleOperation() {}

ScaleRelativeOperation::ScaleRelativeOperation(DataType data_type) : ScaleOperation(data_type) {}

void ScaleRelativeOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  PixelSampler effective_sampler = get_effective_sampler(sampler);

  float scaleX[4];
  float scaleY[4];

  input_xoperation_->read_sampled(scaleX, x, y, effective_sampler);
  input_yoperation_->read_sampled(scaleY, x, y, effective_sampler);

  const float scx = scaleX[0];
  const float scy = scaleY[0];

  float nx = this->canvas_center_x_ + (x - this->canvas_center_x_) / scx;
  float ny = this->canvas_center_y_ + (y - this->canvas_center_y_) / scy;
  input_operation_->read_sampled(output, nx, ny, effective_sampler);
}

bool ScaleRelativeOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  if (!variable_size_) {
    float scaleX[4];
    float scaleY[4];

    input_xoperation_->read_sampled(scaleX, 0, 0, PixelSampler::Nearest);
    input_yoperation_->read_sampled(scaleY, 0, 0, PixelSampler::Nearest);

    const float scx = scaleX[0];
    const float scy = scaleY[0];

    new_input.xmax = this->canvas_center_x_ + (input->xmax - this->canvas_center_x_) / scx + 1;
    new_input.xmin = this->canvas_center_x_ + (input->xmin - this->canvas_center_x_) / scx - 1;
    new_input.ymax = this->canvas_center_y_ + (input->ymax - this->canvas_center_y_) / scy + 1;
    new_input.ymin = this->canvas_center_y_ + (input->ymin - this->canvas_center_y_) / scy - 1;
  }
  else {
    new_input.xmax = this->get_width();
    new_input.xmin = 0;
    new_input.ymax = this->get_height();
    new_input.ymin = 0;
  }
  return BaseScaleOperation::determine_depending_area_of_interest(
      &new_input, read_operation, output);
}

void ScaleAbsoluteOperation::execute_pixel_sampled(float output[4],
                                                   float x,
                                                   float y,
                                                   PixelSampler sampler)
{
  PixelSampler effective_sampler = get_effective_sampler(sampler);

  float scaleX[4];
  float scaleY[4];

  input_xoperation_->read_sampled(scaleX, x, y, effective_sampler);
  input_yoperation_->read_sampled(scaleY, x, y, effective_sampler);

  const float scx = scaleX[0]; /* Target absolute scale. */
  const float scy = scaleY[0]; /* Target absolute scale. */

  const float width = this->get_width();
  const float height = this->get_height();
  /* Divide. */
  float relative_xscale = scx / width;
  float relative_yscale = scy / height;

  float nx = this->canvas_center_x_ + (x - this->canvas_center_x_) / relative_xscale;
  float ny = this->canvas_center_y_ + (y - this->canvas_center_y_) / relative_yscale;

  input_operation_->read_sampled(output, nx, ny, effective_sampler);
}

bool ScaleAbsoluteOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;
  if (!variable_size_) {
    float scaleX[4];
    float scaleY[4];

    input_xoperation_->read_sampled(scaleX, 0, 0, PixelSampler::Nearest);
    input_yoperation_->read_sampled(scaleY, 0, 0, PixelSampler::Nearest);

    const float scx = scaleX[0];
    const float scy = scaleY[0];
    const float width = this->get_width();
    const float height = this->get_height();
    /* Divide. */
    float relateve_xscale = scx / width;
    float relateve_yscale = scy / height;

    new_input.xmax = this->canvas_center_x_ +
                     (input->xmax - this->canvas_center_x_) / relateve_xscale;
    new_input.xmin = this->canvas_center_x_ +
                     (input->xmin - this->canvas_center_x_) / relateve_xscale;
    new_input.ymax = this->canvas_center_y_ +
                     (input->ymax - this->canvas_center_y_) / relateve_yscale;
    new_input.ymin = this->canvas_center_y_ +
                     (input->ymin - this->canvas_center_y_) / relateve_yscale;
  }
  else {
    new_input.xmax = this->get_width();
    new_input.xmin = 0;
    new_input.ymax = this->get_height();
    new_input.ymin = 0;
  }
  return ScaleOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
}

ScaleFixedSizeOperation::ScaleFixedSizeOperation() : BaseScaleOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::None);
  this->add_output_socket(DataType::Color);
  this->set_canvas_input_index(0);
  input_operation_ = nullptr;
  is_offset_ = false;
}

void ScaleFixedSizeOperation::init_data(const rcti &input_canvas)
{
  const int input_width = BLI_rcti_size_x(&input_canvas);
  const int input_height = BLI_rcti_size_y(&input_canvas);
  rel_x_ = input_width / float(new_width_);
  rel_y_ = input_height / float(new_height_);

  /* *** all the options below are for a fairly special case - camera framing *** */
  if (offset_x_ != 0.0f || offset_y_ != 0.0f) {
    is_offset_ = true;

    if (new_width_ > new_height_) {
      offset_x_ *= new_width_;
      offset_y_ *= new_width_;
    }
    else {
      offset_x_ *= new_height_;
      offset_y_ *= new_height_;
    }
  }

  if (is_aspect_) {
    /* apply aspect from clip */
    const float w_src = input_width;
    const float h_src = input_height;

    /* destination aspect is already applied from the camera frame */
    const float w_dst = new_width_;
    const float h_dst = new_height_;

    const float asp_src = w_src / h_src;
    const float asp_dst = w_dst / h_dst;

    if (fabsf(asp_src - asp_dst) >= FLT_EPSILON) {
      if ((asp_src > asp_dst) == (is_crop_ == true)) {
        /* fit X */
        const float div = asp_src / asp_dst;
        rel_x_ /= div;
        offset_x_ += ((w_src - (w_src * div)) / (w_src / w_dst)) / 2.0f;
        if (is_crop_ && execution_model_ == eExecutionModel::FullFrame) {
          int fit_width = new_width_ * div;

          const int added_width = fit_width - new_width_;
          new_width_ += added_width;
          offset_x_ += added_width / 2.0f;
        }
      }
      else {
        /* fit Y */
        const float div = asp_dst / asp_src;
        rel_y_ /= div;
        offset_y_ += ((h_src - (h_src * div)) / (h_src / h_dst)) / 2.0f;
        if (is_crop_ && execution_model_ == eExecutionModel::FullFrame) {
          int fit_height = new_height_ * div;

          const int added_height = fit_height - new_height_;
          new_height_ += added_height;
          offset_y_ += added_height / 2.0f;
        }
      }

      is_offset_ = true;
    }
  }
  /* *** end framing options *** */
}

void ScaleFixedSizeOperation::init_execution()
{
  input_operation_ = this->get_input_socket_reader(0);
}

void ScaleFixedSizeOperation::deinit_execution()
{
  input_operation_ = nullptr;
}

void ScaleFixedSizeOperation::execute_pixel_sampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler sampler)
{
  PixelSampler effective_sampler = get_effective_sampler(sampler);

  if (is_offset_) {
    float nx = ((x - offset_x_) * rel_x_);
    float ny = ((y - offset_y_) * rel_y_);
    input_operation_->read_sampled(output, nx, ny, effective_sampler);
  }
  else {
    input_operation_->read_sampled(output, x * rel_x_, y * rel_y_, effective_sampler);
  }
}

bool ScaleFixedSizeOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  rcti new_input;

  new_input.xmax = (input->xmax - offset_x_) * rel_x_ + 1;
  new_input.xmin = (input->xmin - offset_x_) * rel_x_;
  new_input.ymax = (input->ymax - offset_y_) * rel_y_ + 1;
  new_input.ymin = (input->ymin - offset_y_) * rel_y_;

  return BaseScaleOperation::determine_depending_area_of_interest(
      &new_input, read_operation, output);
}

void ScaleFixedSizeOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  rcti local_preferred = preferred_area;
  local_preferred.xmax = local_preferred.xmin + new_width_;
  local_preferred.ymax = local_preferred.ymin + new_height_;
  rcti input_canvas = COM_AREA_NONE;
  const bool input_determined = get_input_socket(0)->determine_canvas(local_preferred,
                                                                      input_canvas);
  if (input_determined) {
    init_data(input_canvas);
    r_area = input_canvas;
    if (execution_model_ == eExecutionModel::FullFrame) {
      r_area.xmin /= rel_x_;
      r_area.ymin /= rel_y_;
      r_area.xmin += offset_x_;
      r_area.ymin += offset_y_;
    }

    r_area.xmax = r_area.xmin + new_width_;
    r_area.ymax = r_area.ymin + new_height_;
  }
}

void ScaleFixedSizeOperation::get_area_of_interest(const int input_idx,
                                                   const rcti &output_area,
                                                   rcti &r_input_area)
{
  BLI_assert(input_idx == 0);
  UNUSED_VARS_NDEBUG(input_idx);

  r_input_area.xmax = ceilf((output_area.xmax - offset_x_) * rel_x_);
  r_input_area.xmin = floorf((output_area.xmin - offset_x_) * rel_x_);
  r_input_area.ymax = ceilf((output_area.ymax - offset_y_) * rel_y_);
  r_input_area.ymin = floorf((output_area.ymin - offset_y_) * rel_y_);
  expand_area_for_sampler(r_input_area, (PixelSampler)sampler_);
}

void ScaleFixedSizeOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                           const rcti &area,
                                                           Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[0];
  PixelSampler sampler = (PixelSampler)sampler_;
  BuffersIterator<float> it = output->iterate_with({}, area);
  if (is_offset_) {
    for (; !it.is_end(); ++it) {
      const float nx = (canvas_.xmin + it.x - offset_x_) * rel_x_;
      const float ny = (canvas_.ymin + it.y - offset_y_) * rel_y_;
      input_img->read_elem_sampled(nx - canvas_.xmin, ny - canvas_.ymin, sampler, it.out);
    }
  }
  else {
    for (; !it.is_end(); ++it) {
      input_img->read_elem_sampled((canvas_.xmin + it.x) * rel_x_ - canvas_.xmin,
                                   (canvas_.ymin + it.y) * rel_y_ - canvas_.ymin,
                                   sampler,
                                   it.out);
    }
  }
}

}  // namespace blender::compositor
