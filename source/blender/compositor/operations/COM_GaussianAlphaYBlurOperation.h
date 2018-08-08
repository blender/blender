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
 *		Campbell Barton
 */

#ifndef __COM_GAUSSIANALPHAYBLUROPERATION_H__
#define __COM_GAUSSIANALPHAYBLUROPERATION_H__
#include "COM_NodeOperation.h"
#include "COM_BlurBaseOperation.h"

class GaussianAlphaYBlurOperation : public BlurBaseOperation {
private:
	float *m_gausstab;
	float *m_distbuf_inv;
	bool m_do_subtract;
	int m_falloff;
	int m_filtersize;
	void updateGauss();
public:
	GaussianAlphaYBlurOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);

	/**
	 * @brief initialize the execution
	 */
	void initExecution();

	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	void *initializeTileData(rcti *rect);
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	/**
	 * Set subtract for Dilate/Erode functionality
	 */
	void setSubtract(bool subtract) { this->m_do_subtract = subtract; }
	void setFalloff(int falloff) { this->m_falloff = falloff; }
};
#endif
