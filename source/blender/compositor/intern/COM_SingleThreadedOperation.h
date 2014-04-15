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

#ifndef _COM_SingleThreadedOperation_h
#define _COM_SingleThreadedOperation_h
#include "COM_NodeOperation.h"

class SingleThreadedOperation : public NodeOperation {
private:
	MemoryBuffer *m_cachedInstance;
	
protected:
	inline bool isCached() {
		return this->m_cachedInstance != NULL;
	}

public:
	SingleThreadedOperation();
	
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

	virtual MemoryBuffer *createMemoryBuffer(rcti *rect) = 0;
	
	int isSingleThreaded() { return true; }
};
#endif
