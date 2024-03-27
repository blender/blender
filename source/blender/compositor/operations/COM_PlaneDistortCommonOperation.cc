/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_PlaneDistortCommonOperation.h"

#include "BLI_jitter_2d.h"
#include "BLI_math_geom.h"
#include "BLI_math_matrix.h"

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

  /* Offset so that pixel center corresponds to a (0.5, 0.5), which helps keeping transformed
   * image sharp. */
  uv[0] += 0.5f;
  uv[1] += 0.5f;

  deriv[0][0] = (matrix[0][0] - matrix[0][2] * uv[0]) / vec[2];
  deriv[1][0] = (matrix[0][1] - matrix[0][2] * uv[1]) / vec[2];
  deriv[0][1] = (matrix[1][0] - matrix[1][2] * uv[0]) / vec[2];
  deriv[1][1] = (matrix[1][1] - matrix[1][2] * uv[1]) / vec[2];
}

PlaneDistortWarpImageOperation::PlaneDistortWarpImageOperation() : PlaneDistortBaseOperation()
{
  this->add_input_socket(DataType::Color, ResizeMode::Align);
  this->add_output_socket(DataType::Color);
}

void PlaneDistortWarpImageOperation::calculate_corners(const float corners[4][2],
                                                       bool normalized,
                                                       int sample)
{
  PlaneDistortBaseOperation::calculate_corners(corners, normalized, sample);

  const NodeOperation *image = get_input_operation(0);
  const int width = image->get_width();
  const int height = image->get_height();

  MotionSample *sample_data = &samples_[sample];

  /* If the image which is to be warped empty assume unit transform and don't attempt to calculate
   * actual homography (otherwise homography solver will attempt to deal with singularity). */
  if (width == 0 || height == 0) {
    unit_m3(sample_data->perspective_matrix);
    return;
  }

  float frame_corners[4][2] = {
      {0.0f, 0.0f}, {float(width), 0.0f}, {float(width), float(height)}, {0.0f, float(height)}};
  BKE_tracking_homography_between_two_quads(
      sample_data->frame_space_corners, frame_corners, sample_data->perspective_matrix);
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
      input_img->read_elem_filtered(uv[0], uv[1], deriv[0], deriv[1], true, it.out);
    }
  }
  else {
    for (; !it.is_end(); ++it) {
      zero_v4(it.out);
      for (const int sample : IndexRange(motion_blur_samples_)) {
        float color[4];
        warp_coord(it.x, it.y, samples_[sample].perspective_matrix, uv, deriv);
        input_img->read_elem_filtered(uv[0], uv[1], deriv[0], deriv[1], true, color);
        add_v4_v4(it.out, color);
      }
      mul_v4_fl(it.out, 1.0f / float(motion_blur_samples_));
    }
  }
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
    warp_coord(output_area.xmin - 2,
               output_area.ymin - 2,
               sample_data->perspective_matrix,
               UVs[0],
               deriv);
    warp_coord(output_area.xmax + 2,
               output_area.ymin - 2,
               sample_data->perspective_matrix,
               UVs[1],
               deriv);
    warp_coord(output_area.xmax + 2,
               output_area.ymax + 2,
               sample_data->perspective_matrix,
               UVs[2],
               deriv);
    warp_coord(output_area.xmin - 2,
               output_area.ymax + 2,
               sample_data->perspective_matrix,
               UVs[3],
               deriv);
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
}

void PlaneDistortMaskOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                             const rcti &area,
                                                             Span<MemoryBuffer *> /*inputs*/)
{
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    float accumulated_mask = 0.0f;
    const float2 point = float2(it.x, it.y) + 0.5f;
    for (const int motion_sample : IndexRange(motion_blur_samples_)) {
      MotionSample &sample = samples_[motion_sample];
      const bool is_inside_plane = isect_point_tri_v2(point,
                                                      sample.frame_space_corners[0],
                                                      sample.frame_space_corners[1],
                                                      sample.frame_space_corners[2]) ||
                                   isect_point_tri_v2(point,
                                                      sample.frame_space_corners[0],
                                                      sample.frame_space_corners[2],
                                                      sample.frame_space_corners[3]);
      accumulated_mask += is_inside_plane ? 1.0f : 0.0f;
    }
    *it.out = accumulated_mask / motion_blur_samples_;
  }
}

}  // namespace blender::compositor
