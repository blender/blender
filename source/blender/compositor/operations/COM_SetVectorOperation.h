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

#ifndef __COM_SETVECTOROPERATION_H__
#define __COM_SETVECTOROPERATION_H__
#include "COM_NodeOperation.h"


/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class SetVectorOperation : public NodeOperation {
private:
	float m_x;
	float m_y;
	float m_z;
	float m_w;

public:
	/**
	 * Default constructor
	 */
	SetVectorOperation();

	float getX() { return this->m_x; }
	void setX(float value) { this->m_x = value; }
	float getY() { return this->m_y; }
	void setY(float value) { this->m_y = value; }
	float getZ() { return this->m_z; }
	void setZ(float value) { this->m_z = value; }
	float getW() { return this->m_w; }
	void setW(float value) { this->m_w = value; }

	/**
	 * the inner loop of this program
	 */
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);

	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	bool isSetOperation() const { return true; }

	void setVector(const float vector[3]) {
		setX(vector[0]);
		setY(vector[1]);
		setZ(vector[2]);
	}
};
#endif
