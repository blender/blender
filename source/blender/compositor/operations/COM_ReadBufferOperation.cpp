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

#include "COM_ReadBufferOperation.h"
#include "COM_WriteBufferOperation.h"
#include "COM_defines.h"

ReadBufferOperation::ReadBufferOperation(DataType datatype) : NodeOperation()
{
	this->addOutputSocket(datatype);
	this->m_single_value = false;
	this->m_offset = 0;
	this->m_buffer = NULL;
}

void *ReadBufferOperation::initializeTileData(rcti *rect)
{
	return m_buffer;
}

void ReadBufferOperation::determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2])
{
	if (this->m_memoryProxy != NULL) {
		WriteBufferOperation *operation = this->m_memoryProxy->getWriteBufferOperation();
		operation->determineResolution(resolution, preferredResolution);
		operation->setResolution(resolution);

		/// @todo: may not occur!, but does with blur node
		if (this->m_memoryProxy->getExecutor()) {
			this->m_memoryProxy->getExecutor()->setResolution(resolution);
		}

		m_single_value = operation->isSingleValue();
	}
}
void ReadBufferOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if (m_single_value) {
		/* write buffer has a single value stored at (0,0) */
		m_buffer->read(output, 0, 0);
	}
	else {
		switch (sampler) {
		case COM_PS_NEAREST:
			m_buffer->read(output, x, y);
			break;
		case COM_PS_BILINEAR:
		default:
			m_buffer->readBilinear(output, x, y);
			break;
		case COM_PS_BICUBIC:
			m_buffer->readBilinear(output, x, y);
			break;
		}

	}
}

void ReadBufferOperation::executePixelExtend(float output[4], float x, float y, PixelSampler sampler,
                                             MemoryBufferExtend extend_x, MemoryBufferExtend extend_y)
{
	if (m_single_value) {
		/* write buffer has a single value stored at (0,0) */
		m_buffer->read(output, 0, 0);
	}
	else if (sampler == COM_PS_NEAREST) {
		m_buffer->read(output, x, y, extend_x, extend_y);
	}
	else {
		m_buffer->readBilinear(output, x, y, extend_x, extend_y);
	}
}

void ReadBufferOperation::executePixelFiltered(float output[4], float x, float y, float dx[2], float dy[2], PixelSampler sampler)
{
	if (m_single_value) {
		/* write buffer has a single value stored at (0,0) */
		m_buffer->read(output, 0, 0);
	}
	else {
		const float uv[2] = { x, y };
		const float deriv[2][2] = { {dx[0], dx[1]}, {dy[0], dy[1]} };
		m_buffer->readEWA(output, uv, deriv, sampler);
	}
}

bool ReadBufferOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (this == readOperation) {
		BLI_rcti_init(output, input->xmin, input->xmax, input->ymin, input->ymax);
		return true;
	}
	return false;
}

void ReadBufferOperation::readResolutionFromWriteBuffer()
{
	if (this->m_memoryProxy != NULL) {
		WriteBufferOperation *operation = this->m_memoryProxy->getWriteBufferOperation();
		this->setWidth(operation->getWidth());
		this->setHeight(operation->getHeight());
	}
}

void ReadBufferOperation::updateMemoryBuffer() 
{
	this->m_buffer = this->getMemoryProxy()->getBuffer();
	
}
