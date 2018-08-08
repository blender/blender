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

#ifndef __COM_DOUBLEEDGEMASKOPERATION_H__
#define __COM_DOUBLEEDGEMASKOPERATION_H__
#include "COM_NodeOperation.h"


class DoubleEdgeMaskOperation : public NodeOperation {
private:
	/**
	 * Cached reference to the inputProgram
	 */
	SocketReader *m_inputOuterMask;
	SocketReader *m_inputInnerMask;
	bool m_adjecentOnly;
	bool m_keepInside;
	float *m_cachedInstance;
public:
	DoubleEdgeMaskOperation();

	void doDoubleEdgeMask(float *inner, float *outer, float *res);
	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);

	/**
	 * Initialize the execution
	 */
	void initExecution();

	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	void *initializeTileData(rcti *rect);

	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	void setAdjecentOnly(bool adjecentOnly) { this->m_adjecentOnly = adjecentOnly; }
	void setKeepInside(bool keepInside) { this->m_keepInside = keepInside; }
};
#endif
