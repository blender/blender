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

class BokehBlurOperation : public NodeOperation, public QualityStepHelper {
 private:
  SocketReader *m_inputProgram;
  SocketReader *m_inputBokehProgram;
  SocketReader *m_inputBoundingBoxReader;
  void updateSize();
  float m_size;
  bool m_sizeavailable;
  float m_bokehMidX;
  float m_bokehMidY;
  float m_bokehDimension;
  bool m_extend_bounds;

 public:
  BokehBlurOperation();

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
    this->m_size = size;
    this->m_sizeavailable = true;
  }

  void executeOpenCL(OpenCLDevice *device,
                     MemoryBuffer *outputMemoryBuffer,
                     cl_mem clOutputBuffer,
                     MemoryBuffer **inputMemoryBuffers,
                     std::list<cl_mem> *clMemToCleanUp,
                     std::list<cl_kernel> *clKernelsToCleanUp) override;

  void setExtendBounds(bool extend_bounds)
  {
    this->m_extend_bounds = extend_bounds;
  }

  void determineResolution(unsigned int resolution[2],
                           unsigned int preferredResolution[2]) override;
};

}  // namespace blender::compositor
