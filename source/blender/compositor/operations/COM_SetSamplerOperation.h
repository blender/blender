/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output Sampler.
 * it assumes we are in sRGB color space.
 */
class SetSamplerOperation : public NodeOperation {
 private:
  PixelSampler sampler_;

 public:
  SetSamplerOperation();

  void set_sampler(PixelSampler sampler)
  {
    sampler_ = sampler;
  }
};

}  // namespace blender::compositor
