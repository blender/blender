/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "COM_Device.h"

namespace blender::compositor {

/**
 * \brief class representing a CPU device.
 * \note for every hardware thread in the system a CPUDevice instance
 * will exist in the workscheduler.
 */
class CPUDevice : public Device {
 public:
  CPUDevice(int thread_id);

  /**
   * \brief execute a WorkPackage
   * \param work: the WorkPackage to execute
   */
  void execute(WorkPackage *work) override;

  int thread_id()
  {
    return thread_id_;
  }

 protected:
  int thread_id_;
};

}  // namespace blender::compositor
