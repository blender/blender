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
#include "BLI_math.h"
#include "DNA_camera_types.h"

ConvertDepthToRadiusOperation::ConvertDepthToRadiusOperation()
{
  this->addInputSocket(COM_DT_VALUE);
  this->addOutputSocket(COM_DT_VALUE);
  this->m_inputOperation = nullptr;
  this->m_fStop = 128.0f;
  this->m_cameraObject = nullptr;
  this->m_maxRadius = 32.0f;
  this->m_blurPostOperation = nullptr;
}

float ConvertDepthToRadiusOperation::determineFocalDistance()
{
  if (this->m_cameraObject && this->m_cameraObject->type == OB_CAMERA) {
    Camera *camera = (Camera *)this->m_cameraObject->data;
    this->m_cam_lens = camera->lens;
    return BKE_camera_object_dof_distance(this->m_cameraObject);
  }

  return 10.0f;
}

void ConvertDepthToRadiusOperation::initExecution()
{
  float cam_sensor = DEFAULT_SENSOR_WIDTH;
  Camera *camera = nullptr;

  if (this->m_cameraObject && this->m_cameraObject->type == OB_CAMERA) {
    camera = (Camera *)this->m_cameraObject->data;
    cam_sensor = BKE_camera_sensor_size(camera->sensor_fit, camera->sensor_x, camera->sensor_y);
  }

  this->m_inputOperation = this->getInputSocketReader(0);
  float focalDistance = determineFocalDistance();
  if (focalDistance == 0.0f) {
    focalDistance = 1e10f; /* if the dof is 0.0 then set it to be far away */
  }
  this->m_inverseFocalDistance = 1.0f / focalDistance;
  this->m_aspect = (this->getWidth() > this->getHeight()) ?
                       (this->getHeight() / (float)this->getWidth()) :
                       (this->getWidth() / (float)this->getHeight());
  this->m_aperture = 0.5f * (this->m_cam_lens / (this->m_aspect * cam_sensor)) / this->m_fStop;
  const float minsz = MIN2(getWidth(), getHeight());
  this->m_dof_sp = minsz /
                   ((cam_sensor / 2.0f) /
                    this->m_cam_lens);  // <- == aspect * MIN2(img->x, img->y) / tan(0.5f * fov);

  if (this->m_blurPostOperation) {
    m_blurPostOperation->setSigma(MIN2(m_aperture * 128.0f, this->m_maxRadius));
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
  this->m_inputOperation->readSampled(inputValue, x, y, sampler);
  z = inputValue[0];
  if (z != 0.0f) {
    float iZ = (1.0f / z);

    /* bug T6656 part 2b, do not re-scale. */
#if 0
    bcrad = 0.5f * fabs(aperture * (dof_sp * (cam_invfdist - iZ) - 1.0f));
    // scale crad back to original maximum and blend
    crad->rect[px] = bcrad + wts->rect[px] * (scf * crad->rect[px] - bcrad);
#endif
    radius = 0.5f * fabsf(this->m_aperture *
                          (this->m_dof_sp * (this->m_inverseFocalDistance - iZ) - 1.0f));
    /* 'bug' T6615, limit minimum radius to 1 pixel,
     * not really a solution, but somewhat mitigates the problem. */
    if (radius < 0.0f) {
      radius = 0.0f;
    }
    if (radius > this->m_maxRadius) {
      radius = this->m_maxRadius;
    }
    output[0] = radius;
  }
  else {
    output[0] = 0.0f;
  }
}

void ConvertDepthToRadiusOperation::deinitExecution()
{
  this->m_inputOperation = nullptr;
}
