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
#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief temporarily storage during execution of Tone-map
 * \ingroup operation
 */
typedef struct AvgLogLum {
  float al;
  float auto_key;
  float lav;
  float cav[4];
  float igm;
} AvgLogLum;

/**
 * \brief base class of tonemap, implementing the simple tonemap
 * \ingroup operation
 */
class TonemapOperation : public NodeOperation {
 protected:
  /**
   * \brief Cached reference to the reader
   */
  SocketReader *m_imageReader;

  /**
   * \brief settings of the Tonemap
   */
  NodeTonemap *m_data;

  /**
   * \brief temporarily cache of the execution storage
   */
  AvgLogLum *m_cachedInstance;

 public:
  TonemapOperation();

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

  void setData(NodeTonemap *data)
  {
    this->m_data = data;
  }

  bool determineDependingAreaOfInterest(rcti *input,
                                        ReadBufferOperation *readOperation,
                                        rcti *output) override;
};

/**
 * \brief class of tonemap, implementing the photoreceptor tonemap
 * most parts have already been done in TonemapOperation
 * \ingroup operation
 */

class PhotoreceptorTonemapOperation : public TonemapOperation {
 public:
  /**
   * The inner loop of this operation.
   */
  void executePixel(float output[4], int x, int y, void *data) override;
};

}  // namespace blender::compositor
