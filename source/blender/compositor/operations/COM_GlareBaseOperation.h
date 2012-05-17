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

#ifndef _COM_GlareBaseOperation_h
#define _COM_GlareBaseOperation_h
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"

class GlareBaseOperation : public NodeOperation {
private:
	/**
	  * @brief Cached reference to the inputProgram
	  */
	SocketReader * inputProgram;
	
	/**
	  * @brief settings of the glare node.
	  */
	NodeGlare * settings;
	
	float* cachedInstance;

public:
	
	/**
	  * the inner loop of this program
	  */
	void executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data);
	
	/**
	  * Initialize the execution
	  */
	void initExecution();
	
	/**
	  * Deinitialize the execution
	  */
	void deinitExecution();

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);

	void setGlareSettings(NodeGlare * settings) {this->settings = settings;}
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
protected:
	GlareBaseOperation();
	
	virtual void generateGlare(float* data, MemoryBuffer* inputTile, NodeGlare* settings) = 0;
	
	
};
#endif
