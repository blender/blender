/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_math_vector_types.hh"

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class BokehImageOperation : public MultiThreadedOperation {
 private:
  const NodeBokehImage *data_;

  int resolution_ = COM_BLUR_BOKEH_PIXELS;

  float exterior_angle_;
  float rotation_;
  float roundness_;
  float catadioptric_;
  float lens_shift_;

  /* See the delete_data_on_finish method. */
  bool delete_data_;

  float2 get_regular_polygon_vertex_position(int vertex_index);
  float2 closest_point_on_line(float2 point, float2 line_start, float2 line_end);
  float bokeh(float2 point, float circumradius);

 public:
  BokehImageOperation();

  void init_execution() override;
  void deinit_execution() override;

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void set_data(const NodeBokehImage *data)
  {
    data_ = data;
  }

  void set_resolution(int resolution)
  {
    resolution_ = resolution;
  }

  /**
   * \brief delete_data_on_finish
   *
   * There are cases that the compositor uses this operation on its own (see defocus node)
   * the delete_data_on_finish must only be called when the data has been created by the
   *compositor. It should not be called when the data has been created by the node-editor/user.
   */
  void delete_data_on_finish()
  {
    delete_data_ = true;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
