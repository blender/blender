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

#include "COM_CPUDevice.h"

CPUDevice::CPUDevice(int thread_id)
  : Device(),
    m_thread_id(thread_id)
{
}

void CPUDevice::execute(WorkPackage *work)
{
	const unsigned int chunkNumber = work->getChunkNumber();
	ExecutionGroup *executionGroup = work->getExecutionGroup();
	rcti rect;

	executionGroup->determineChunkRect(&rect, chunkNumber);

	executionGroup->getOutputOperation()->executeRegion(&rect, chunkNumber);

	executionGroup->finalizeChunkExecution(chunkNumber, NULL);
}

