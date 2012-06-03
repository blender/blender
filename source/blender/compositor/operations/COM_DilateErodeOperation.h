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

#ifndef _COM_DilateErodeOperation_h
#define _COM_DilateErodeOperation_h
#include "COM_NodeOperation.h"


class DilateErodeDistanceOperation : public NodeOperation {
private:
	/**
	  * Cached reference to the inputProgram
	  */
	SocketReader * inputProgram;
	
	float distance;
	float _switch;
	float inset;
	
	/**
	  * determines the area of interest to track pixels
	  * keep this one as small as possible for speed gain.
	  */
	int scope;
public:
	DilateErodeDistanceOperation();
	
	/**
	  * the inner loop of this program
	  */
	void executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data);
	
	/**
	  * Initialize the execution
	  */
	void initExecution();
	
	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	/**
	  * Deinitialize the execution
	  */
	void deinitExecution();
	
	void setDistance(float distance) {this->distance = distance;}
	void setSwitch(float sw) {this->_switch = sw;}
	void setInset(float inset) {this->inset = inset;}
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);

};

class DilateDistanceOperation : public NodeOperation {
private:
	/**
	  * Cached reference to the inputProgram
	  */
	SocketReader * inputProgram;
protected:
	float distance;
	int scope;
public:
	DilateDistanceOperation();
	
	/**
	  * the inner loop of this program
	  */
	void executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data);
	
	/**
	  * Initialize the execution
	  */
	void initExecution();
	
	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	/**
	  * Deinitialize the execution
	  */
	void deinitExecution();
	
	void setDistance(float distance) {this->distance = distance;}
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};
class ErodeDistanceOperation : public DilateDistanceOperation {
public:
	ErodeDistanceOperation();
	
	/**
	  * the inner loop of this program
	  */
	void executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data);
};

class DilateStepOperation : public NodeOperation {
protected:
	/**
	  * Cached reference to the inputProgram
	  */
	SocketReader * inputProgram;
	
	int iterations;
	
	float *cached_buffer;
public:
	DilateStepOperation();
	
	/**
	  * the inner loop of this program
	  */
	void executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data);
	
	/**
	  * Initialize the execution
	  */
	void initExecution();
	
	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	/**
	  * Deinitialize the execution
	  */
	void deinitExecution();
	
	void setIterations(int iterations) {this->iterations = iterations;}
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};

class ErodeStepOperation : public DilateStepOperation {
public:
	ErodeStepOperation();
	
	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
};

#endif
