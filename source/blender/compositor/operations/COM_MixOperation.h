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

#ifndef __COM_MIXOPERATION_H__
#define __COM_MIXOPERATION_H__
#include "COM_NodeOperation.h"


/**
 * All this programs converts an input color to an output value.
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

	inline void clampIfNeeded(float color[4])
	{
		if (m_useClamp) {
			CLAMP(color[0], 0.0f, 1.0f);
			CLAMP(color[1], 0.0f, 1.0f);
			CLAMP(color[2], 0.0f, 1.0f);
			CLAMP(color[3], 0.0f, 1.0f);
		}
	}

public:
	/**
	 * Default constructor
	 */
	MixBaseOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

	/**
	 * Initialize the execution
	 */
	void initExecution();

	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);


	void setUseValueAlphaMultiply(const bool value) { this->m_valueAlphaMultiply = value; }
	inline bool useValueAlphaMultiply() { return this->m_valueAlphaMultiply; }
	void setUseClamp(bool value) { this->m_useClamp = value; }
};

class MixAddOperation : public MixBaseOperation {
public:
	MixAddOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixBlendOperation : public MixBaseOperation {
public:
	MixBlendOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixBurnOperation : public MixBaseOperation {
public:
	MixBurnOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixColorOperation : public MixBaseOperation {
public:
	MixColorOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixDarkenOperation : public MixBaseOperation {
public:
	MixDarkenOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixDifferenceOperation : public MixBaseOperation {
public:
	MixDifferenceOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixDivideOperation : public MixBaseOperation {
public:
	MixDivideOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixDodgeOperation : public MixBaseOperation {
public:
	MixDodgeOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixGlareOperation : public MixBaseOperation {
public:
	MixGlareOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixHueOperation : public MixBaseOperation {
public:
	MixHueOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixLightenOperation : public MixBaseOperation {
public:
	MixLightenOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixLinearLightOperation : public MixBaseOperation {
public:
	MixLinearLightOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixMultiplyOperation : public MixBaseOperation {
public:
	MixMultiplyOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixOverlayOperation : public MixBaseOperation {
public:
	MixOverlayOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixSaturationOperation : public MixBaseOperation {
public:
	MixSaturationOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixScreenOperation : public MixBaseOperation {
public:
	MixScreenOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixSoftLightOperation : public MixBaseOperation {
public:
	MixSoftLightOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixSubtractOperation : public MixBaseOperation {
public:
	MixSubtractOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

class MixValueOperation : public MixBaseOperation {
public:
	MixValueOperation();
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

#endif
