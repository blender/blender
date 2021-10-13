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

#include "COM_FastGaussianBlurOperation.h"
#include "COM_MultiThreadedOperation.h"
#include "DNA_object_types.h"

namespace blender::compositor {

/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ConvertDepthToRadiusOperation : public MultiThreadedOperation {
 private:
  /**
   * Cached reference to the inputProgram
   */
  SocketReader *inputOperation_;
  float fStop_;
  float aspect_;
  float maxRadius_;
  float inverseFocalDistance_;
  float aperture_;
  float cam_lens_;
  float dof_sp_;
  Object *cameraObject_;

  FastGaussianBlurValueOperation *blurPostOperation_;

 public:
  /**
   * Default constructor
   */
  ConvertDepthToRadiusOperation();

  /**
   * The inner loop of this operation.
   */
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  /**
   * Initialize the execution
   */
  void initExecution() override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  void setfStop(float fStop)
  {
    fStop_ = fStop;
  }
  void setMaxRadius(float maxRadius)
  {
    maxRadius_ = maxRadius;
  }
  void setCameraObject(Object *camera)
  {
    cameraObject_ = camera;
  }
  float determineFocalDistance();
  void setPostBlur(FastGaussianBlurValueOperation *operation)
  {
    blurPostOperation_ = operation;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
