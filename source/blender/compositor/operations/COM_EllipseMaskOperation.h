/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class EllipseMaskOperation : public MultiThreadedOperation {
 private:
  using MaskFunc = std::function<float(bool is_inside, const float *mask, const float *value)>;

  float sine_;
  float cosine_;
  float aspect_ratio_;
  int mask_type_;

  NodeEllipseMask *data_;

 public:
  EllipseMaskOperation();

  void init_execution() override;

  void set_data(NodeEllipseMask *data)
  {
    data_ = data;
  }

  void set_mask_type(int mask_type)
  {
    mask_type_ = mask_type;
  }

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  void apply_mask(MemoryBuffer *output,
                  const rcti &area,
                  Span<MemoryBuffer *> inputs,
                  MaskFunc mask_func);
};

}  // namespace blender::compositor
