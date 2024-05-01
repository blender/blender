/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class CropBaseOperation : public MultiThreadedOperation {
 protected:
  NodeTwoXYs *settings_;
  bool relative_;
  int xmax_;
  int xmin_;
  int ymax_;
  int ymin_;

  void update_area();

 public:
  CropBaseOperation();
  void init_execution() override;
  void set_crop_settings(NodeTwoXYs *settings)
  {
    settings_ = settings;
  }
  void set_relative(bool rel)
  {
    relative_ = rel;
  }
};

class CropOperation : public CropBaseOperation {
 private:
 public:
  CropOperation();
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class CropImageOperation : public CropBaseOperation {
 private:
 public:
  CropImageOperation();
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
