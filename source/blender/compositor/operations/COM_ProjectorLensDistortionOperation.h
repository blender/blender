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
#include "DNA_node_types.h"

namespace blender::compositor {

class ProjectorLensDistortionOperation : public MultiThreadedOperation {
 private:
  /**
   * Cached reference to the inputProgram
   */
  SocketReader *inputProgram_;

  float dispersion_;
  /* TODO(manzanilla): to be removed with tiled implementation. */
  bool dispersionAvailable_;

  float kr_, kr2_;

 public:
  ProjectorLensDistortionOperation();

  /**
   * The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;

  void init_data() override;
  /**
   * Initialize the execution
   */
  void initExecution() override;

  void *initializeTileData(rcti *rect) override;
  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void updateDispersion();

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;
  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
