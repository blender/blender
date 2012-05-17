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


#ifndef _COM_MemoryProxy_h
#define _COM_MemoryProxy_h
#include "COM_ExecutionGroup.h"
#include "COM_MemoryManagerState.h"

class ExecutionGroup;

/**
  * @brief A MemoryProxy is a unique identifier for a memory buffer.
  * A single MemoryProxy is used among all chunks of the same buffer,
  * the MemoryBuffer only stores the data of a single chunk.
  * @ingroup Memory
  */
class MemoryProxy {
private:
	/**
	  * @brief reference to the ouput operation of the executiongroup
	  */
	WriteBufferOperation *writeBufferOperation;
	
	/**
	  * @brief reference to the executor. the Execution group that can fill a chunk
	  */
	ExecutionGroup *executor;
	
	/**
	  * @brief data of the different chunks.
	  * @note state is part of this class due to optimization in the MemoryManager
	  */
	MemoryManagerState * state;
	
	/**
	  * @brief datatype of this MemoryProxy
	  */
	DataType datatype;
	
	/**
	  * @brief channel information of this buffer
	  */
	ChannelInfo channelInfo[COM_NUMBER_OF_CHANNELS];
public:
	MemoryProxy();
	~MemoryProxy();
	
	/**
	  * @brief set the ExecutionGroup that can be scheduled to calculate a certain chunk.
	  * @param group the ExecutionGroup to set
	  */
	void setExecutor(ExecutionGroup *executor) {this->executor = executor;}
	
	/**
	  * @brief get the ExecutionGroup that can be scheduled to calculate a certain chunk.
	  */
	ExecutionGroup* getExecutor() {return this->executor;}
	
	/**
	  * @brief set the WriteBufferOperation that is responsible for writing to this MemoryProxy
	  * @param operation
	  */
	void setWriteBufferOperation(WriteBufferOperation* operation) {this->writeBufferOperation = operation;}
	
	/**
	  * @brief get the WriteBufferOperation that is responsible for writing to this MemoryProxy
	  * @return WriteBufferOperation
	  */
	WriteBufferOperation* getWriteBufferOperation() {return this->writeBufferOperation;}
	
	/**
	  * @brief set the memorymanager state of this MemoryProxy, this is set from the MemoryManager
	  * @param state the state to set
	  */
	void setState(MemoryManagerState *state) {this->state = state;}
	
	/**
	  * @brief get the state of this MemoryProxy
	  * @return MemoryManagerState reference to the state of this MemoryProxy.
	  */
	MemoryManagerState* getState() {return this->state;}
};

#endif
