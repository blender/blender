/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_GaussianXBlurOperation.h"
#include "COM_GaussianYBlurOperation.h"
#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class ConvertDepthToRadiusOperation : public MultiThreadedOperation {
 private:
  SocketReader *depth_input_operation_;
  SocketReader *image_input_operation_;

  const Scene *scene_;
  const NodeDefocus *data_;

  float f_stop;
  float max_radius;
  float focal_length;
  float pixels_per_meter;
  float distance_to_image_of_focus;

  GaussianXBlurOperation *blur_x_operation_;
  GaussianYBlurOperation *blur_y_operation_;

 public:
  ConvertDepthToRadiusOperation();

  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void init_execution() override;

  void deinit_execution() override;

  void set_data(const NodeDefocus *data)
  {
    data_ = data;
  }

  void set_scene(const Scene *scene)
  {
    scene_ = scene;
  }

  void set_blur_x_operation(GaussianXBlurOperation *blur_x_operation)
  {
    blur_x_operation_ = blur_x_operation;
  }

  void set_blur_y_operation(GaussianYBlurOperation *blur_y_operation)
  {
    blur_y_operation_ = blur_y_operation;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  float compute_maximum_defocus_radius() const;
  float compute_maximum_diameter_of_circle_of_confusion() const;
  float compute_distance_to_image_of_focus() const;
  float get_focal_length() const;
  float compute_focus_distance() const;
  float compute_pixels_per_meter() const;
  float get_f_stop() const;
  const Camera *get_camera() const;
  const Object *get_camera_object() const;
};

}  // namespace blender::compositor
