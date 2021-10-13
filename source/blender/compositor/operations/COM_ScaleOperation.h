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
 * Copyright 2011, Blender Foundation.
 */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class BaseScaleOperation : public MultiThreadedOperation {
 public:
  static constexpr float DEFAULT_MAX_SCALE_CANVAS_SIZE = 12000;

 public:
  void setSampler(PixelSampler sampler)
  {
    m_sampler = (int)sampler;
  }
  void setVariableSize(bool variable_size)
  {
    m_variable_size = variable_size;
  };

  void set_scale_canvas_max_size(Size2f size);

 protected:
  BaseScaleOperation();

  PixelSampler getEffectiveSampler(PixelSampler sampler)
  {
    return (m_sampler == -1) ? sampler : (PixelSampler)m_sampler;
  }

  Size2f max_scale_canvas_size_ = {DEFAULT_MAX_SCALE_CANVAS_SIZE, DEFAULT_MAX_SCALE_CANVAS_SIZE};
  int m_sampler;
  /* TODO(manzanilla): to be removed with tiled implementation. */
  bool m_variable_size;
};

class ScaleOperation : public BaseScaleOperation {
 public:
  static constexpr float MIN_RELATIVE_SCALE = 0.0001f;

 protected:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int X_INPUT_INDEX = 1;
  static constexpr int Y_INPUT_INDEX = 2;

  SocketReader *m_inputOperation;
  SocketReader *m_inputXOperation;
  SocketReader *m_inputYOperation;
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
                                         const float relative_scale_x,
                                         const float relative_scale_y,
                                         const rcti &output_area,
                                         rcti &r_input_area);
  static void clamp_area_size_max(rcti &area, Size2f max_size);

  void init_data() override;
  void initExecution() override;
  void deinitExecution() override;

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
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  float get_relative_scale_x_factor(float UNUSED(width)) override
  {
    return 1.0f;
  }

  float get_relative_scale_y_factor(float UNUSED(height)) override
  {
    return 1.0f;
  }
};

class ScaleAbsoluteOperation : public ScaleOperation {
 public:
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

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
  SocketReader *m_inputOperation;
  int m_newWidth;
  int m_newHeight;
  float m_relX;
  float m_relY;

  /* center is only used for aspect correction */
  float m_offsetX;
  float m_offsetY;
  bool m_is_aspect;
  bool m_is_crop;
  /* set from other properties on initialization,
   * check if we need to apply offset */
  bool m_is_offset;

 public:
  ScaleFixedSizeOperation();
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  void initExecution() override;
  void deinitExecution() override;
  void setNewWidth(int width)
  {
    m_newWidth = width;
  }
  void setNewHeight(int height)
  {
    m_newHeight = height;
  }
  void setIsAspect(bool is_aspect)
  {
    m_is_aspect = is_aspect;
  }
  void setIsCrop(bool is_crop)
  {
    m_is_crop = is_crop;
  }
  void setOffset(float x, float y)
  {
    m_offsetX = x;
    m_offsetY = y;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  void init_data(const rcti &input_canvas);
};

}  // namespace blender::compositor
