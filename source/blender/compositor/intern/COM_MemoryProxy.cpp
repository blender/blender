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

#include "COM_MemoryProxy.h"


MemoryProxy::MemoryProxy(DataType datatype)
{
	this->m_writeBufferOperation = NULL;
	this->m_executor = NULL;
	this->m_datatype = datatype;
}

void MemoryProxy::allocate(unsigned int width, unsigned int height)
{
	rcti result;
	result.xmin = 0;
	result.xmax = width;
	result.ymin = 0;
	result.ymax = height;

	this->m_buffer = new MemoryBuffer(this, 1, &result);
}

void MemoryProxy::free()
{
	if (this->m_buffer) {
		delete this->m_buffer;
		this->m_buffer = NULL;
	}
}

