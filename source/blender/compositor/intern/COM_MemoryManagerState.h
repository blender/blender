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

class MemoryManagerState;

#ifndef _COM_MemoryManagerState_h_
#define _COM_MemoryManagerState_h_

#include "COM_MemoryProxy.h"
#include "COM_MemoryBuffer.h"
#include <vector>
extern "C" {
	#include "BLI_threads.h"
}

/**
  * @brief State of a MemoryProxy in the MemoryManager.
  * @ingroup Memory
  */
class MemoryManagerState {
private:
	/**
	  * @brief reference to the MemoryProxy of this state
	  */
	MemoryProxy *memoryProxy;
	
	/**
	  * @brief list of all chunkbuffers
	  */
	MemoryBuffer** chunkBuffers;
	
	/**
	  * @brief size of the chunkBuffers
	  */
	unsigned int currentSize;
	
	/**
	  * @brief lock to this memory for multithreading
	  */
	ThreadMutex mutex;
public:
	/**
	  * @brief creates a new MemoryManagerState for a certain MemoryProxy.
	  */
	MemoryManagerState(MemoryProxy * memoryProxy);
	/**
	  * @brief destructor
	  */
	~MemoryManagerState();
	
	/**
	  * @brief get the reference to the MemoryProxy this state belongs to.
	  */
	MemoryProxy *getMemoryProxy();
	
	/**
	  * @brief add a new memorybuffer to the state
	  */
	void addMemoryBuffer(MemoryBuffer* buffer);
	
	/**
	  * @brief get the MemoryBuffer assiciated to a chunk.
	  * @param chunkNumber the chunknumber
	  */
	MemoryBuffer* getMemoryBuffer(unsigned int chunkNumber);
};

#endif
