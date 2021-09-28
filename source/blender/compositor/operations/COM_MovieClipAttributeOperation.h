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
#include "COM_NodeOperation.h"
#include "DNA_movieclip_types.h"

namespace blender::compositor {

typedef enum MovieClipAttribute {
  MCA_SCALE,
  MCA_X,
  MCA_Y,
  MCA_ANGLE,
} MovieClipAttribute;
/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class MovieClipAttributeOperation : public ConstantOperation {
 private:
  MovieClip *m_clip;
  float m_value;
  int m_framenumber;
  bool m_invert;
  MovieClipAttribute m_attribute;
  bool is_value_calculated_;
  NodeOperationInput *stabilization_resolution_socket_;

 public:
  /**
   * Default constructor
   */
  MovieClipAttributeOperation();

  void initExecution() override;

  /**
   * The inner loop of this operation.
   */
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  const float *get_constant_elem() override;

  void setMovieClip(MovieClip *clip)
  {
    this->m_clip = clip;
  }
  void setFramenumber(int framenumber)
  {
    this->m_framenumber = framenumber;
  }
  void setAttribute(MovieClipAttribute attribute)
  {
    this->m_attribute = attribute;
  }
  void setInvert(bool invert)
  {
    this->m_invert = invert;
  }

  /**
   * Set an operation socket which input will be used to get the resolution for stabilization.
   */
  void set_socket_input_resolution_for_stabilization(NodeOperationInput *input_socket)
  {
    stabilization_resolution_socket_ = input_socket;
  }

 private:
  void calc_value();
};

}  // namespace blender::compositor
