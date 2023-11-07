/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_SingleThreadedOperation.h"

namespace blender::compositor {

/**
 * \brief SummedAreaTableOperation class computes the summed area table.
 */
class SummedAreaTableOperation : public SingleThreadedOperation {

 public:
  SummedAreaTableOperation();

  enum eMode { Identity = 1, Squared };

  void set_mode(const eMode mode);
  eMode get_mode();

  /**
   * Initialize the execution
   */
  void init_execution() override;

  /**
   * Deinitialize the execution
   */
  void deinit_execution() override;

  bool determine_depending_area_of_interest(rcti *input,
                                            ReadBufferOperation *read_operation,
                                            rcti *output) override;

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  MemoryBuffer *create_memory_buffer(rcti *rect) override;

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;

 private:
  SocketReader *image_reader_;
  eMode mode_;
};

/* Computes the sum of the rectangular region defined by the given area from the
 * given summed area table. All coordinates within the area are included. */
float4 summed_area_table_sum(MemoryBuffer *buffer, const rcti &area);
float4 summed_area_table_sum_tiled(SocketReader *buffer, const rcti &area);

}  // namespace blender::compositor
