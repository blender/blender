/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class BaseScaleOperation : public MultiThreadedOperation {
 public:
  static constexpr float DEFAULT_MAX_SCALE_CANVAS_SIZE = 12000;

 public:
  void set_sampler(PixelSampler sampler)
  {
    sampler_ = (int)sampler;
  }
  void set_variable_size(bool variable_size)
  {
    variable_size_ = variable_size;
  };

  void set_scale_canvas_max_size(Size2f size);

 protected:
  BaseScaleOperation();

  PixelSampler get_effective_sampler(PixelSampler sampler)
  {
    return (sampler_ == -1) ? sampler : (PixelSampler)sampler_;
  }

  Size2f max_scale_canvas_size_ = {DEFAULT_MAX_SCALE_CANVAS_SIZE, DEFAULT_MAX_SCALE_CANVAS_SIZE};
  int sampler_;
  /* TODO(manzanilla): to be removed with tiled implementation. */
  bool variable_size_;
};

class ScaleOperation : public BaseScaleOperation {
 public:
  static constexpr float MIN_RELATIVE_SCALE = 0.0001f;

 protected:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int X_INPUT_INDEX = 1;
  static constexpr int Y_INPUT_INDEX = 2;

  SocketReader *input_operation_;
  SocketReader *input_xoperation_;
  SocketReader *input_yoperation_;
  float canvas_center_x_;
  float canvas_center_y_;

 public:
  ScaleOperation();
  ScaleOperation(DataType data_type);

  static float scale_coord(const float coord, const float center, const float relative_scale)
  {
    return center + (coord - center) * MAX2(relative_scale, MIN_RELATIVE_SCALE);
  }

  static float scale_coord_inverted(const float coord,
                                    const float center,
                                    const float relative_scale)
  {
    return center + (coord - center) / MAX2(relative_scale, MIN_RELATIVE_SCALE);
  }

  static void get_scale_offset(const rcti &input_canvas,
                               const rcti &scale_canvas,
                               float &r_scale_offset_x,
                               float &r_scale_offset_y);
  static void scale_area(rcti &area, float relative_scale_x, float relative_scale_y);
  static void get_scale_area_of_interest(const rcti &input_canvas,
                                         const rcti &scale_canvas,
                                         float relative_scale_x,
                                         float relative_scale_y,
                                         const rcti &output_area,
                                         rcti &r_input_area);
  static void clamp_area_size_max(rcti &area, Size2f max_size);

  void init_data() override;
  void init_execution() override;
  void deinit_execution() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

 protected:
  virtual float get_relative_scale_x_factor(float width) = 0;
  virtual float get_relative_scale_y_factor(float height) = 0;

 private:
  bool is_scaling_variable();
  float get_constant_scale(int input_op_idx, float factor);
  float get_constant_scale_x(float width);
  float get_constant_scale_y(float height);
};

class ScaleRelativeOperation : public ScaleOperation {
 public:
  ScaleRelativeOperation();
  ScaleRelativeOperation(DataType data_type);
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  float get_relative_scale_x_factor(float /*width*/) override
  {
    return 1.0f;
  }

  float get_relative_scale_y_factor(float /*height*/) override
  {
    return 1.0f;
  }
};

class ScaleAbsoluteOperation : public ScaleOperation {
 public:
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  float get_relative_scale_x_factor(float width) override
  {
    return 1.0f / width;
  }

  float get_relative_scale_y_factor(float height) override
  {
    return 1.0f / height;
  }
};

class ScaleFixedSizeOperation : public BaseScaleOperation {
  SocketReader *input_operation_;
  int new_width_;
  int new_height_;
  float rel_x_;
  float rel_y_;

  /* center is only used for aspect correction */
  float offset_x_;
  float offset_y_;
  bool is_aspect_;
  bool is_crop_;
  /* set from other properties on initialization,
   * check if we need to apply offset */
  bool is_offset_;

 public:
  /** Absolute fixed size. */
  ScaleFixedSizeOperation();
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void init_execution() override;
  void deinit_execution() override;
  void set_new_width(int width)
  {
    new_width_ = width;
  }
  void set_new_height(int height)
  {
    new_height_ = height;
  }
  void set_is_aspect(bool is_aspect)
  {
    is_aspect_ = is_aspect;
  }
  void set_is_crop(bool is_crop)
  {
    is_crop_ = is_crop;
  }
  void set_offset(float x, float y)
  {
    offset_x_ = x;
    offset_y_ = y;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  void init_data(const rcti &input_canvas);
};

}  // namespace blender::compositor
