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

#ifndef _COM_ColorMatteOperation_h
#define _COM_ColorMatteOperation_h
#include "COM_MixOperation.h"


/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ColorMatteOperation : public NodeOperation {
private:
	NodeChroma *m_settings;
	SocketReader *m_inputImageProgram;
	SocketReader *m_inputKeyProgram;
public:
	/**
	 * Default constructor
	 */
	ColorMatteOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

	void initExecution();
	void deinitExecution();

	void setSettings(NodeChroma *nodeChroma) { this->m_settings = nodeChroma; }
};
#endif
