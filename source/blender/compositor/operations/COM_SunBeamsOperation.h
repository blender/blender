/*
 * Copyright 2014, Blender Foundation.
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
 *		Lukas Toenne
 */

#ifndef _COM_SunBeamsOperation_h
#define _COM_SunBeamsOperation_h

#include "COM_NodeOperation.h"

class SunBeamsOperation : public NodeOperation {
public:
	SunBeamsOperation();

	void executePixel(float output[4], int x, int y, void *data);

	void initExecution();

	void *initializeTileData(rcti *rect);

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	void setData(const NodeSunBeams &data) { m_data = data; }

private:
	NodeSunBeams m_data;

	float m_source_px[2];
	float m_ray_length_px;
};

#endif
