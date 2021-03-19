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

#include "COM_DistanceYCCMatteOperation.h"
#include "BLI_math.h"

DistanceYCCMatteOperation::DistanceYCCMatteOperation()
{
  /* pass */
}

float DistanceYCCMatteOperation::calculateDistance(float key[4], float image[4])
{
  /* only measure the second 2 values */
  return len_v2v2(key + 1, image + 1);
}
