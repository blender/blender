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

#ifndef __COM_CHUNKORDER_H__
#define __COM_CHUNKORDER_H__

#include "COM_ChunkOrderHotspot.h"
class ChunkOrder {
private:
	unsigned int m_number;
	int m_x;
	int m_y;
	double m_distance;
public:
	ChunkOrder();
	void determineDistance(ChunkOrderHotspot **hotspots, unsigned int numberOfHotspots);
	friend bool operator<(const ChunkOrder &a, const ChunkOrder &b);

	void setChunkNumber(unsigned int chunknumber) { this->m_number = chunknumber; }
	void setX(int x) { this->m_x = x; }
	void setY(int y) { this->m_y = y; }
	unsigned int getChunkNumber() { return this->m_number; }
	double getDistance() { return this->m_distance; }
};

#endif
