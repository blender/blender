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

#ifndef _COM_MemoryManager_h_
#define _COM_MemoryManager_h_

#include "COM_MemoryBuffer.h"
#include "COM_MemoryProxy.h"
#include "COM_ExecutionGroup.h"
#include "COM_MemoryManagerState.h"

/**
  * @page memorymanager The Memory Manager
  * The compositor has its own MemoryManager. The goal of the MemoryManager is to manage the memory allocated by chunks.
  * During execution new chunks will be created [MemoryManager.allocateMemoryBuffer] When calculation is finished the MemoryBuffer will get the state [MemoryBufferState.COM_MB_AVAILABLE].
  * From now on other ExecutionGroup and NodeOperations may read from the MemoryBuffer.
  * The MemoryManager also has the capability to save MemoryBuffer's to disk in order to free some memory.
  *
  * @section S_MEM Memory manager
  * The memory manager synchronize and optimize data across devices.
  * Only one NodeOperation running on a device is able to write to a MemoryBuffer. This MemoryBuffer is only allocated on the main-device memory (CPU).
  * The MemoryBuffer.state will be [MemoryBufferState.COM_MB_ALLOCATED]. As soon as the chunk has been executed the state changes to [MemoryBufferState.COM_MB_AVAILABLE]. This MemoryBuffer can now be used as inputBuffer of ExecutionGroup's.
  * When needed the MemoryBuffer will be stored to a file. This will save memory that can be used by other tiles.
  * @subsection S_MEM_1 Step one
  * When a chunk of an ExecutionGroup is being executed by a device, the MemoryBuffer is allocated on the CPU.
  * <pre>
  * Allocation of the output MemoryBuffer
  *  +----------------------------------------+
  *  | Main device (CPU)                      |
  *  | +----------------+   +--------------+  |
  *  | | ExecutionGroup |   | MemoryBuffer |  |
  *  | |                |   | Chunk a      |  |
  *  | +----------------+   +--------------+  |
  *  |                                        |
  *  +----------------------------------------+
  * </pre>
  * @see MemoryManager.allocateMemoryBuffer
  *
  * @subsection S_MEM_2 Step two
  * The Device will execute the ExecutionGroup. This differs per type of Device. CPUDevice will call the NodeOperation.executeRegion method of the outputnode of the ExecutionGroup.
  * The [NodeOperation.executeRegion] writes the result to the allocated MemoryBuffer. When finished the state of the MemoryBuffer will be set to [MemoryBufferState.COM_MB_AVAILABLE].
  * <pre>
  * Execute a chunk and store result to the MemoryBuffer
  *  +----------------------------------------+
  *  | Main device (CPU)                      |
  *  | +----------------+   +--------------+  |
  *  | | ExecutionGroup |   | MemoryBuffer |  |
  *  | |                |   | Chunk a      |  |
  *  | +----------------+   +--------------+  |
  *  |         |                 ^            |
  *  | +----------------+        |            |
  *  | | NodeOperation  |--------+            |
  *  | |                |     Write result    |
  *  | +----------------+                     |
  *  |                                        |
  *  +----------------------------------------+
  * </pre>
  * @subsection S_MEM_3 Step 3
  * Other Chunks that depend on the MemoryBuffer can now use it.
  * When a MemoryBuffer is being used its number of users are increased. When a 'user' is finished the number of users are decreased, If a MemoryBuffer has no users, the system can decide to store the data to disk and free some memory.
  * @see MemoryBuffer.numberOfUsers
  * @see MemoryBuffer.saveToDisk
  *
  * @subsection S_MEM_CON Temporarily MemoryBuffers
  * Nodes like blur nodes can depend on multiple MemoryBuffer of the same MemoryProxy. These multiple buffers will be consolidated temporarily to a new MemoryBuffer.
  * When execution is finished this temporarily memory buffer is deallicated.
  * <pre>
  *  Original MemoryBuffer's  Temporarily
  *  +-------+ +-------+      MemoryBuffer
  *  | MB A  | | MB B  |      +-------+-------+
  *  +-------+ +-------+      | MB A  | MB B  |
  *                       ==> +-------+-------+
  *  +-------+ +-------+      | MB C  | MB D  |
  *  | MB C  | | MB D  |      +-------+-------+
  *  +-------+ +-------+
  * </pre>
  * @see ExecutionGroup.constructConsolidatedMemoryBuffer constructs the temporarily MemoryBuffer
  * @see MemoryBuffer.state state is MemoryManagerState.COM_MB_TEMPORARILY
  * @see ExecutionGroup.finalizeChunkExecution deallocate the temporarily MemoryBuffer
  * @note this MemoryBuffer is not managed by the MemoryManager
  */

/**
  * @brief the memory manager for the compostor
  * @ingroup Memory
  */
class MemoryManager {
private:
	/**
	  * @brief retrieve the state of a certain MemoryProxy;
	  * @param memoryProxy the MemoryProxy to retrieve the state from
	  */
	static MemoryManagerState* getState(MemoryProxy* memoryProxy);
public:
	/**
	  * @brief allocate a memory buffer
	  * @param memoryProxy the MemoryProxy to get a chunk from
	   * @param chunkNumber number of the chunk to receive
	   * @param rect size + position of the chunk
	  */
	static MemoryBuffer* allocateMemoryBuffer(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti* rect);

	/**
	  * @brief get a memory buffer
	  * @param memoryProxy the MemoryProxy to get a chunk from
	  * @param chunkNumber number of the chunk to receive
	  * @param addUser must we add a user to the chunk.
	  */
	static MemoryBuffer* getMemoryBuffer(MemoryProxy *memoryProxy, unsigned int chunkNumber);

	/**
	  * @brief add a MemoryProxy to the scope of the memory manager
	  * @param memoryProxy the MemoryProxy to add
	  */
	static void addMemoryProxy(MemoryProxy *memoryProxy);

	/**
	  * @brief clear the memory manager
	  */
	static void clear();

	/**
	  * @brief initialize the memory manager.
	  */
	static void initialize();
};
#endif
