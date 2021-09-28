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
  void setSampler(PixelSampler sampler)
  {
    this->m_sampler = (int)sampler;
  }
  void setVariableSize(bool variable_size)
  {
    m_variable_size = variable_size;
  };

 protected:
  BaseScaleOperation();

  PixelSampler getEffectiveSampler(PixelSampler sampler)
  {
    return (m_sampler == -1) ? sampler : (PixelSampler)m_sampler;
  }

  int m_sampler;
  bool m_variable_size;
};

class ScaleOperation : public BaseScaleOperation {
 public:
  static constexpr float MIN_SCALE = 0.0001f;

 protected:
  SocketReader *m_inputOperation;
  SocketReader *m_inputXOperation;
  SocketReader *m_inputYOperation;
  float m_centerX;
  float m_centerY;

 public:
  ScaleOperation();
  ScaleOperation(DataType data_type);

  static float scale_coord(const float coord, const float center, const float relative_scale)
  {
    return center + (coord - center) / MAX2(relative_scale, MIN_SCALE);
  }
  static void scale_area(rcti &rect, float center_x, float center_y, float scale_x, float scale_y);

  void init_data() override;
  void initExecution() override;
  void deinitExecution() override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 protected:
  virtual float get_relative_scale_x_factor() = 0;
  virtual float get_relative_scale_y_factor() = 0;

 private:
  float get_constant_scale(int input_op_idx, float factor);
  float get_constant_scale_x();
  float get_constant_scale_y();
  void scale_area(rcti &rect, float scale_x, float scale_y);
};

class ScaleRelativeOperation : public ScaleOperation {
 public:
  ScaleRelativeOperation();
  ScaleRelativeOperation(DataType data_type);
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
  float get_relative_scale_x_factor() override
  {
    return 1.0f;
  }
  float get_relative_scale_y_factor() override
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
  float get_relative_scale_x_factor() override
  {
    return 1.0f / getWidth();
  }
  float get_relative_scale_y_factor() override
  {
    return 1.0f / getHeight();
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

  void init_data() override;
  void initExecution() override;
  void deinitExecution() override;
  void setNewWidth(int width)
  {
    this->m_newWidth = width;
  }
  void setNewHeight(int height)
  {
    this->m_newHeight = height;
  }
  void setIsAspect(bool is_aspect)
  {
    this->m_is_aspect = is_aspect;
  }
  void setIsCrop(bool is_crop)
  {
    this->m_is_crop = is_crop;
  }
  void setOffset(float x, float y)
  {
    this->m_offsetX = x;
    this->m_offsetY = y;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
