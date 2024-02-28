/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_CalculateMeanOperation.h"
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

namespace blender::compositor {

/**
 * \brief base class of CalculateStandardDeviation,
 * implementing the simple CalculateStandardDeviation.
 * \ingroup operation
 */
class CalculateStandardDeviationOperation : public CalculateMeanOperation {
 protected:
  float standard_deviation_;

  float calculate_value(const MemoryBuffer *input) const override;

 private:
  PixelsSum calc_area_sum(const MemoryBuffer *input, const rcti &area, float mean) const;
};

}  // namespace blender::compositor
