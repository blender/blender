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

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

class CropBaseOperation : public MultiThreadedOperation {
 protected:
  SocketReader *input_operation_;
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
  void deinit_execution() override;
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
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class CropImageOperation : public CropBaseOperation {
 private:
 public:
  CropImageOperation();
  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
