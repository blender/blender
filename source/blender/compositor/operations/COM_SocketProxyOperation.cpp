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

#include "COM_SocketProxyOperation.h"

SocketProxyOperation::SocketProxyOperation(DataType type) : NodeOperation()
{
	this->addInputSocket(type);
	this->addOutputSocket(type);
	this->m_inputOperation = NULL;
}

void SocketProxyOperation::initExecution()
{
	this->m_inputOperation = this->getInputSocketReader(0);
}

void SocketProxyOperation::deinitExecution()
{
	this->m_inputOperation = NULL;
}

void SocketProxyOperation::executePixelSampled(float output[4], float x, float y, PixelSampler sampler)
{
	if (this->m_inputOperation) {
		this->m_inputOperation->readSampled(output, x, y, sampler);
	}
}
