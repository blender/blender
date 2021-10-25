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

#ifndef __COM_GAUSSIANBOKEHBLUROPERATION_H__
#define __COM_GAUSSIANBOKEHBLUROPERATION_H__
#include "COM_NodeOperation.h"
#include "COM_BlurBaseOperation.h"
#include "COM_QualityStepHelper.h"

class GaussianBokehBlurOperation : public BlurBaseOperation {
private:
	float *m_gausstab;
	int m_radx, m_rady;
	void updateGauss();

public:
	GaussianBokehBlurOperation();
	void initExecution();
	void *initializeTileData(rcti *rect);
	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};

class GaussianBlurReferenceOperation : public BlurBaseOperation {
private:
	float **m_maintabs;
	
	void updateGauss();
	int m_filtersizex;
	int m_filtersizey;
	float m_radx;
	float m_rady;

public:
	GaussianBlurReferenceOperation();
	void initExecution();
	void *initializeTileData(rcti *rect);
	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};

#endif
