/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_DistanceYCCMatteOperation.h"

namespace blender::compositor {

float DistanceYCCMatteOperation::calculate_distance(const float key[4], const float image[4])
{
  /* only measure the second 2 values */
  return len_v2v2(key + 1, image + 1);
}

}  // namespace blender::compositor
