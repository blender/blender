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

#ifndef _COM_KeyingDespillOperation_h
#define _COM_KeyingDespillOperation_h

#include "COM_NodeOperation.h"

/**
  * Class with implementation of keying despill node
  */
class KeyingDespillOperation : public NodeOperation {
protected:
	SocketReader *m_pixelReader;
	SocketReader *m_screenReader;
	float m_despillFactor;

public:
	KeyingDespillOperation();

	void initExecution();
	void deinitExecution();

	void setDespillFactor(float value) {this->m_despillFactor = value;}

	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};

#endif
