/*
 * Copyright 2011, Blender Foundation.
 *
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_ChunkOrder.h"
#include "BLI_math.h"

ChunkOrder::ChunkOrder()
{
	this->distance = 0.0;
	this->number = 0;
	this->x = 0;
	this->y = 0;
}

void ChunkOrder::determineDistance(ChunkOrderHotspot **hotspots, unsigned int numberOfHotspots)
{
	unsigned int index;
	double distance = MAXFLOAT;
	for (index = 0 ; index < numberOfHotspots ; index ++) {
		ChunkOrderHotspot *hotspot = hotspots[index];
		double ndistance = hotspot->determineDistance(this->x, this->y);
		if (ndistance < distance) {
			distance = ndistance;
		}
	}
	this->distance = distance;
}

bool operator<(const ChunkOrder& a, const ChunkOrder& b)
{
	return a.distance < b.distance;
}
