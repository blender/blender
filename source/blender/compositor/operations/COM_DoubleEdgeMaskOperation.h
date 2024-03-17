/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_span.hh"

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class DoubleEdgeMaskOperation : public NodeOperation {
 private:
  bool include_all_inner_edges_;
  bool include_edges_of_image_;

  bool is_output_rendered_;

 public:
  DoubleEdgeMaskOperation();

  void compute_boundary(const float *inner_mask,
                        const float *outer_mask,
                        MutableSpan<int2> inner_boundary,
                        MutableSpan<int2> outer_boundary);

  void compute_gradient(const float *inner_mask_buffer,
                        const float *outer_mask_buffer,
                        MutableSpan<int2> flooded_inner_boundary,
                        MutableSpan<int2> flooded_outer_boundary,
                        float *output_mask);

  void compute_double_edge_mask(const float *inner_mask,
                                const float *outer_mask,
                                float *output_mask);

  void set_include_all_inner_edges(bool include_all_inner_edges)
  {
    include_all_inner_edges_ = include_all_inner_edges;
  }
  void set_include_edges_of_image(bool include_edges_of_image)
  {
    include_edges_of_image_ = include_edges_of_image;
  }

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
