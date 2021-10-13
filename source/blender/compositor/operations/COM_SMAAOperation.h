/*
 * Copyright 2017, Blender Foundation.
 *
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
 * Contributor: IRIE Shinsuke
 */

#pragma once

#include "COM_MultiThreadedOperation.h"

namespace blender::compositor {

/*-----------------------------------------------------------------------------*/
/* Edge Detection (First Pass) */

class SMAAEdgeDetectionOperation : public MultiThreadedOperation {
 protected:
  SocketReader *imageReader_;
  SocketReader *valueReader_;

  float threshold_;
  float contrast_limit_;

 public:
  SMAAEdgeDetectionOperation();

  /**
   * the inner loop of this program
   */
  virtual void executePixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void initExecution() override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  void setThreshold(float threshold);

  void setLocalContrastAdaptationFactor(float factor);

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

/*-----------------------------------------------------------------------------*/
/*  Blending Weight Calculation (Second Pass) */

class SMAABlendingWeightCalculationOperation : public MultiThreadedOperation {
 private:
  SocketReader *imageReader_;
  std::function<void(int x, int y, float *out)> sample_image_fn_;
  int corner_rounding_;

 public:
  SMAABlendingWeightCalculationOperation();

  /**
   * the inner loop of this program
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

  void setCornerRounding(float rounding);

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;

 private:
  /* Diagonal Search Functions */
  int searchDiag1(int x, int y, int dir, bool *found);
  int searchDiag2(int x, int y, int dir, bool *found);
  void calculateDiagWeights(int x, int y, const float edges[2], float weights[2]);
  bool isVerticalSearchUnneeded(int x, int y);

  /* Horizontal/Vertical Search Functions */
  int searchXLeft(int x, int y);
  int searchXRight(int x, int y);
  int searchYUp(int x, int y);
  int searchYDown(int x, int y);

  /*  Corner Detection Functions */
  void detectHorizontalCornerPattern(float weights[2], int left, int right, int y, int d1, int d2);
  void detectVerticalCornerPattern(float weights[2], int x, int top, int bottom, int d1, int d2);
};

/*-----------------------------------------------------------------------------*/
/* Neighborhood Blending (Third Pass) */

class SMAANeighborhoodBlendingOperation : public MultiThreadedOperation {
 private:
  SocketReader *image1Reader_;
  SocketReader *image2Reader_;

 public:
  SMAANeighborhoodBlendingOperation();

  /**
   * the inner loop of this program
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

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
