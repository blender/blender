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

namespace blender::compositor {

class GaussianXBlurOperation : public BlurBaseOperation {
 private:
  float *m_gausstab;
#ifdef BLI_HAVE_SSE2
  __m128 *m_gausstab_sse;
#endif
  int m_filtersize;
  void updateGauss();

 public:
  GaussianXBlurOperation();

  /**
   * \brief The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;

  void executeOpenCL(OpenCLDevice *device,
                     MemoryBuffer *outputMemoryBuffer,
                     cl_mem clOutputBuffer,
                     MemoryBuffer **inputMemoryBuffers,
                     std::list<cl_mem> *clMemToCleanUp,
                     std::list<cl_kernel> *clKernelsToCleanUp) override;

  /**
   * \brief initialize the execution
   */
  void initExecution() override;

  /**
   * \brief Deinitialize the execution
   */
  void deinitExecution() override;

  void *initializeTileData(rcti *rect) override;
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void checkOpenCL()
  {
    flags.open_cl = (m_data.sizex >= 128);
  }
};

}  // namespace blender::compositor
