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
 *		Dalai Felinto
 */

#ifndef _COM_MapUVOperation_h
#define _COM_MapUVOperation_h
#include "COM_NodeOperation.h"


class MapUVOperation : public NodeOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputUVProgram;
	SocketReader *m_inputColorProgram;

	float m_alpha;
	
public:
	MapUVOperation();

	/**
	 * we need a 3x3 differential filter for UV Input and full buffer for the image
	 */
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	/**
	 * the inner loop of this program
	 */
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

	void pixelTransform(const float xy[2], float r_uv[2], float r_deriv[2][2], float &r_alpha);

	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	void setAlpha(float alpha) { this->m_alpha = alpha; }

private:
	bool read_uv(float x, float y, float &r_u, float &r_v, float &r_alpha);
};

#endif
