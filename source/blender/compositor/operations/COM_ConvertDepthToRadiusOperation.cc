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

#include "COM_ConvertDepthToRadiusOperation.h"
#include "BKE_camera.h"
#include "DNA_camera_types.h"

namespace blender::compositor {

ConvertDepthToRadiusOperation::ConvertDepthToRadiusOperation()
{
  this->addInputSocket(DataType::Value);
  this->addOutputSocket(DataType::Value);
  inputOperation_ = nullptr;
  fStop_ = 128.0f;
  cameraObject_ = nullptr;
  maxRadius_ = 32.0f;
  blurPostOperation_ = nullptr;
}

float ConvertDepthToRadiusOperation::determineFocalDistance()
{
  if (cameraObject_ && cameraObject_->type == OB_CAMERA) {
    Camera *camera = (Camera *)cameraObject_->data;
    cam_lens_ = camera->lens;
    return BKE_camera_object_dof_distance(cameraObject_);
  }

  return 10.0f;
}

void ConvertDepthToRadiusOperation::initExecution()
{
  float cam_sensor = DEFAULT_SENSOR_WIDTH;
  Camera *camera = nullptr;

  if (cameraObject_ && cameraObject_->type == OB_CAMERA) {
    camera = (Camera *)cameraObject_->data;
    cam_sensor = BKE_camera_sensor_size(camera->sensor_fit, camera->sensor_x, camera->sensor_y);
  }

  inputOperation_ = this->getInputSocketReader(0);
  float focalDistance = determineFocalDistance();
  if (focalDistance == 0.0f) {
    focalDistance = 1e10f; /* If the DOF is 0.0 then set it to be far away. */
  }
  inverseFocalDistance_ = 1.0f / focalDistance;
  aspect_ = (this->getWidth() > this->getHeight()) ?
                (this->getHeight() / (float)this->getWidth()) :
                (this->getWidth() / (float)this->getHeight());
  aperture_ = 0.5f * (cam_lens_ / (aspect_ * cam_sensor)) / fStop_;
  const float minsz = MIN2(getWidth(), getHeight());
  dof_sp_ = minsz / ((cam_sensor / 2.0f) /
                     cam_lens_); /* <- == `aspect * MIN2(img->x, img->y) / tan(0.5f * fov)` */

  if (blurPostOperation_) {
    blurPostOperation_->setSigma(MIN2(aperture_ * 128.0f, maxRadius_));
  }
}

void ConvertDepthToRadiusOperation::executePixelSampled(float output[4],
                                                        float x,
                                                        float y,
                                                        PixelSampler sampler)
{
  float inputValue[4];
  float z;
  float radius;
  inputOperation_->readSampled(inputValue, x, y, sampler);
  z = inputValue[0];
  if (z != 0.0f) {
    float iZ = (1.0f / z);

    /* bug T6656 part 2b, do not re-scale. */
#if 0
    bcrad = 0.5f * fabs(aperture * (dof_sp * (cam_invfdist - iZ) - 1.0f));
    /* Scale crad back to original maximum and blend. */
    crad->rect[px] = bcrad + wts->rect[px] * (scf * crad->rect[px] - bcrad);
#endif
    radius = 0.5f * fabsf(aperture_ * (dof_sp_ * (inverseFocalDistance_ - iZ) - 1.0f));
    /* 'bug' T6615, limit minimum radius to 1 pixel,
     * not really a solution, but somewhat mitigates the problem. */
    if (radius < 0.0f) {
      radius = 0.0f;
    }
    if (radius > maxRadius_) {
      radius = maxRadius_;
    }
    output[0] = radius;
  }
  else {
    output[0] = 0.0f;
  }
}

void ConvertDepthToRadiusOperation::deinitExecution()
{
  inputOperation_ = nullptr;
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

    /* Bug T6656 part 2b, do not re-scale. */
#if 0
    bcrad = 0.5f * fabs(aperture * (dof_sp * (cam_invfdist - iZ) - 1.0f));
    /* Scale crad back to original maximum and blend:
     * `crad->rect[px] = bcrad + wts->rect[px] * (scf * crad->rect[px] - bcrad);` */
#endif
    const float radius = 0.5f *
                         fabsf(aperture_ * (dof_sp_ * (inverseFocalDistance_ - inv_z) - 1.0f));
    /* Bug T6615, limit minimum radius to 1 pixel,
     * not really a solution, but somewhat mitigates the problem. */
    *it.out = CLAMPIS(radius, 0.0f, maxRadius_);
  }
}

}  // namespace blender::compositor
