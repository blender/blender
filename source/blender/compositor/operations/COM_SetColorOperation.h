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

#ifndef __COM_SETCOLOROPERATION_H__
#define __COM_SETCOLOROPERATION_H__
#include "COM_NodeOperation.h"


/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class SetColorOperation : public NodeOperation {
private:
	float m_color[4];

public:
	/**
	 * Default constructor
	 */
	SetColorOperation();

	float getChannel1() { return this->m_color[0]; }
	void setChannel1(float value) { this->m_color[0] = value; }
	float getChannel2() { return this->m_color[1]; }
	void setChannel2(float value) { this->m_color[1] = value; }
	float getChannel3() { return this->m_color[2]; }
	void setChannel3(float value) { this->m_color[2] = value; }
	float getChannel4() { return this->m_color[3]; }
	void setChannel4(const float value) { this->m_color[3] = value; }
	void setChannels(const float value[4])
	{
		copy_v4_v4(this->m_color, value);
	}

	/**
	 * the inner loop of this program
	 */
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	bool isSetOperation() const { return true; }

};
#endif
