/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ConvertDepthToRadiusOperation.h"
#include "BKE_camera.h"
#include "DNA_camera_types.h"

namespace blender::compositor {

ConvertDepthToRadiusOperation::ConvertDepthToRadiusOperation()
{
  this->add_input_socket(DataType::Value);
  this->add_output_socket(DataType::Value);
  input_operation_ = nullptr;
  f_stop_ = 128.0f;
  camera_object_ = nullptr;
  max_radius_ = 32.0f;
  blur_post_operation_ = nullptr;
}

float ConvertDepthToRadiusOperation::determine_focal_distance()
{
  if (camera_object_ && camera_object_->type == OB_CAMERA) {
    Camera *camera = (Camera *)camera_object_->data;
    cam_lens_ = camera->lens;
    return BKE_camera_object_dof_distance(camera_object_);
  }

  return 10.0f;
}

void ConvertDepthToRadiusOperation::init_execution()
{
  float cam_sensor = DEFAULT_SENSOR_WIDTH;
  Camera *camera = nullptr;

  if (camera_object_ && camera_object_->type == OB_CAMERA) {
    camera = (Camera *)camera_object_->data;
    cam_sensor = BKE_camera_sensor_size(camera->sensor_fit, camera->sensor_x, camera->sensor_y);
  }

  input_operation_ = this->get_input_socket_reader(0);
  float focal_distance = determine_focal_distance();
  if (focal_distance == 0.0f) {
    focal_distance = 1e10f; /* If the DOF is 0.0 then set it to be far away. */
  }
  inverse_focal_distance_ = 1.0f / focal_distance;
  aspect_ = (this->get_width() > this->get_height()) ?
                (this->get_height() / float(this->get_width())) :
                (this->get_width() / float(this->get_height()));
  aperture_ = 0.5f * (cam_lens_ / (aspect_ * cam_sensor)) / f_stop_;
  const float minsz = MIN2(get_width(), get_height());
  /* Equal to: `aspect * MIN2(img->x, img->y) / tan(0.5f * fov)`. */
  dof_sp_ = minsz / ((cam_sensor / 2.0f) / cam_lens_);

  if (blur_post_operation_) {
    blur_post_operation_->set_sigma(MIN2(aperture_ * 128.0f, max_radius_));
  }
}

void ConvertDepthToRadiusOperation::execute_pixel_sampled(float output[4],
                                                          float x,
                                                          float y,
                                                          PixelSampler sampler)
{
  float input_value[4];
  float z;
  float radius;
  input_operation_->read_sampled(input_value, x, y, sampler);
  z = input_value[0];
  if (z != 0.0f) {
    float iZ = (1.0f / z);

    /* bug #6656 part 2b, do not re-scale. */
#if 0
    bcrad = 0.5f * fabs(aperture * (dof_sp * (cam_invfdist - iZ) - 1.0f));
    /* Scale crad back to original maximum and blend. */
    crad->rect[px] = bcrad + wts->rect[px] * (scf * crad->rect[px] - bcrad);
#endif
    radius = 0.5f * fabsf(aperture_ * (dof_sp_ * (inverse_focal_distance_ - iZ) - 1.0f));
    /* 'bug' #6615, limit minimum radius to 1 pixel,
     * not really a solution, but somewhat mitigates the problem. */
    if (radius < 0.0f) {
      radius = 0.0f;
    }
    if (radius > max_radius_) {
      radius = max_radius_;
    }
    output[0] = radius;
  }
  else {
    output[0] = 0.0f;
  }
}

void ConvertDepthToRadiusOperation::deinit_execution()
{
  input_operation_ = nullptr;
}

void ConvertDepthToRadiusOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                 const rcti &area,
                                                                 Span<MemoryBuffer *> inputs)
{
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    const float z = *it.in(0);
    if (z == 0.0f) {
      *it.out = 0.0f;
      continue;
    }

    const float inv_z = (1.0f / z);

    /* Bug #6656 part 2b, do not re-scale. */
#if 0
    bcrad = 0.5f * fabs(aperture * (dof_sp * (cam_invfdist - iZ) - 1.0f));
    /* Scale crad back to original maximum and blend:
     * `crad->rect[px] = bcrad + wts->rect[px] * (scf * crad->rect[px] - bcrad);` */
#endif
    const float radius = 0.5f *
                         fabsf(aperture_ * (dof_sp_ * (inverse_focal_distance_ - inv_z) - 1.0f));
    /* Bug #6615, limit minimum radius to 1 pixel,
     * not really a solution, but somewhat mitigates the problem. */
    *it.out = CLAMPIS(radius, 0.0f, max_radius_);
  }
}

}  // namespace blender::compositor
