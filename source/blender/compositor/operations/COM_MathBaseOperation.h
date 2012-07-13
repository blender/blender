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
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class MathBaseOperation : public NodeOperation {
protected:
	/**
	 * Prefetched reference to the inputProgram
	 */
	SocketReader *m_inputValue1Operation;
	SocketReader *m_inputValue2Operation;

	bool m_useClamp;

protected:
	/**
	 * Default constructor
	 */
	MathBaseOperation();

	void clampIfNeeded(float *color);
public:
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, float x, float y, PixelSampler sampler) = 0;
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	/**
	 * Determine resolution
	 */
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

	void setUseClamp(bool value) { this->m_useClamp = value; }
};

class MathAddOperation : public MathBaseOperation {
public:
	MathAddOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathSubtractOperation : public MathBaseOperation {
public:
	MathSubtractOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathMultiplyOperation : public MathBaseOperation {
public:
	MathMultiplyOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathDivideOperation : public MathBaseOperation {
public:
	MathDivideOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathSineOperation : public MathBaseOperation {
public:
	MathSineOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathCosineOperation : public MathBaseOperation {
public:
	MathCosineOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathTangentOperation : public MathBaseOperation {
public:
	MathTangentOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};

class MathArcSineOperation : public MathBaseOperation {
public:
	MathArcSineOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathArcCosineOperation : public MathBaseOperation {
public:
	MathArcCosineOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathArcTangentOperation : public MathBaseOperation {
public:
	MathArcTangentOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathPowerOperation : public MathBaseOperation {
public:
	MathPowerOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathLogarithmOperation : public MathBaseOperation {
public:
	MathLogarithmOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathMinimumOperation : public MathBaseOperation {
public:
	MathMinimumOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathMaximumOperation : public MathBaseOperation {
public:
	MathMaximumOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathRoundOperation : public MathBaseOperation {
public:
	MathRoundOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathLessThanOperation : public MathBaseOperation {
public:
	MathLessThanOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};
class MathGreaterThanOperation : public MathBaseOperation {
public:
	MathGreaterThanOperation() : MathBaseOperation() {}
	void executePixel(float *outputValue, float x, float y, PixelSampler sampler);
};

#endif
