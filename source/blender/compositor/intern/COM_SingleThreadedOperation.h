/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

class SingleThreadedOperation : public NodeOperation {
 private:
  MemoryBuffer *cached_instance_;

 protected:
  inline bool is_cached()
  {
    return cached_instance_ != nullptr;
  }

 public:
  SingleThreadedOperation();

  /**
   * The inner loop of this operation.
   */
  void execute_pixel(float output[4], int x, int y, void *data) override;

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  void *initialize_tile_data(rcti *rect) override;

  virtual MemoryBuffer *create_memory_buffer(rcti *rect) = 0;
};

}  // namespace blender::compositor
