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

#include "COM_MemoryManagerState.h"

MemoryManagerState::MemoryManagerState(MemoryProxy *memoryProxy) {
	this->memoryProxy = memoryProxy;
	this->currentSize = 0;
	this->chunkBuffers = NULL;
	BLI_mutex_init(&this->mutex);
}

MemoryProxy * MemoryManagerState::getMemoryProxy() {
	return this->memoryProxy;
}

MemoryManagerState::~MemoryManagerState() {
	this->memoryProxy = NULL;
	unsigned int index;
	for (index = 0 ; index < this->currentSize; index ++){
		MemoryBuffer* buffer = this->chunkBuffers[index];
		if (buffer) {
			delete buffer;
		}
	}
	delete this->chunkBuffers;
	BLI_mutex_end(&this->mutex);
}

void MemoryManagerState::addMemoryBuffer(MemoryBuffer *buffer) {
	BLI_mutex_lock(&this->mutex);
	unsigned int chunkNumber = buffer->getChunkNumber();
	unsigned int index;
	while (this->currentSize <= chunkNumber) {
		unsigned int newSize = this->currentSize + 1000;
		MemoryBuffer** newbuffer = new MemoryBuffer*[newSize];
		MemoryBuffer** oldbuffer = this->chunkBuffers;
	
		for (index = 0 ; index < this->currentSize ; index++) {
			newbuffer[index] = oldbuffer[index];
		}
		for (index = currentSize ; index < newSize; index++) {
			newbuffer[index] = NULL;
		}
	
		this->chunkBuffers = newbuffer;
		this->currentSize = newSize;
		if (oldbuffer) delete oldbuffer;
	}
	
	if (this->chunkBuffers[chunkNumber] == NULL) {
		this->chunkBuffers[chunkNumber] = buffer;
	} else {
		throw "ALREADY ALLOCATED!";
	}
	BLI_mutex_unlock(&this->mutex);
}

MemoryBuffer* MemoryManagerState::getMemoryBuffer(unsigned int chunkNumber) {
	MemoryBuffer* result = NULL;
	if (chunkNumber< this->currentSize){
		result = this->chunkBuffers[chunkNumber];
		if (result) {
			return result;
		}
	}
	
	BLI_mutex_lock(&this->mutex);
	if (chunkNumber< this->currentSize){
		result = this->chunkBuffers[chunkNumber];
	}
	
	BLI_mutex_unlock(&this->mutex);
	return result;
}
