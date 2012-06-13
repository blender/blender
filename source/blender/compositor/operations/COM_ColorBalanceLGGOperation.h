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

#ifndef _COM_ColorBalanceLGGOperation_h
#define _COM_ColorBalanceLGGOperation_h
#include "COM_NodeOperation.h"


/**
 * this program converts an input colour to an output value.
 * it assumes we are in sRGB colour space.
 */
class ColorBalanceLGGOperation : public NodeOperation {
protected:
	/**
	 * Prefetched reference to the inputProgram
	 */
	SocketReader *inputValueOperation;
	SocketReader *inputColorOperation;
	
	float gain[3];
	float lift[3];
	float gamma_inv[3];

public:
	/**
	 * Default constructor
	 */
	ColorBalanceLGGOperation();
	
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
	
	void setGain(float gain[3]) {
		this->gain[0] = gain[0];
		this->gain[1] = gain[1];
		this->gain[2] = gain[2];
	}
	void setLift(float lift[3]) {
		this->lift[0] = lift[0];
		this->lift[1] = lift[1];
		this->lift[2] = lift[2];
	}
	void setGammaInv(float gamma_inv[3]) {
		this->gamma_inv[0] = gamma_inv[0];
		this->gamma_inv[1] = gamma_inv[1];
		this->gamma_inv[2] = gamma_inv[2];
	}
};
#endif
