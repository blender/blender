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

class DilateErodeThresholdOperation : public MultiThreadedOperation {
 public:
  struct PixelData;

 private:
  /**
   * Cached reference to the inputProgram
   */
  SocketReader *inputProgram_;

  float distance_;
  float switch_;
  float inset_;

  /**
   * determines the area of interest to track pixels
   * keep this one as small as possible for speed gain.
   */
  int scope_;

 public:
  DilateErodeThresholdOperation();

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

  void setDistance(float distance)
  {
    distance_ = distance;
  }
  void setSwitch(float sw)
  {
    switch_ = sw;
  }
  void setInset(float inset)
  {
    inset_ = inset;
  }

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class DilateDistanceOperation : public MultiThreadedOperation {
 public:
  struct PixelData;

 protected:
  /**
   * Cached reference to the inputProgram
   */
  SocketReader *inputProgram_;
  float distance_;
  int scope_;

 public:
  DilateDistanceOperation();

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

  void setDistance(float distance)
  {
    distance_ = distance;
  }
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void executeOpenCL(OpenCLDevice *device,
                     MemoryBuffer *outputMemoryBuffer,
                     cl_mem clOutputBuffer,
                     MemoryBuffer **inputMemoryBuffers,
                     std::list<cl_mem> *clMemToCleanUp,
                     std::list<cl_kernel> *clKernelsToCleanUp) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) final;
  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

class ErodeDistanceOperation : public DilateDistanceOperation {
 public:
  ErodeDistanceOperation();

  /**
   * The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;

  void executeOpenCL(OpenCLDevice *device,
                     MemoryBuffer *outputMemoryBuffer,
                     cl_mem clOutputBuffer,
                     MemoryBuffer **inputMemoryBuffers,
                     std::list<cl_mem> *clMemToCleanUp,
                     std::list<cl_kernel> *clKernelsToCleanUp) override;

  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class DilateStepOperation : public MultiThreadedOperation {
 protected:
  /**
   * Cached reference to the inputProgram
   */
  SocketReader *inputProgram_;

  int iterations_;

 public:
  DilateStepOperation();

  /**
   * The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void initExecution() override;

  void *initializeTileData(rcti *rect) override;
  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;
  void deinitializeTileData(rcti *rect, void *data) override;

  void setIterations(int iterations)
  {
    iterations_ = iterations;
  }

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) final;
  virtual void update_memory_buffer_partial(MemoryBuffer *output,
                                            const rcti &area,
                                            Span<MemoryBuffer *> inputs) override;
};

class ErodeStepOperation : public DilateStepOperation {
 public:
  ErodeStepOperation();

  void *initializeTileData(rcti *rect) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
