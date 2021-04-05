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

#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

namespace blender::compositor {

//#define COM_DEFOCUS_SEARCH

class VariableSizeBokehBlurOperation : public NodeOperation, public QualityStepHelper {
 private:
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
    this->m_maxBlur = maxRadius;
  }

  void setThreshold(float threshold)
  {
    this->m_threshold = threshold;
  }

  void setDoScaleSize(bool scale_size)
  {
    this->m_do_size_scale = scale_size;
  }

  void executeOpenCL(OpenCLDevice *device,
                     MemoryBuffer *outputMemoryBuffer,
                     cl_mem clOutputBuffer,
                     MemoryBuffer **inputMemoryBuffers,
                     std::list<cl_mem> *clMemToCleanUp,
                     std::list<cl_kernel> *clKernelsToCleanUp) override;
};

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
  void determineResolution(unsigned int resolution[2],
                           unsigned int preferredResolution[2]) override;

  void setMaxBlur(int maxRadius)
  {
    this->m_maxBlur = maxRadius;
  }
};
#endif

}  // namespace blender::compositor
