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
    : motion_blur_samples_(1), motion_blur_shutter_(0.5f)
{
}

void PlaneDistortBaseOperation::calculate_corners(const float corners[4][2],
                                                  bool normalized,
                                                  int sample)
{
  BLI_assert(sample < motion_blur_samples_);
  MotionSample *sample_data = &samples_[sample];
  if (normalized) {
    for (int i = 0; i < 4; i++) {
      sample_data->frame_space_corners[i][0] = corners[i][0] * this->get_width();
      sample_data->frame_space_corners[i][1] = corners[i][1] * this->get_height();
    }
  }
  else {
    for (int i = 0; i < 4; i++) {
      sample_data->frame_space_corners[i][0] = corners[i][0];
      sample_data->frame_space_corners[i][1] = corners[i][1];
    }
  }
}

/* ******** PlaneDistort WarpImage ******** */

BLI_INLINE void warp_coord(float x, float y, float matrix[3][3], float uv[2], float deriv[2][2])
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
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_output_socket(DataType::Color);
  pixel_reader_ = nullptr;
  flags_.complex = true;
}

void PlaneDistortWarpImageOperation::calculate_corners(const float corners[4][2],
                                                       bool normalized,
                                                       int sample)
{
  PlaneDistortBaseOperation::calculate_corners(corners, normalized, sample);

  const NodeOperation *image = get_input_operation(0);
  const int width = image->get_width();
  const int height = image->get_height();
  float frame_corners[4][2] = {
      {0.0f, 0.0f}, {(float)width, 0.0f}, {(float)width, (float)height}, {0.0f, (float)height}};
  MotionSample *sample_data = &samples_[sample];
  BKE_tracking_homography_between_two_quads(
      sample_data->frame_space_corners, frame_corners, sample_data->perspective_matrix);
}

void PlaneDistortWarpImageOperation::init_execution()
{
  pixel_reader_ = this->get_input_socket_reader(0);
}

void PlaneDistortWarpImageOperation::deinit_execution()
{
  pixel_reader_ = nullptr;
}

