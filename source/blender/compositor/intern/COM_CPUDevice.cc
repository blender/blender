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

#include "COM_CPUDevice.h"

#include "COM_ExecutionGroup.h"

#include "BLI_rect.h"

namespace blender::compositor {

CPUDevice::CPUDevice(int thread_id) : m_thread_id(thread_id)
{
}

void CPUDevice::execute(WorkPackage *work_package)
{
  const unsigned int chunkNumber = work_package->chunk_number;
  ExecutionGroup *executionGroup = work_package->execution_group;

  executionGroup->getOutputOperation()->executeRegion(&work_package->rect, chunkNumber);
  executionGroup->finalizeChunkExecution(chunkNumber, nullptr);
}

}  // namespace blender::compositor
