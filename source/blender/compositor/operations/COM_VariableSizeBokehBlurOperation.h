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

//#define COM_DEFOCUS_SEARCH

class VariableSizeBokehBlurOperation : public MultiThreadedOperation, public QualityStepHelper {
 private:
  static constexpr int IMAGE_INPUT_INDEX = 0;
  static constexpr int BOKEH_INPUT_INDEX = 1;
  static constexpr int SIZE_INPUT_INDEX = 2;
#ifdef COM_DEFOCUS_SEARCH
  static constexpr int DEFOCUS_INPUT_INDEX = 3;
#endif

  int m_maxBlur;
  float m_threshold;
  bool m_do_size_scale; /* scale size, matching 'BokehBlurNode' */
  SocketReader *m_inputProgram;
  SocketReader *m_inputBokehProgram;
  SocketReader *m_inputSizeProgram;
#ifdef COM_DEFOCUS_SEARCH
  SocketReader *m_inputSearchProgram;
#endif

 public:
  VariableSizeBokehBlurOperation();

  /**
   * The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void initExecution() override;

  void *initializeTileData(rcti *rect) override;

  void deinitializeTileData(rcti *rect, void *data) override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void setMaxBlur(int maxRadius)
  {
    m_maxBlur = maxRadius;
  }

  void setThreshold(float threshold)
  {
    m_threshold = threshold;
  }

  void setDoScaleSize(bool scale_size)
  {
    m_do_size_scale = scale_size;
  }

  void executeOpenCL(OpenCLDevice *device,
                     MemoryBuffer *outputMemoryBuffer,
                     cl_mem clOutputBuffer,
                     MemoryBuffer **inputMemoryBuffers,
                     std::list<cl_mem> *clMemToCleanUp,
                     std::list<cl_kernel> *clKernelsToCleanUp) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

/* Currently unused. If ever used, it needs full-frame implementation. */
#ifdef COM_DEFOCUS_SEARCH
class InverseSearchRadiusOperation : public NodeOperation {
 private:
  int m_maxBlur;
  SocketReader *m_inputRadius;

 public:
  static const int DIVIDER = 4;

  InverseSearchRadiusOperation();

  /**
   * The inner loop of this operation.
   */
  void executePixelChunk(float output[4], int x, int y, void *data);

  /**
   * Initialize the execution
   */
  void initExecution() override;
  void *initializeTileData(rcti *rect) override;
  void deinitializeTileData(rcti *rect, void *data) override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
  void determine_canvas(const rcti &preferred_area, rcti &r_area) override;

  void setMaxBlur(int maxRadius)
  {
    m_maxBlur = maxRadius;
  }
};
#endif

}  // namespace blender::compositor
