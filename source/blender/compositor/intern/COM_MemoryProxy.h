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

class MemoryProxy;


#ifndef __COM_MEMORYPROXY_H__
#define __COM_MEMORYPROXY_H__
#include "COM_ExecutionGroup.h"

class ExecutionGroup;
class WriteBufferOperation;

/**
 * \brief A MemoryProxy is a unique identifier for a memory buffer.
 * A single MemoryProxy is used among all chunks of the same buffer,
 * the MemoryBuffer only stores the data of a single chunk.
 * \ingroup Memory
 */
class MemoryProxy {
private:
	/**
	 * \brief reference to the output operation of the executiongroup
	 */
	WriteBufferOperation *m_writeBufferOperation;

	/**
	 * \brief reference to the executor. the Execution group that can fill a chunk
	 */
	ExecutionGroup *m_executor;

	/**
	 * \brief datatype of this MemoryProxy
	 */
	/* DataType m_datatype; */ /* UNUSED */

	/**
	 * \brief channel information of this buffer
	 */
	/* ChannelInfo m_channelInfo[COM_NUMBER_OF_CHANNELS]; */ /* UNUSED */

	/**
	 * \brief the allocated memory
	 */
	MemoryBuffer *m_buffer;

	/**
	 * \brief datatype of this MemoryProxy
	 */
	DataType m_datatype;

public:
	MemoryProxy(DataType type);

	/**
	 * \brief set the ExecutionGroup that can be scheduled to calculate a certain chunk.
	 * \param group the ExecutionGroup to set
	 */
	void setExecutor(ExecutionGroup *executor) { this->m_executor = executor; }

	/**
	 * \brief get the ExecutionGroup that can be scheduled to calculate a certain chunk.
	 */
	ExecutionGroup *getExecutor() { return this->m_executor; }

	/**
	 * \brief set the WriteBufferOperation that is responsible for writing to this MemoryProxy
	 * \param operation
	 */
	void setWriteBufferOperation(WriteBufferOperation *operation) { this->m_writeBufferOperation = operation; }

	/**
	 * \brief get the WriteBufferOperation that is responsible for writing to this MemoryProxy
	 * \return WriteBufferOperation
	 */
	WriteBufferOperation *getWriteBufferOperation() { return this->m_writeBufferOperation; }

	/**
	 * \brief allocate memory of size width x height
	 */
	void allocate(unsigned int width, unsigned int height);

	/**
	 * \brief free the allocated memory
	 */
	void free();

	/**
	 * \brief get the allocated memory
	 */
	inline MemoryBuffer *getBuffer() { return this->m_buffer; }

	inline DataType getDataType() { return this->m_datatype; }

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:MemoryProxy")
#endif
};

#endif
