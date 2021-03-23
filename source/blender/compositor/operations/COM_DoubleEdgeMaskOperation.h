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

namespace blender::compositor {

class DoubleEdgeMaskOperation : public NodeOperation {
 private:
  /**
   * Cached reference to the inputProgram
   */
  SocketReader *m_inputOuterMask;
  SocketReader *m_inputInnerMask;
  bool m_adjacentOnly;
  bool m_keepInside;
  float *m_cachedInstance;

 public:
  DoubleEdgeMaskOperation();

  void doDoubleEdgeMask(float *imask, float *omask, float *res);
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

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;

  void setAdjecentOnly(bool adjacentOnly)
  {
    this->m_adjacentOnly = adjacentOnly;
  }
  void setKeepInside(bool keepInside)
  {
    this->m_keepInside = keepInside;
  }
};

}  // namespace blender::compositor
