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
#include "COM_QualityStepHelper.h"

namespace blender::compositor {

class BokehBlurOperation : public MultiThreadedOperation, public QualityStepHelper {
 private:
  SocketReader *inputProgram_;
  SocketReader *inputBokehProgram_;
  SocketReader *inputBoundingBoxReader_;
  void updateSize();
  float size_;
  bool sizeavailable_;

  float bokehMidX_;
  float bokehMidY_;
  float bokehDimension_;
  bool extend_bounds_;

 public:
  BokehBlurOperation();

  void init_data() override;

  void *initializeTileData(rcti *rect) override;
  /**
   * The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void initExecution() override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void setSize(float size)
  {
    size_ = size;
    sizeavailable_ = true;
  }

  void executeOpenCL(OpenCLDevice *device,
                     MemoryBuffer *outputMemoryBuffer,
                     cl_mem clOutputBuffer,
                     MemoryBuffer **inputMemoryBuffers,
                     std::list<cl_mem> *clMemToCleanUp,
                     std::list<cl_kernel> *clKernelsToCleanUp) override;

  void setExtendBounds(bool extend_bounds)
  {
    extend_bounds_ = extend_bounds;
  }

  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
