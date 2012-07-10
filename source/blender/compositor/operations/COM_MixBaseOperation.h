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

#ifndef _COM_MixBaseOperation_h
#define _COM_MixBaseOperation_h
#include "COM_NodeOperation.h"


/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class MixBaseOperation : public NodeOperation {
protected:
	/**
	 * Prefetched reference to the inputProgram
	 */
	SocketReader *m_inputValueOperation;
	SocketReader *m_inputColor1Operation;
	SocketReader *m_inputColor2Operation;
	bool m_valueAlphaMultiply;
	bool m_useClamp;

	void clampIfNeeded(float *color);
public:
	/**
	 * Default constructor
	 */
	MixBaseOperation();
	
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	
	void setUseValueAlphaMultiply(const bool value) { this->m_valueAlphaMultiply = value; }
	bool useValueAlphaMultiply() { return this->m_valueAlphaMultiply; }
	void setUseClamp(bool value) { this->m_useClamp = value; }
};
#endif
