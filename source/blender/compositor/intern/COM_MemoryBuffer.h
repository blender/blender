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

class MemoryBuffer;

#ifndef _COM_MemoryBuffer_h_
#define _COM_MemoryBuffer_h_

#include "COM_ExecutionGroup.h"
#include "BLI_rect.h"
#include "COM_MemoryProxy.h"
extern "C" {
	#include "BLI_threads.h"
}
#include <vector>

/**
 * @brief state of a memory buffer
 * @ingroup Memory
 */
typedef enum MemoryBufferState {
	/** @brief memory has been allocated on creator device and CPU machine, but kernel has not been executed */
	COM_MB_ALLOCATED = 1,
	/** @brief memory is available for use, content has been created */
	COM_MB_AVAILABLE = 2,
	/** @brief chunk is consolidated from other chunks. special state.*/
	COM_MB_TEMPORARILY = 6
} MemoryBufferState;

class MemoryProxy;

/**
 * @brief a MemoryBuffer contains access to the data of a chunk
 */
class MemoryBuffer {
private:
	/**
	 * @brief proxy of the memory (same for all chunks in the same buffer)
	 */
	MemoryProxy *m_memoryProxy;
	
	/**
	 * @brief the type of buffer COM_DT_VALUE, COM_DT_VECTOR, COM_DT_COLOR
	 */
	DataType m_datatype;
	
	
	/**
	 * @brief region of this buffer inside reative to the MemoryProxy
	 */
	rcti m_rect;
	
	/**
	 * brief refers to the chunknumber within the executiongroup where related to the MemoryProxy
	 * @see memoryProxy
	 */
	unsigned int m_chunkNumber;
	
	/**
	 * @brief width of the chunk
	 */
	unsigned int m_chunkWidth;
	
	/**
	 * @brief state of the buffer
	 */
	MemoryBufferState m_state;
	
	/**
	 * @brief the actual float buffer/data
	 */
	float *m_buffer;

public:
	/**
	 * @brief construct new MemoryBuffer for a chunk
	 */
	MemoryBuffer(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect);
	
	/**
	 * @brief construct new temporarily MemoryBuffer for an area
	 */
	MemoryBuffer(MemoryProxy *memoryProxy, rcti *rect);
	
	/**
	 * @brief destructor
	 */
	~MemoryBuffer();
	
	/**
	 * @brief read the ChunkNumber of this MemoryBuffer
	 */
	unsigned int getChunkNumber() { return this->m_chunkNumber; }
	
	/**
	 * @brief get the data of this MemoryBuffer
	 * @note buffer should already be available in memory
	 */
	float *getBuffer() { return this->m_buffer; }
	
	/**
	 * @brief after execution the state will be set to available by calling this method
	 */
	void setCreatedState() {
		this->m_state = COM_MB_AVAILABLE;
	}
	
	void read(float result[4], int x, int y);
	void writePixel(int x, int y, const float color[4]);
	void addPixel(int x, int y, const float color[4]);
	void readCubic(float result[4], float x, float y);
	void readEWA(float result[4], float fx, float fy, float dx, float dy);
	
	/**
	 * @brief is this MemoryBuffer a temporarily buffer (based on an area, not on a chunk)
	 */
	inline const bool isTemporarily() const { return this->m_state == COM_MB_TEMPORARILY; }
	
	/**
	 * @brief add the content from otherBuffer to this MemoryBuffer
	 * @param otherBuffer source buffer
	 */
	void copyContentFrom(MemoryBuffer *otherBuffer);
	
	/**
	 * @brief get the rect of this MemoryBuffer
	 */
	rcti *getRect() { return &this->m_rect; }
	
	/**
	 * @brief get the width of this MemoryBuffer
	 */
	int getWidth() const;
	
	/**
	 * @brief get the height of this MemoryBuffer
	 */
	int getHeight() const;
	
	/**
	 * @brief clear the buffer. Make all pixels black transparant.
	 */
	void clear();
	
	MemoryBuffer *duplicate();
	
	float *convertToValueBuffer();
private:
	unsigned int determineBufferSize();

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:MemoryBuffer")
#endif
};

#endif
