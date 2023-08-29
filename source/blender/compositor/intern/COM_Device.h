/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

struct WorkPackage;

/**
 * \brief Abstract class for device implementations to be used by the Compositor.
 * devices are queried, initialized and used by the WorkScheduler.
 * work are packaged as a WorkPackage instance.
 */
class Device {

 public:
  Device() = default;

  Device(const Device &other) = delete;
  Device(Device &&other) noexcept = default;

  Device &operator=(const Device &other) = delete;
  Device &operator=(Device &&other) = delete;

  /**
   * \brief Declaration of the virtual destructor
   * \note resolve warning gcc 4.7
   */
  virtual ~Device() {}

  /**
   * \brief execute a WorkPackage
   * \param work: the WorkPackage to execute
   */
  virtual void execute(struct WorkPackage *work) = 0;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:Device")
#endif
};

}  // namespace blender::compositor
