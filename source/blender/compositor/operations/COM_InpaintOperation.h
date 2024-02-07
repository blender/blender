/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_array.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"

#include "COM_NodeOperation.h"

namespace blender::compositor {

class InpaintSimpleOperation : public NodeOperation {
 protected:
  SocketReader *input_image_program_;
  MemoryBuffer *cached_buffer_;
  bool cached_buffer_ready_;
  int max_distance_;

 public:
  InpaintSimpleOperation();

  void compute_inpainting_region(const MemoryBuffer *input,
                                 const MemoryBuffer &inpainted_region,
                                 const MemoryBuffer &distance_to_boundary,
                                 MemoryBuffer *output);

  void fill_inpainting_region(const MemoryBuffer *input,
                              Span<int2> flooded_boundary,
                              MemoryBuffer &filled_region,
                              MemoryBuffer &distance_to_boundary_buffer,
                              MemoryBuffer &smoothing_radius_buffer);

  Array<int2> compute_inpainting_boundary(const MemoryBuffer *input);

  void inpaint(const MemoryBuffer *input, MemoryBuffer *output);

  void execute_pixel(float output[4], int x, int y, void *data) override;

  void init_execution() override;

  void *initialize_tile_data(rcti *rect) override;

  void deinit_execution() override;

  void set_max_distance(int max_distance)
  {
    max_distance_ = max_distance;
  }

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
