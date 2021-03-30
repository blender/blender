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
#include "DNA_light_types.h"

namespace blender::compositor {

class GlareThresholdOperation : public NodeOperation {
 private:
  /**
   * \brief Cached reference to the inputProgram
   */
  SocketReader *m_inputProgram;

  /**
   * \brief settings of the glare node.
   */
  NodeGlare *m_settings;

 public:
  GlareThresholdOperation();

  /**
   * The inner loop of this operation.
   */
  void executePixelSampled(float output[4], float x, float y, PixelSampler sampler) override;

  /**
   * Initialize the execution
   */
  void initExecution() override;

  /**
   * Deinitialize the execution
   */
  void deinitExecution() override;

  void setGlareSettings(NodeGlare *settings)
  {
    this->m_settings = settings;
  }

  void determineResolution(unsigned int resolution[2],
                           unsigned int preferredResolution[2]) override;
};

}  // namespace blender::compositor
