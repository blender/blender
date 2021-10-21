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
 * Copyright 2021, Blender Foundation.
 */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class TransformOperation : public MultiThreadedOperation {
 private:
  constexpr static int IMAGE_INPUT_INDEX = 0;
  constexpr static int X_INPUT_INDEX = 1;
  constexpr static int Y_INPUT_INDEX = 2;
  constexpr static int DEGREE_INPUT_INDEX = 3;
  constexpr static int SCALE_INPUT_INDEX = 4;

  float rotate_cosine_;
  float rotate_sine_;
  int translate_x_;
  int translate_y_;
  float scale_;
  rcti scale_canvas_;
  rcti rotate_canvas_;
  rcti translate_canvas_;

  /* Set variables. */
  PixelSampler sampler_;
  bool convert_degree_to_rad_;
  float translate_factor_x_;
  float translate_factor_y_;
  bool invert_;
  Size2f max_scale_canvas_size_;

 public:
  TransformOperation();

  void set_translate_factor_xy(float x, float y)
  {
    translate_factor_x_ = x;
    translate_factor_y_ = y;
  }

  void set_convert_rotate_degree_to_rad(bool value)
  {
    convert_degree_to_rad_ = value;
  }

  void set_sampler(PixelSampler sampler)
  {
    sampler_ = sampler;
  }

  void set_invert(bool value)
  {
    invert_ = value;
  }

  void set_scale_canvas_max_size(Size2f size);

  void init_data() override;
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

 private:
  void transform(BuffersIterator<float> &it, const MemoryBuffer *input_img);
  void transform_inverted(BuffersIterator<float> &it, const MemoryBuffer *input_img);
};

}  // namespace blender::compositor
