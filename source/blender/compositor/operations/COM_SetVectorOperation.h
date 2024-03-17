/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void set_vector(const float vector[3])
  {
    setX(vector[0]);
    setY(vector[1]);
    setZ(vector[2]);
  }
};

}  // namespace blender::compositor
