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

#ifndef __COM_ZCOMBINEOPERATION_H__
#define __COM_ZCOMBINEOPERATION_H__
#include "COM_MixOperation.h"


/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ZCombineOperation : public NodeOperation {
protected:
	SocketReader *m_image1Reader;
	SocketReader *m_depth1Reader;
	SocketReader *m_image2Reader;
	SocketReader *m_depth2Reader;
public:
	/**
	 * Default constructor
	 */
	ZCombineOperation();

	void initExecution();
	void deinitExecution();

	/**
	 * the inner loop of this program
	 */
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class ZCombineAlphaOperation : public ZCombineOperation {
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class ZCombineMaskOperation : public NodeOperation {
protected:
	SocketReader *m_maskReader;
	SocketReader *m_image1Reader;
	SocketReader *m_image2Reader;
public:
	ZCombineMaskOperation();

	void initExecution();
	void deinitExecution();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};
class ZCombineMaskAlphaOperation : public ZCombineMaskOperation {
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

#endif
