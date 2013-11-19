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

#ifndef _COM_GlareScaleOperation_h
#define _COM_GlareScaleOperation_h
#include "COM_NodeOperation.h"
#include "DNA_lamp_types.h"

class GlareThresholdOperation : public NodeOperation {
private:
	/**
	 * @brief Cached reference to the inputProgram
	 */
	SocketReader *m_inputProgram;

	/**
	 * @brief settings of the glare node.
	 */
	NodeGlare *m_settings;
public:
	GlareThresholdOperation();

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

	void setGlareSettings(NodeGlare *settings) {
		this->m_settings = settings;
	}
	
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
};
#endif
