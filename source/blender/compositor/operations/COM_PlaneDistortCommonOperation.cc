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
 * Copyright 2013, Blender Foundation.
 */

#include "COM_PlaneDistortCommonOperation.h"

#include "MEM_guardedalloc.h"

#include "BLI_jitter_2d.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color.h"

#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_tracking.h"

/* ******** PlaneDistort WarpImage ******** */

BLI_INLINE void warpCoord(float x, float y, float matrix[3][3], float uv[2], float deriv[2][2])
{
  float vec[3] = {x, y, 1.0f};
  mul_m3_v3(matrix, vec);
  uv[0] = vec[0] / vec[2];
  uv[1] = vec[1] / vec[2];

  deriv[0][0] = (matrix[0][0] - matrix[0][2] * uv[0]) / vec[2];
  deriv[1][0] = (matrix[0][1] - matrix[0][2] * uv[1]) / vec[2];
  deriv[0][1] = (matrix[1][0] - matrix[1][2] * uv[0]) / vec[2];
  deriv[1][1] = (matrix[1][1] - matrix[1][2] * uv[1]) / vec[2];
}

PlaneDistortWarpImageOperation::PlaneDistortWarpImageOperation()
{
  this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
  this->addOutputSocket(COM_DT_COLOR);
  this->m_pixelReader = nullptr;
  this->m_motion_blur_samples = 1;
  this->m_motion_blur_shutter = 0.5f;
  this->setComplex(true);
}

void PlaneDistortWarpImageOperation::calculateCorners(const float corners[4][2],
                                                      bool normalized,
                                                      int sample)
{
  BLI_assert(sample < this->m_motion_blur_samples);
  const int width = this->m_pixelReader->getWidth();
  const int height = this->m_pixelReader->getHeight();
  float frame_corners[4][2] = {
      {0.0f, 0.0f}, {(float)width, 0.0f}, {(float)width, (float)height}, {0.0f, (float)height}};
  MotionSample *sample_data = &this->m_samples[sample];
  if (normalized) {
    for (int i = 0; i < 4; i++) {
      sample_data->frameSpaceCorners[i][0] = corners[i][0] * this->getWidth();
      sample_data->frameSpaceCorners[i][1] = corners[i][1] * this->getHeight();
    }
  }
  else {
    for (int i = 0; i < 4; i++) {
      sample_data->frameSpaceCorners[i][0] = corners[i][0];
      sample_data->frameSpaceCorners[i][1] = corners[i][1];
    }
  }
  BKE_tracking_homography_between_two_quads(
      sample_data->frameSpaceCorners, frame_corners, sample_data->perspectiveMatrix);
}

void PlaneDistortWarpImageOperation::initExecution()
{
  this->m_pixelReader = this->getInputSocketReader(0);
}

void PlaneDistortWarpImageOperation::deinitExecution()
{
  this->m_pixelReader = nullptr;
}

void PlaneDistortWarpImageOperation::executePixelSampled(float output[4],
                                                         float x,
                                                         float y,
                                                         PixelSampler /*sampler*/)
{
  float uv[2];
  float deriv[2][2];
  if (this->m_motion_blur_samples == 1) {
    warpCoord(x, y, this->m_samples[0].perspectiveMatrix, uv, deriv);
    m_pixelReader->readFiltered(output, uv[0], uv[1], deriv[0], deriv[1]);
  }
  else {
    zero_v4(output);
    for (int sample = 0; sample < this->m_motion_blur_samples; sample++) {
      float color[4];
      warpCoord(x, y, this->m_samples[sample].perspectiveMatrix, uv, deriv);
      m_pixelReader->readFiltered(color, uv[0], uv[1], deriv[0], deriv[1]);
      add_v4_v4(output, color);
    }
    mul_v4_fl(output, 1.0f / (float)this->m_motion_blur_samples);
  }
}

