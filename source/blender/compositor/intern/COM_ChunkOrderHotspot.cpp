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

#include "COM_ChunkOrderHotspot.h"
#include <math.h>

ChunkOrderHotspot::ChunkOrderHotspot(int x, int y, float addition)
{
	this->x = x;
	this->y = y;
	this->addition = addition;
}

double ChunkOrderHotspot::determineDistance(int x, int y)
{
	int dx = x-this->x;
	int dy = y-this->y;
	double result = sqrt((double)(dx*dx+dy*dy));
	result += this->addition;
	return result;
}
