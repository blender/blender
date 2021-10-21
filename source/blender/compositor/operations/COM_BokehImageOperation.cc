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

#include "COM_BokehImageOperation.h"

namespace blender::compositor {

BokehImageOperation::BokehImageOperation()
{
  this->add_output_socket(DataType::Color);
  delete_data_ = false;
}
void BokehImageOperation::init_execution()
{
  center_[0] = get_width() / 2;
  center_[1] = get_height() / 2;
  inverse_rounding_ = 1.0f - data_->rounding;
  circular_distance_ = get_width() / 2;
  flap_rad_ = (float)(M_PI * 2) / data_->flaps;
  flap_rad_add_ = data_->angle;
  while (flap_rad_add_ < 0.0f) {
    flap_rad_add_ += (float)(M_PI * 2.0);
  }
  while (flap_rad_add_ > (float)M_PI) {
    flap_rad_add_ -= (float)(M_PI * 2.0);
  }
}
void BokehImageOperation::detemine_start_point_of_flap(float r[2], int flap_number, float distance)
{
  r[0] = sinf(flap_rad_ * flap_number + flap_rad_add_) * distance + center_[0];
  r[1] = cosf(flap_rad_ * flap_number + flap_rad_add_) * distance + center_[1];
}
float BokehImageOperation::is_inside_bokeh(float distance, float x, float y)
{
  float inside_bokeh = 0.0f;
  const float deltaX = x - center_[0];
  const float deltaY = y - center_[1];
  float closest_point[2];
  float line_p1[2];
  float line_p2[2];
  float point[2];
  point[0] = x;
  point[1] = y;

  const float distance_to_center = len_v2v2(point, center_);
  const float bearing = (atan2f(deltaX, deltaY) + (float)(M_PI * 2.0));
  int flap_number = (int)((bearing - flap_rad_add_) / flap_rad_);

  detemine_start_point_of_flap(line_p1, flap_number, distance);
  detemine_start_point_of_flap(line_p2, flap_number + 1, distance);
  closest_to_line_v2(closest_point, point, line_p1, line_p2);

  const float distance_line_to_center = len_v2v2(center_, closest_point);
  const float distance_rounding_to_center = inverse_rounding_ * distance_line_to_center +
                                            data_->rounding * distance;

  const float catadioptric_distance_to_center = distance_rounding_to_center * data_->catadioptric;
  if (distance_rounding_to_center >= distance_to_center &&
      catadioptric_distance_to_center <= distance_to_center) {
    if (distance_rounding_to_center - distance_to_center < 1.0f) {
      inside_bokeh = (distance_rounding_to_center - distance_to_center);
    }
    else if (data_->catadioptric != 0.0f &&
             distance_to_center - catadioptric_distance_to_center < 1.0f) {
      inside_bokeh = (distance_to_center - catadioptric_distance_to_center);
    }
    else {
      inside_bokeh = 1.0f;
    }
  }
  return inside_bokeh;
}
void BokehImageOperation::execute_pixel_sampled(float output[4],
                                                float x,
                                                float y,
                                                PixelSampler /*sampler*/)
{
  float shift = data_->lensshift;
  float shift2 = shift / 2.0f;
  float distance = circular_distance_;
  float inside_bokeh_max = is_inside_bokeh(distance, x, y);
  float inside_bokeh_med = is_inside_bokeh(distance - fabsf(shift2 * distance), x, y);
  float inside_bokeh_min = is_inside_bokeh(distance - fabsf(shift * distance), x, y);
  if (shift < 0) {
    output[0] = inside_bokeh_max;
    output[1] = inside_bokeh_med;
    output[2] = inside_bokeh_min;
  }
  else {
    output[0] = inside_bokeh_min;
    output[1] = inside_bokeh_med;
    output[2] = inside_bokeh_max;
  }
  output[3] = (inside_bokeh_max + inside_bokeh_med + inside_bokeh_min) / 3.0f;
}

void BokehImageOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                       const rcti &area,
                                                       Span<MemoryBuffer *> UNUSED(inputs))
{
  const float shift = data_->lensshift;
  const float shift2 = shift / 2.0f;
  const float distance = circular_distance_;
  for (BuffersIterator<float> it = output->iterate_with({}, area); !it.is_end(); ++it) {
    const float inside_bokeh_max = is_inside_bokeh(distance, it.x, it.y);
    const float inside_bokeh_med = is_inside_bokeh(
        distance - fabsf(shift2 * distance), it.x, it.y);
    const float inside_bokeh_min = is_inside_bokeh(distance - fabsf(shift * distance), it.x, it.y);
    if (shift < 0) {
      it.out[0] = inside_bokeh_max;
      it.out[1] = inside_bokeh_med;
      it.out[2] = inside_bokeh_min;
    }
    else {
      it.out[0] = inside_bokeh_min;
      it.out[1] = inside_bokeh_med;
      it.out[2] = inside_bokeh_max;
    }
    it.out[3] = (inside_bokeh_max + inside_bokeh_med + inside_bokeh_min) / 3.0f;
  }
}

void BokehImageOperation::deinit_execution()
{
  if (delete_data_) {
    if (data_) {
      delete data_;
      data_ = nullptr;
    }
  }
}

void BokehImageOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  BLI_rcti_init(&r_area,
                preferred_area.xmin,
                preferred_area.xmin + COM_BLUR_BOKEH_PIXELS,
                preferred_area.ymin,
                preferred_area.ymin + COM_BLUR_BOKEH_PIXELS);
}

}  // namespace blender::compositor
