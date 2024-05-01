/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class MapUVOperation : public MultiThreadedOperation {
 private:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int UV_INPUT_INDEX = 1;

  int uv_width_;
  int uv_height_;
  int image_width_;
  int image_height_;

  float alpha_;
  bool nearest_neighbour_;
  std::function<void(float x, float y, float *out)> uv_input_read_fn_;

 public:
  MapUVOperation();

  void pixel_transform(const float xy[2], float r_uv[2], float r_deriv[2][2], float &r_alpha);

  void init_data() override;

  void set_alpha(float alpha)
  {
    alpha_ = alpha;
  }

  void set_nearest_neighbour(bool nearest_neighbour)
  {
    nearest_neighbour_ = nearest_neighbour;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  bool read_uv(float x, float y, float &r_u, float &r_v, float &r_alpha);
};

}  // namespace blender::compositor
