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

#include "COM_ChunkOrder.h"
#include "BLI_math.h"

ChunkOrder::ChunkOrder()
{
  distance = 0.0;
  number = 0;
  x = 0;
  y = 0;
}

void ChunkOrder::update_distance(ChunkOrderHotspot **hotspots, unsigned int len_hotspots)
{
  double new_distance = FLT_MAX;
  for (int index = 0; index < len_hotspots; index++) {
    ChunkOrderHotspot *hotspot = hotspots[index];
    double distance_to_hotspot = hotspot->calc_distance(x, y);
    if (distance_to_hotspot < new_distance) {
      new_distance = distance_to_hotspot;
    }
  }
  this->distance = new_distance;
}

bool operator<(const ChunkOrder &a, const ChunkOrder &b)
{
  return a.distance < b.distance;
}
