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

#include "COM_ConstantOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class SetVectorOperation : public ConstantOperation {
 private:
  struct {
    float x;
    float y;
    float z;
    float w;
  } vector_;

 public:
  /**
   * Default constructor
   */
  SetVectorOperation();

  const float *get_constant_elem() override
  {
    return reinterpret_cast<float *>(&vector_);
  }

  float getX()
  {
    return vector_.x;
  }
  void setX(float value)
  {
    vector_.x = value;
  }
  float getY()
  {
    return vector_.y;
  }
  void setY(float value)
  {
    vector_.y = value;
  }
  float getZ()
  {
    return vector_.z;
  }
  void setZ(float value)
  {
    vector_.z = value;
  }
  float getW()
  {
    return vector_.w;
  }
  void setW(float value)
  {
    vector_.w = value;
  }

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void set_vector(const float vector[3])
  {
    setX(vector[0]);
    setY(vector[1]);
    setZ(vector[2]);
  }
};

}  // namespace blender::compositor
