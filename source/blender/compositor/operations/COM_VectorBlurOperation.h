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
#include "DNA_node_types.h"

namespace blender::compositor {

class VectorBlurOperation : public NodeOperation, public QualityStepHelper {
 private:
  /**
   * \brief Cached reference to the inputProgram
   */
  SocketReader *m_inputImageProgram;
  SocketReader *m_inputSpeedProgram;
  SocketReader *m_inputZProgram;

  /**
   * \brief settings of the glare node.
   */
  NodeBlurData *m_settings;

  float *m_cachedInstance;

 public:
  VectorBlurOperation();

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

  void *initializeTileData(rcti *rect) override;

  void setVectorBlurSettings(NodeBlurData *settings)
  {
    this->m_settings = settings;
  }
  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

 protected:
  void generateVectorBlur(float *data,
                          MemoryBuffer *inputImage,
                          MemoryBuffer *inputSpeed,
                          MemoryBuffer *inputZ);
};

}  // namespace blender::compositor
