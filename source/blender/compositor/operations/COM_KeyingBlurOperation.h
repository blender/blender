/*
 * Copyright 2012, Blender Foundation.
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
 *		Sergey Sharybin
 */

#ifndef _COM_KeyingBlurOperation_h
#define _COM_KeyingBlurOperation_h

#include "COM_NodeOperation.h"

/**
 * Class with implementation of bluring for keying node
 */
class KeyingBlurOperation : public NodeOperation {
protected:
	int m_size;
	int m_axis;

public:
	enum BlurAxis {
		BLUR_AXIS_X = 0,
		BLUR_AXIS_Y = 1
	};

	KeyingBlurOperation();

	void setSize(int value) {this->m_size = value;}
	void setAxis(int value) {this->m_axis = value;}

	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);

	void executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data);

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};

#endif
