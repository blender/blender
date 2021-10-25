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

#ifndef _COM_CalculateStandardDeviationOperation_h
#define _COM_CalculateStandardDeviationOperation_h
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"
#include "COM_CalculateMeanOperation.h"
/**
 * @brief base class of CalculateStandardDeviation, implementing the simple CalculateStandardDeviation
 * @ingroup operation
 */
class CalculateStandardDeviationOperation : public CalculateMeanOperation {
protected:
	float m_standardDeviation;

public:
	CalculateStandardDeviationOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);

	void *initializeTileData(rcti *rect);

};
#endif
