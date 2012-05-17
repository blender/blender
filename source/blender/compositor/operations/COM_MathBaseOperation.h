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

#ifndef _COM_MathBaseOperation_h
#define _COM_MathBaseOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class MathBaseOperation : public NodeOperation {
protected:
	/**
	  * Prefetched reference to the inputProgram
	  */
	SocketReader * inputValue1Operation;
	SocketReader * inputValue2Operation;

protected:
	/**
	  * Default constructor
	  */
	MathBaseOperation();
public:
	/**
	  * the inner loop of this program
	  */
	void executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) = 0;
	
	/**
	  * Initialize the execution
	  */
	void initExecution();
	
	/**
	  * Deinitialize the execution
	  */
	void deinitExecution();

};

class MathAddOperation: public MathBaseOperation {
public:
	MathAddOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathSubtractOperation: public MathBaseOperation {
public:
	MathSubtractOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathMultiplyOperation: public MathBaseOperation {
public:
	MathMultiplyOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathDivideOperation: public MathBaseOperation {
public:
	MathDivideOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathSineOperation: public MathBaseOperation {
public:
	MathSineOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathCosineOperation: public MathBaseOperation {
public:
	MathCosineOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathTangentOperation: public MathBaseOperation {
public:
	MathTangentOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};

class MathArcSineOperation: public MathBaseOperation {
public:
	MathArcSineOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathArcCosineOperation: public MathBaseOperation {
public:
	MathArcCosineOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathArcTangentOperation: public MathBaseOperation {
public:
	MathArcTangentOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathPowerOperation: public MathBaseOperation {
public:
	MathPowerOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathLogarithmOperation: public MathBaseOperation {
public:
	MathLogarithmOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathMinimumOperation: public MathBaseOperation {
public:
	MathMinimumOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathMaximumOperation: public MathBaseOperation {
public:
	MathMaximumOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathRoundOperation: public MathBaseOperation {
public:
	MathRoundOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathLessThanOperation: public MathBaseOperation {
public:
	MathLessThanOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};
class MathGreaterThanOperation: public MathBaseOperation {
public:
	MathGreaterThanOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
};

#endif
