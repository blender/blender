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

#include "COM_BlurBaseOperation.h"
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

namespace blender::compositor {

class GaussianBokehBlurOperation : public BlurBaseOperation {
 private:
  float *gausstab_;
  int radx_, rady_;
  float radxf_;
  float radyf_;
  void updateGauss();

 public:
  GaussianBokehBlurOperation();
  void init_data() override;
  void initExecution() override;
  void *initializeTileData(rcti *rect) override;
  /**
   * The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(const int input_idx,
                            const rcti &output_area,
                            rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

class GaussianBlurReferenceOperation : public BlurBaseOperation {
 private:
  float **maintabs_;

  void updateGauss();
  int filtersizex_;
  int filtersizey_;
  float radx_;
  float rady_;

 public:
  GaussianBlurReferenceOperation();
  void init_data() override;
  void initExecution() override;
  void *initializeTileData(rcti *rect) override;
  /**
   * The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void get_area_of_interest(const int input_idx,
                            const rcti &output_area,
                            rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
