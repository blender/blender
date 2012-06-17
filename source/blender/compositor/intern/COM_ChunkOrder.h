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

#ifndef _COM_ChunkOrder_h_
#define _COM_ChunkOrder_h_

#include "COM_ChunkOrderHotspot.h"
class ChunkOrder {
private:
	unsigned int number;
	int x;
	int y;
	double distance;
public:
	ChunkOrder();
	void determineDistance(ChunkOrderHotspot **hotspots, unsigned int numberOfHotspots);
	friend bool operator<(const ChunkOrder& a, const ChunkOrder& b);
	
	void setChunkNumber(unsigned int chunknumber) { this->number = chunknumber; }
	void setX(int x) { this->x = x; }
	void setY(int y) { this->y = y; }
	unsigned int getChunkNumber() { return this->number; }
	double getDistance() { return this->distance; }
};

#endif
