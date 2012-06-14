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

#ifndef _COM_SingleThreadedNodeOperation_h
#define _COM_SingleThreadedNodeOperation_h
#include "COM_NodeOperation.h"

class SingleThreadedNodeOperation : public NodeOperation {
private:
	MemoryBuffer *cachedInstance;
	
protected:
	inline bool isCached() {
		return cachedInstance != NULL;
	}

public:
	SingleThreadedNodeOperation();
	
	/**
	 * the inner loop of this program
	 */
	void executePixel(float *color, int x, int y, MemoryBuffer * inputBuffers[], void *data);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();

	void *initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);

	virtual MemoryBuffer *createMemoryBuffer(rcti *rect, MemoryBuffer **memoryBuffers) = 0;
	
	int isSingleThreaded() { return true; }
};
#endif
