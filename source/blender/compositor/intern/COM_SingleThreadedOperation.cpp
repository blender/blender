/*
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
 * Copyright 2011, Blender Foundation.
 */

#include "COM_SingleThreadedOperation.h"

SingleThreadedOperation::SingleThreadedOperation()
{
  this->m_cachedInstance = nullptr;
  setComplex(true);
}

void SingleThreadedOperation::initExecution()
{
  initMutex();
}

void SingleThreadedOperation::executePixel(float output[4], int x, int y, void * /*data*/)
{
  this->m_cachedInstance->readNoCheck(output, x, y);
}

void SingleThreadedOperation::deinitExecution()
{
  deinitMutex();
  if (this->m_cachedInstance) {
    delete this->m_cachedInstance;
    this->m_cachedInstance = nullptr;
  }
}
void *SingleThreadedOperation::initializeTileData(rcti *rect)
{
  if (this->m_cachedInstance) {
    return this->m_cachedInstance;
  }

  lockMutex();
  if (this->m_cachedInstance == nullptr) {
    //
    this->m_cachedInstance = createMemoryBuffer(rect);
  }
  unlockMutex();
  return this->m_cachedInstance;
}
