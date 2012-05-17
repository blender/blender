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

#ifndef _COM_BokehImageOperation_h
#define _COM_BokehImageOperation_h
#include "COM_NodeOperation.h"


class BokehImageOperation : public NodeOperation {
private:
	NodeBokehImage *data;

	float center[2];
	float centerX;
	float centerY;
	float inverseRounding;
	float circularDistance;
	float flapRad;
	float flapRadAdd;
	
	bool deleteData;

	void detemineStartPointOfFlap(float r[2], int flapNumber, float distance);
	float isInsideBokeh(float distance, float x, float y);
public:
	BokehImageOperation();

	/**
	  * the inner loop of this program
	  */
	void executePixel(float* color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]);
	
	/**
	  * Initialize the execution
	  */
	void initExecution();
	
	/**
	  * Deinitialize the execution
	  */
	void deinitExecution();
	
	void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

	void setData(NodeBokehImage *data) {this->data = data;}
	void deleteDataOnFinish() {this->deleteData = true;}
};
#endif
