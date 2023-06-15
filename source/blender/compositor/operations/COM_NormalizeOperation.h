/* SPDX-FileCopyrightText: 2012 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_MultiThreadedOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief base class of normalize, implementing the simple normalize
 * \ingroup operation
 */
class NormalizeOperation : public MultiThreadedOperation {
 protected:
  /**
   * \brief Cached reference to the reader
   */
  SocketReader *image_reader_;

  /**
   * \brief temporarily cache of the execution storage
   * it stores `x->min` and `y->multiply`.
   */
  NodeTwoFloats *cached_instance_;

 public:
  NormalizeOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  void *initialize_tile_data(rcti *rect) override;
  void deinitialize_tile_data(rcti *rect, void *data) override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;
  void update_memory_buffer_started(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
  void update_memory_buffer_partial(MemoryBuffer *output,
                                    const rcti &area,
                                    Span<MemoryBuffer *> inputs) override;
};

}  // namespace blender::compositor
