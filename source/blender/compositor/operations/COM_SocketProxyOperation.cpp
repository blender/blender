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

SocketProxyOperation::SocketProxyOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR/*|COM_DT_VECTOR|COM_DT_VALUE*/);
	this->addOutputSocket(COM_DT_COLOR);
	this->inputOperation = NULL;
}

void SocketProxyOperation::initExecution()
{
	this->inputOperation = this->getInputSocketReader(0);
}

void SocketProxyOperation::deinitExecution()
{
	this->inputOperation = NULL;
}

void SocketProxyOperation::executePixel(float *color,float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[])
{
	if (this->inputOperation) {
		this->inputOperation->read(color, x, y, sampler, inputBuffers);
	}
}
