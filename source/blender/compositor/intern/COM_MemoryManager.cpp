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

#include "COM_MemoryManager.h"
#include "BLI_threads.h"
#include <stdio.h>
#include "COM_defines.h"

vector<MemoryBuffer*> buffers;

ThreadMutex mutex;

MemoryBuffer* MemoryManager::allocateMemoryBuffer(MemoryProxy *id, unsigned int chunkNumber, rcti *rect) {
	MemoryBuffer *result = new MemoryBuffer(id, chunkNumber, rect);
	MemoryManagerState * state = MemoryManager::getState(id);
	state->addMemoryBuffer(result);
	BLI_mutex_lock(&mutex);
	buffers.push_back(result);
	BLI_mutex_unlock(&mutex);
	return result;
}

void MemoryManager::addMemoryProxy(MemoryProxy *memoryProxy) {
	MemoryManagerState * state = MemoryManager::getState(memoryProxy);
	if (!state) {
		state = new MemoryManagerState(memoryProxy);
		memoryProxy->setState(state);
	}
}
MemoryBuffer* MemoryManager::getMemoryBuffer(MemoryProxy *id, unsigned int chunkNumber){
	MemoryManagerState * state = MemoryManager::getState(id);
	if (!state) {
		return NULL;
	}
	MemoryBuffer* buffer = state->getMemoryBuffer(chunkNumber);
	if (!buffer) return NULL;
	return buffer;
}

MemoryManagerState* MemoryManager::getState(MemoryProxy* memoryProxy) {
	return memoryProxy->getState();
}
void MemoryManager::initialize() {
	BLI_mutex_init(&mutex);
}
void MemoryManager::clear() {
	buffers.clear();
	BLI_mutex_end(&mutex);
}
