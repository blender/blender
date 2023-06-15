/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  MovieClip *clip_;
  float value_;
  int framenumber_;
  bool invert_;
  MovieClipAttribute attribute_;
  bool is_value_calculated_;
  NodeOperationInput *stabilization_resolution_socket_;

 public:
  /**
   * Default constructor
   */
  MovieClipAttributeOperation();

  void init_execution() override;

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  const float *get_constant_elem() override;

  void set_movie_clip(MovieClip *clip)
  {
    clip_ = clip;
  }
  void set_framenumber(int framenumber)
  {
    framenumber_ = framenumber;
  }
  void set_attribute(MovieClipAttribute attribute)
  {
    attribute_ = attribute;
  }
  void set_invert(bool invert)
  {
    invert_ = invert;
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
