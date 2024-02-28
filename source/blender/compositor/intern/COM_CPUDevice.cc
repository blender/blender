/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_CPUDevice.h"

#include "COM_WorkPackage.h"

namespace blender::compositor {

CPUDevice::CPUDevice(int thread_id) : thread_id_(thread_id) {}

void CPUDevice::execute(WorkPackage *work_package)
{
  work_package->execute_fn();
  if (work_package->executed_fn) {
    work_package->executed_fn();
  }
}

}  // namespace blender::compositor
