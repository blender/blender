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

#include "BLI_jitter_2d.h"

#include "BKE_tracking.h"

namespace blender::compositor {

PlaneDistortBaseOperation::PlaneDistortBaseOperation()
    : m_motion_blur_samples(1), m_motion_blur_shutter(0.5f)
{
}

void PlaneDistortBaseOperation::calculateCorners(const float corners[4][2],
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

PlaneDistortWarpImageOperation::PlaneDistortWarpImageOperation() : PlaneDistortBaseOperation()
{
  this->addInputSocket(DataType::Color, ResizeMode::Align);
  this->addOutputSocket(DataType::Color);
  this->m_pixelReader = nullptr;
  this->flags.complex = true;
}

void PlaneDistortWarpImageOperation::calculateCorners(const float corners[4][2],
                                                      bool normalized,
                                                      int sample)
{
  PlaneDistortBaseOperation::calculateCorners(corners, normalized, sample);

  const NodeOperation *image = get_input_operation(0);
  const int width = image->getWidth();
  const int height = image->getHeight();
  float frame_corners[4][2] = {
      {0.0f, 0.0f}, {(float)width, 0.0f}, {(float)width, (float)height}, {0.0f, (float)height}};
  MotionSample *sample_data = &this->m_samples[sample];
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

void PlaneDistortWarpImageOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                                  const rcti &area,
                                                                  Span<MemoryBuffer *> inputs)
{
  const MemoryBuffer *input_img = inputs[0];
  float uv[2];
  float deriv[2][2];
  BuffersIterator<float> it = output->iterate_with({}, area);
  if (this->m_motion_blur_samples == 1) {
    for (; !it.is_end(); ++it) {
      warpCoord(it.x, it.y, this->m_samples[0].perspectiveMatrix, uv, deriv);
      input_img->read_elem_filtered(uv[0], uv[1], deriv[0], deriv[1], it.out);
    }
  }
  else {
    for (; !it.is_end(); ++it) {
      zero_v4(it.out);
      for (const int sample : IndexRange(this->m_motion_blur_samples)) {
        float color[4];
        warpCoord(it.x, it.y, this->m_samples[sample].perspectiveMatrix, uv, deriv);
        input_img->read_elem_filtered(uv[0], uv[1], deriv[0], deriv[1], color);
        add_v4_v4(it.out, color);
      }
      mul_v4_fl(it.out, 1.0f / (float)this->m_motion_blur_samples);
    }
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

void PlaneDistortWarpImageOperation::get_area_of_interest(const int input_idx,
                                                          const rcti &output_area,
                                                          rcti &r_input_area)
{
  if (input_idx != 0) {
    r_input_area = output_area;
    return;
  }

  /* TODO: figure out the area needed for warping and EWA filtering. */
  r_input_area = get_input_operation(0)->get_canvas();

/* Old implementation but resulting coordinates are way out of input operation bounds and in some
 * cases the area result may incorrectly cause cropping. */
#if 0
  float min[2], max[2];
  INIT_MINMAX2(min, max);
  for (int sample = 0; sample < this->m_motion_blur_samples; sample++) {
    float UVs[4][2];
    float deriv[2][2];
    MotionSample *sample_data = &this->m_samples[sample];
    /* TODO(sergey): figure out proper way to do this. */
    warpCoord(
        output_area.xmin - 2, output_area.ymin - 2, sample_data->perspectiveMatrix, UVs[0], deriv);
    warpCoord(
        output_area.xmax + 2, output_area.ymin - 2, sample_data->perspectiveMatrix, UVs[1], deriv);
    warpCoord(
        output_area.xmax + 2, output_area.ymax + 2, sample_data->perspectiveMatrix, UVs[2], deriv);
    warpCoord(
        output_area.xmin - 2, output_area.ymax + 2, sample_data->perspectiveMatrix, UVs[3], deriv);
    for (int i = 0; i < 4; i++) {
      minmax_v2v2_v2(min, max, UVs[i]);
    }
  }

  r_input_area.xmin = min[0] - 1;
  r_input_area.ymin = min[1] - 1;
  r_input_area.xmax = max[0] + 1;
  r_input_area.ymax = max[1] + 1;
#endif
}

/* ******** PlaneDistort Mask ******** */

PlaneDistortMaskOperation::PlaneDistortMaskOperation() : PlaneDistortBaseOperation()
{
  addOutputSocket(DataType::Value);

  /* Currently hardcoded to 8 samples. */
  m_osa = 8;
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

void PlaneDistortMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> UNUSED(inputs))
{
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    int inside_count = 0;
    for (const int motion_sample : IndexRange(this->m_motion_blur_samples)) {
      MotionSample &sample = this->m_samples[motion_sample];
      inside_count += get_jitter_samples_inside_count(it.x, it.y, sample);
    }
    *it.out = (float)inside_count / (this->m_osa * this->m_motion_blur_samples);
  }
}

int PlaneDistortMaskOperation::get_jitter_samples_inside_count(int x,
                                                               int y,
                                                               MotionSample &sample_data)
{
  float point[2];
  int inside_count = 0;
  for (int sample = 0; sample < this->m_osa; sample++) {
    point[0] = x + this->m_jitter[sample][0];
    point[1] = y + this->m_jitter[sample][1];
    if (isect_point_tri_v2(point,
                           sample_data.frameSpaceCorners[0],
                           sample_data.frameSpaceCorners[1],
                           sample_data.frameSpaceCorners[2]) ||
        isect_point_tri_v2(point,
                           sample_data.frameSpaceCorners[0],
                           sample_data.frameSpaceCorners[2],
                           sample_data.frameSpaceCorners[3])) {
      inside_count++;
    }
  }
  return inside_count;
}

}  // namespace blender::compositor
