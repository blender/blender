/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
   * Cached reference to the input_program
   */
  SocketReader *input_operation_;
  float f_stop_;
  float aspect_;
  float max_radius_;
  float inverse_focal_distance_;
  float aperture_;
  float cam_lens_;
  float dof_sp_;
  Object *camera_object_;

  FastGaussianBlurValueOperation *blur_post_operation_;

 public:
  /**
   * Default constructor
   */
  ConvertDepthToRadiusOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void setf_stop(float f_stop)
  {
    f_stop_ = f_stop;
  }
  void set_max_radius(float max_radius)
  {
    max_radius_ = max_radius;
  }
  void set_camera_object(Object *camera)
  {
    camera_object_ = camera;
  }
  float determine_focal_distance();
  void set_post_blur(FastGaussianBlurValueOperation *operation)
  {
    blur_post_operation_ = operation;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
