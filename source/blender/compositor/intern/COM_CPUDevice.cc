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
#include "COM_NodeOperation.h"

namespace blender::compositor {

CPUDevice::CPUDevice(int thread_id) : thread_id_(thread_id)
{
}

void CPUDevice::execute(WorkPackage *work_package)
{
  switch (work_package->type) {
    case eWorkPackageType::Tile: {
      const unsigned int chunk_number = work_package->chunk_number;
      ExecutionGroup *execution_group = work_package->execution_group;

      execution_group->get_output_operation()->execute_region(&work_package->rect, chunk_number);
      execution_group->finalize_chunk_execution(chunk_number, nullptr);
      break;
    }
    case eWorkPackageType::CustomFunction: {
      work_package->execute_fn();
      break;
    }
  }

  if (work_package->executed_fn) {
    work_package->executed_fn();
  }
}

}  // namespace blender::compositor
