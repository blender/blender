/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

/**
 * \brief Pixelate operation
 *
 * The Tile compositor is by default sub-pixel accurate.
 * For some setups you don want this.
 * This operation will remove the sub-pixel accuracy
 */
class PixelateOperation : public NodeOperation {
 private:
  /**
   * \brief cached reference to the input operation
   */
  SocketReader *input_operation_;

 public:
  /**
   * \brief PixelateOperation
   * \param data_type: the datatype to create this operator for (saves datatype conversions)
   */
  PixelateOperation(DataType data_type);

  /**
   * \brief initialization of the execution
   */
  void init_execution() override;

  /**
   * \brief de-initialization of the execution
   */
  void deinit_execution() override;

  /**
   * \brief execute_pixel
   * \param output: result
   * \param x: x-coordinate
   * \param y: y-coordinate
   * \param sampler: sampler
   */
  void execute_pixel_sampled(float output[4], float x, float y, PixelSampler sampler) override;
};

}  // namespace blender::compositor
