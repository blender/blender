/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_NodeOperation.h"

namespace blender::compositor {

/**
 * \brief SummedAreaTableOperation class computes the summed area table.
 */
class SummedAreaTableOperation : public NodeOperation {

 public:
  SummedAreaTableOperation();

  enum eMode { Identity = 1, Squared };

  void set_mode(const eMode mode);
  eMode get_mode();

  void get_area_of_interest(int input_idx, const rcti &output_area, rcti &r_input_area) override;

  void update_memory_buffer(MemoryBuffer *output,
                            const rcti &area,
                            Span<MemoryBuffer *> inputs) override;

 private:
  eMode mode_;
};

/* Computes the sum of the rectangular region defined by the given area from the
 * given summed area table. All coordinates within the area are included. */
float4 summed_area_table_sum(MemoryBuffer *buffer, const rcti &area);

}  // namespace blender::compositor
