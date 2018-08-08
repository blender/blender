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

#ifndef __COM_COLORCURVEOPERATION_H__
#define __COM_COLORCURVEOPERATION_H__
#include "COM_NodeOperation.h"
#include "DNA_color_types.h"
#include "COM_CurveBaseOperation.h"

class ColorCurveOperation : public CurveBaseOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputFacProgram;
	SocketReader *m_inputImageProgram;
	SocketReader *m_inputBlackProgram;
	SocketReader *m_inputWhiteProgram;
public:
	ColorCurveOperation();

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
};

class ConstantLevelColorCurveOperation : public CurveBaseOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputFacProgram;
	SocketReader *m_inputImageProgram;
	float m_black[3];
	float m_white[3];

public:
	ConstantLevelColorCurveOperation();

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

	void setBlackLevel(float black[3]) { copy_v3_v3(this->m_black, black); }
	void setWhiteLevel(float white[3]) { copy_v3_v3(this->m_white, white); }
};

#endif
