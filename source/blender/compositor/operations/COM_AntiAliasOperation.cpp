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

#include "COM_AntiAliasOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
extern "C" {
	#include "RE_render_ext.h"
}


AntiAliasOperation::AntiAliasOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_valueReader = NULL;
	this->m_buffer = NULL;
	this->setComplex(true);
}
void AntiAliasOperation::initExecution()
{
	this->m_valueReader = this->getInputSocketReader(0);
	NodeOperation::initMutex();
}

void AntiAliasOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	if (y < 0 || (unsigned int)y >= this->m_height || x < 0 || (unsigned int)x >= this->m_width) {
		color[0] = 0.0f;
	}
	else {
		int offset = y * this->m_width + x;
		color[0] = this->m_buffer[offset] / 255.0f;
	}
	
}

void AntiAliasOperation::deinitExecution()
{
	this->m_valueReader = NULL;
	if (this->m_buffer) {
		delete this->m_buffer;
	}
	NodeOperation::deinitMutex();
}

bool AntiAliasOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti imageInput;
	if (this->m_buffer) {
		return false;
	}
	else {
		NodeOperation *operation = getInputOperation(0);
		imageInput.xmax = operation->getWidth();
		imageInput.xmin = 0;
		imageInput.ymax = operation->getHeight();
		imageInput.ymin = 0;
		if (operation->determineDependingAreaOfInterest(&imageInput, readOperation, output) ) {
			return true;
		}
	}
	return false;
}

void *AntiAliasOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	if (this->m_buffer) { return this->m_buffer; }
	lockMutex();
	if (this->m_buffer == NULL) {
		MemoryBuffer *tile = (MemoryBuffer *)this->m_valueReader->initializeTileData(rect, memoryBuffers);
		int size = tile->getHeight() * tile->getWidth();
		float *input = tile->getBuffer();
		char *valuebuffer = new char[size];
		for (int i = 0; i < size; i++) {
			float in = input[i * COM_NUMBER_OF_CHANNELS];
			if (in < 0.0f) { in = 0.0f; }
			if (in > 1.0f) {in = 1.0f; }
			valuebuffer[i] = in * 255;
		}
		antialias_tagbuf(tile->getWidth(), tile->getHeight(), valuebuffer);
		this->m_buffer = valuebuffer;
	}
	unlockMutex();
	return this->m_buffer;
}
