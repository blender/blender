/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "MEM_guardedalloc.h"

#include <functional>

namespace blender::compositor {

/**
 * \brief contains data about work that can be scheduled
 * \see WorkScheduler
 */
struct WorkPackage {
  /**
   * Called to execute work.
   */
  std::function<void()> execute_fn;

  /**
   * Called when work execution is finished.
   */
  std::function<void()> executed_fn;

  MEM_CXX_CLASS_ALLOC_FUNCS("COM:WorkPackage")
};

}  // namespace blender::compositor
