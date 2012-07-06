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

#ifndef _COM_SetSamplerOperation_h
#define _COM_SetSamplerOperation_h
#include "COM_NodeOperation.h"


/**
 * this program converts an input color to an output Sampler.
 * it assumes we are in sRGB color space.
 */
class SetSamplerOperation : public NodeOperation {
private:
	PixelSampler m_sampler;
	SocketReader *m_reader;
public:
	/**
	 * Default constructor
	 */
	SetSamplerOperation();
	
	void setSampler(PixelSampler sampler) { this->m_sampler = sampler; }
	
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer * inputBuffers[]);
	void initExecution();
	void deinitExecution();
};
#endif
