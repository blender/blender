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
 * Contributor: Campbell Barton
 */

#ifndef _COM_DespeckleOperation_h
#define _COM_DespeckleOperation_h
#include "COM_NodeOperation.h"

class DespeckleOperation : public NodeOperation {
private:
	float m_threshold;
	float m_threshold_neighbor;

	// int m_filterWidth;
	// int m_filterHeight;

protected:
	SocketReader *m_inputOperation;
	SocketReader *m_inputValueOperation;

public:
	DespeckleOperation();
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	void executePixel(float output[4], int x, int y, void *data);

	void setThreshold(float threshold) { this->m_threshold = threshold; }
	void setThresholdNeighbor(float threshold) { this->m_threshold_neighbor = threshold; }

	void initExecution();
	void deinitExecution();
};

#endif
