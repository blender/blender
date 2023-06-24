/* SPDX-FileCopyrightText: 2011 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_ChunkOrderHotspot.h"
#include <cmath>

namespace blender::compositor {

double ChunkOrderHotspot::calc_distance(int x, int y)
{
  int dx = this->x - x;
  int dy = this->y - y;
  double result = sqrt(double(dx * dx + dy * dy));
  result += double(this->addition);
  return result;
}

}  // namespace blender::compositor