void PlaneDistortWarpImageOperation::execute_pixel_sampled(float output[4],
                                                           float x,
                                                           float y,
                                                           PixelSampler /*sampler*/)
{
  float uv[2];
  float deriv[2][2];
  if (motion_blur_samples_ == 1) {
    warp_coord(x, y, samples_[0].perspective_matrix, uv, deriv);
    pixel_reader_->read_filtered(output, uv[0], uv[1], deriv[0], deriv[1]);
  }
  else {
    zero_v4(output);
    for (int sample = 0; sample < motion_blur_samples_; sample++) {
      float color[4];
      warp_coord(x, y, samples_[sample].perspective_matrix, uv, deriv);
      pixel_reader_->read_filtered(color, uv[0], uv[1], deriv[0], deriv[1]);
      add_v4_v4(output, color);
    }
    mul_v4_fl(output, 1.0f / (float)motion_blur_samples_);
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
  if (motion_blur_samples_ == 1) {
    for (; !it.is_end(); ++it) {
      warp_coord(it.x, it.y, samples_[0].perspective_matrix, uv, deriv);
      input_img->read_elem_filtered(uv[0], uv[1], deriv[0], deriv[1], it.out);
    }
  }
  else {
    for (; !it.is_end(); ++it) {
      zero_v4(it.out);
      for (const int sample : IndexRange(motion_blur_samples_)) {
        float color[4];
        warp_coord(it.x, it.y, samples_[sample].perspective_matrix, uv, deriv);
        input_img->read_elem_filtered(uv[0], uv[1], deriv[0], deriv[1], color);
        add_v4_v4(it.out, color);
      }
      mul_v4_fl(it.out, 1.0f / (float)motion_blur_samples_);
    }
  }
}

bool PlaneDistortWarpImageOperation::determine_depending_area_of_interest(
    rcti *input, ReadBufferOperation *read_operation, rcti *output)
{
  float min[2], max[2];
  INIT_MINMAX2(min, max);

  for (int sample = 0; sample < motion_blur_samples_; sample++) {
    float UVs[4][2];
    float deriv[2][2];
    MotionSample *sample_data = &samples_[sample];
    /* TODO(sergey): figure out proper way to do this. */
    warp_coord(input->xmin - 2, input->ymin - 2, sample_data->perspective_matrix, UVs[0], deriv);
    warp_coord(input->xmax + 2, input->ymin - 2, sample_data->perspective_matrix, UVs[1], deriv);
    warp_coord(input->xmax + 2, input->ymax + 2, sample_data->perspective_matrix, UVs[2], deriv);
    warp_coord(input->xmin - 2, input->ymax + 2, sample_data->perspective_matrix, UVs[3], deriv);
    for (int i = 0; i < 4; i++) {
      minmax_v2v2_v2(min, max, UVs[i]);
    }
  }

  rcti new_input;

  new_input.xmin = min[0] - 1;
  new_input.ymin = min[1] - 1;
  new_input.xmax = max[0] + 1;
  new_input.ymax = max[1] + 1;

  return NodeOperation::determine_depending_area_of_interest(&new_input, read_operation, output);
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
  for (int sample = 0; sample < motion_blur_samples_; sample++) {
    float UVs[4][2];
    float deriv[2][2];
    MotionSample *sample_data = &samples_[sample];
    /* TODO(sergey): figure out proper way to do this. */
    warp_coord(
        output_area.xmin - 2, output_area.ymin - 2, sample_data->perspective_matrix, UVs[0], deriv);
    warp_coord(
        output_area.xmax + 2, output_area.ymin - 2, sample_data->perspective_matrix, UVs[1], deriv);
    warp_coord(
        output_area.xmax + 2, output_area.ymax + 2, sample_data->perspective_matrix, UVs[2], deriv);
    warp_coord(
        output_area.xmin - 2, output_area.ymax + 2, sample_data->perspective_matrix, UVs[3], deriv);
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
  add_output_socket(DataType::Value);

  /* Currently hardcoded to 8 samples. */
  osa_ = 8;
}

void PlaneDistortMaskOperation::init_execution()
{
  BLI_jitter_init(jitter_, osa_);
}

void PlaneDistortMaskOperation::execute_pixel_sampled(float output[4],
                                                      float x,
                                                      float y,
                                                      PixelSampler /*sampler*/)
{
  float point[2];
  int inside_counter = 0;
  if (motion_blur_samples_ == 1) {
    MotionSample *sample_data = &samples_[0];
    for (int sample = 0; sample < osa_; sample++) {
      point[0] = x + jitter_[sample][0];
      point[1] = y + jitter_[sample][1];
      if (isect_point_tri_v2(point,
                             sample_data->frame_space_corners[0],
                             sample_data->frame_space_corners[1],
                             sample_data->frame_space_corners[2]) ||
          isect_point_tri_v2(point,
                             sample_data->frame_space_corners[0],
                             sample_data->frame_space_corners[2],
                             sample_data->frame_space_corners[3])) {
        inside_counter++;
      }
    }
    output[0] = (float)inside_counter / osa_;
  }
  else {
    for (int motion_sample = 0; motion_sample < motion_blur_samples_; motion_sample++) {
      MotionSample *sample_data = &samples_[motion_sample];
      for (int osa_sample = 0; osa_sample < osa_; osa_sample++) {
        point[0] = x + jitter_[osa_sample][0];
        point[1] = y + jitter_[osa_sample][1];
        if (isect_point_tri_v2(point,
                               sample_data->frame_space_corners[0],
                               sample_data->frame_space_corners[1],
                               sample_data->frame_space_corners[2]) ||
            isect_point_tri_v2(point,
                               sample_data->frame_space_corners[0],
                               sample_data->frame_space_corners[2],
                               sample_data->frame_space_corners[3])) {
          inside_counter++;
        }
      }
    }
    output[0] = (float)inside_counter / (osa_ * motion_blur_samples_);
  }
}

void PlaneDistortMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> UNUSED(inputs))
{
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    int inside_count = 0;
    for (const int motion_sample : IndexRange(motion_blur_samples_)) {
      MotionSample &sample = samples_[motion_sample];
      inside_count += get_jitter_samples_inside_count(it.x, it.y, sample);
    }
    *it.out = (float)inside_count / (osa_ * motion_blur_samples_);
  }
}

int PlaneDistortMaskOperation::get_jitter_samples_inside_count(int x,
                                                               int y,
                                                               MotionSample &sample_data)
{
  float point[2];
  int inside_count = 0;
  for (int sample = 0; sample < osa_; sample++) {
    point[0] = x + jitter_[sample][0];
    point[1] = y + jitter_[sample][1];
    if (isect_point_tri_v2(point,
                           sample_data.frame_space_corners[0],
                           sample_data.frame_space_corners[1],
                           sample_data.frame_space_corners[2]) ||
        isect_point_tri_v2(point,
                           sample_data.frame_space_corners[0],
                           sample_data.frame_space_corners[2],
                           sample_data.frame_space_corners[3])) {
      inside_count++;
    }
  }
  return inside_count;
}

}  // namespace blender::compositor
