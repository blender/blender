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

#include "COM_SingleThreadedNodeOperation.h"

SingleThreadedNodeOperation::SingleThreadedNodeOperation() : NodeOperation()
{
	this->m_cachedInstance = NULL;
	setComplex(true);
}

void SingleThreadedNodeOperation::initExecution()
{
	initMutex();
}

void SingleThreadedNodeOperation::executePixel(float output[4], int x, int y, void *data)
{
	this->m_cachedInstance->readNoCheck(output, x, y);
}

void SingleThreadedNodeOperation::deinitExecution()
{
	deinitMutex();
	if (this->m_cachedInstance) {
		delete this->m_cachedInstance;
		this->m_cachedInstance = NULL;
	}
}
void *SingleThreadedNodeOperation::initializeTileData(rcti *rect)
{
	if (this->m_cachedInstance) return this->m_cachedInstance;
	
	lockMutex();
	if (this->m_cachedInstance == NULL) {
		//
		this->m_cachedInstance = createMemoryBuffer(rect);
	}
	unlockMutex();
	return this->m_cachedInstance;
}