bool PlaneDistortWarpImageOperation::determineDependingAreaOfInterest(
    rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
  float min[2], max[2];
  INIT_MINMAX2(min, max);

  for (int sample = 0; sample < this->m_motion_blur_samples; sample++) {
    float UVs[4][2];
    float deriv[2][2];
    MotionSample *sample_data = &this->m_samples[sample];
    /* TODO(sergey): figure out proper way to do this. */
    warpCoord(input->xmin - 2, input->ymin - 2, sample_data->perspectiveMatrix, UVs[0], deriv);
    warpCoord(input->xmax + 2, input->ymin - 2, sample_data->perspectiveMatrix, UVs[1], deriv);
    warpCoord(input->xmax + 2, input->ymax + 2, sample_data->perspectiveMatrix, UVs[2], deriv);
    warpCoord(input->xmin - 2, input->ymax + 2, sample_data->perspectiveMatrix, UVs[3], deriv);
    for (int i = 0; i < 4; i++) {
      minmax_v2v2_v2(min, max, UVs[i]);
    }
  }

  rcti newInput;

  newInput.xmin = min[0] - 1;
  newInput.ymin = min[1] - 1;
  newInput.xmax = max[0] + 1;
  newInput.ymax = max[1] + 1;

  return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

/* ******** PlaneDistort Mask ******** */

PlaneDistortMaskOperation::PlaneDistortMaskOperation()
{
  addOutputSocket(COM_DT_VALUE);

  /* Currently hardcoded to 8 samples. */
  m_osa = 8;
  this->m_motion_blur_samples = 1;
  this->m_motion_blur_shutter = 0.5f;
}

void PlaneDistortMaskOperation::calculateCorners(const float corners[4][2],
                                                 bool normalized,
                                                 int sample)
{
  BLI_assert(sample < this->m_motion_blur_samples);
  MotionSample *sample_data = &this->m_samples[sample];
  if (normalized) {
    for (int i = 0; i < 4; i++) {
      sample_data->frameSpaceCorners[i][0] = corners[i][0] * this->getWidth();
      sample_data->frameSpaceCorners[i][1] = corners[i][1] * this->getHeight();
    }
  }
  else {
    for (int i = 0; i < 4; i++) {
      sample_data->frameSpaceCorners[i][0] = corners[i][0];
      sample_data->frameSpaceCorners[i][1] = corners[i][1];
    }
  }
}

void PlaneDistortMaskOperation::initExecution()
{
  BLI_jitter_init(m_jitter, m_osa);
}

void PlaneDistortMaskOperation::executePixelSampled(float output[4],
                                                    float x,
                                                    float y,
                                                    PixelSampler /*sampler*/)
{
  float point[2];
  int inside_counter = 0;
  if (this->m_motion_blur_samples == 1) {
    MotionSample *sample_data = &this->m_samples[0];
    for (int sample = 0; sample < this->m_osa; sample++) {
      point[0] = x + this->m_jitter[sample][0];
      point[1] = y + this->m_jitter[sample][1];
      if (isect_point_tri_v2(point,
                             sample_data->frameSpaceCorners[0],
                             sample_data->frameSpaceCorners[1],
                             sample_data->frameSpaceCorners[2]) ||
          isect_point_tri_v2(point,
                             sample_data->frameSpaceCorners[0],
                             sample_data->frameSpaceCorners[2],
                             sample_data->frameSpaceCorners[3])) {
        inside_counter++;
      }
    }
    output[0] = (float)inside_counter / this->m_osa;
  }
  else {
    for (int motion_sample = 0; motion_sample < this->m_motion_blur_samples; motion_sample++) {
      MotionSample *sample_data = &this->m_samples[motion_sample];
      for (int osa_sample = 0; osa_sample < this->m_osa; osa_sample++) {
        point[0] = x + this->m_jitter[osa_sample][0];
        point[1] = y + this->m_jitter[osa_sample][1];
        if (isect_point_tri_v2(point,
                               sample_data->frameSpaceCorners[0],
                               sample_data->frameSpaceCorners[1],
                               sample_data->frameSpaceCorners[2]) ||
            isect_point_tri_v2(point,
                               sample_data->frameSpaceCorners[0],
                               sample_data->frameSpaceCorners[2],
                               sample_data->frameSpaceCorners[3])) {
          inside_counter++;
        }
      }
    }
    output[0] = (float)inside_counter / (this->m_osa * this->m_motion_blur_samples);
  }
}
