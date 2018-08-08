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

#ifndef __COM_PIXELATEOPERATION_H__
#define __COM_PIXELATEOPERATION_H__

#include "COM_NodeOperation.h"

/**
 * @brief Pixelate operation
 *
 * The Tile compositor is by default sub-pixel accurate.
 * For some setups you don want this.
 * This operation will remove the sub-pixel accuracy
 */
class PixelateOperation : public NodeOperation {
private:
	/**
	 * @brief cached reference to the input operation
	 */
	SocketReader *m_inputOperation;
public:
	/**
	 * @brief PixelateOperation
	 * @param dataType the datatype to create this operator for (saves datatype conversions)
	 */
	PixelateOperation(DataType dataType);

	/**
	 * @brief initialization of the execution
	 */
	void initExecution();

	/**
	 * @brief de-initialization of the execution
	 */
	void deinitExecution();

	/**
	 * @brief executePixel
	 * @param output result
	 * @param x x-coordinate
	 * @param y y-coordinate
	 * @param sampler sampler
	 */
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
};

#endif
