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

#ifndef __COM_CONVERTCOLORPROFILEOPERATION_H__
#define __COM_CONVERTCOLORPROFILEOPERATION_H__
#include "COM_NodeOperation.h"


/**
 * this program converts an input color to an output value.
 * it assumes we are in sRGB color space.
 */
class ConvertColorProfileOperation : public NodeOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputOperation;

	/**
	 * @brief color profile where to convert from
	 */
	int m_fromProfile;

	/**
	 * @brief color profile where to convert to
	 */
	int m_toProfile;

	/**
	 * @brief is color predivided
	 */
	bool m_predivided;
public:
	/**
	 * Default constructor
	 */
	ConvertColorProfileOperation();

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

	void setFromColorProfile(int colorProfile) { this->m_fromProfile = colorProfile; }
	void setToColorProfile(int colorProfile) { this->m_toProfile = colorProfile; }
	void setPredivided(bool predivided) { this->m_predivided = predivided; }
};
#endif
