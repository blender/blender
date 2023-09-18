/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  rcti scale_canvas_ = COM_AREA_NONE;
  rcti rotate_canvas_ = COM_AREA_NONE;
  rcti translate_canvas_ = COM_AREA_NONE;

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
  /** Translate -> Rotate -> Scale. */
  void transform(BuffersIterator<float> &it, const MemoryBuffer *input_img);
  /** Scale -> Rotate -> Translate. */
  void transform_inverted(BuffersIterator<float> &it, const MemoryBuffer *input_img);
};

}  // namespace blender::compositor
