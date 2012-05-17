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

#ifndef _COM_LensGhostOperation_h
#define _COM_LensGhostOperation_h
#include "COM_NodeOperation.h"
#include "DNA_lamp_types.h"
#include "DNA_object_types.h"
#include "DNA_camera_types.h"

class LensGhostProjectionOperation : public NodeOperation {
protected:
	Object* lampObject;
	Lamp* lamp;
	Object* cameraObject;

	void* system;
	float visualLampPosition[3];
	CompositorQuality quality;
	int step;
	SocketReader * bokehReader;

public:
	LensGhostProjectionOperation();

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
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

	void setLampObject(Object* lampObject) {this->lampObject = lampObject;}
	void setCameraObject(Object* cameraObject) {this->cameraObject = cameraObject;}

	void setQuality(CompositorQuality quality) {this->quality = quality;}
};

class LensGhostOperation : public LensGhostProjectionOperation {
public:
	LensGhostOperation();

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	void deinitializeTileData(rcti *rect, MemoryBuffer **memoryBuffers, void *data);
	/**
	  * the inner loop of this program
	  */
	void executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void * data);
	/**
	  * Initialize the execution
	  */
	void initExecution();
};
#endif
