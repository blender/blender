/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

struct WorkPackage;

/** \brief the workscheduler
 * \ingroup execution
 */
struct WorkScheduler {
  /**
   * \brief schedule a chunk of a group to be calculated.
   * An execution group schedules a chunk in the WorkScheduler
   */
  static void schedule(WorkPackage *package);

  /**
   * \brief initialize the WorkScheduler
   *
   * during initialization the mutexes are initialized.
   * there are two mutexes (for every device type one)
   * After mutex initialization the system is queried in order to count the number of CPUDevices
   * to be created. For every hardware thread a CPUDevice is created.
   */
  static void initialize(int num_cpu_threads);

  /**
   * \brief deinitialize the WorkScheduler
   * free all allocated resources
   */
  static void deinitialize();

  /**
   * \brief Start the execution
   * this methods will start the WorkScheduler. Inside this method all threads are initialized.
   * for every device a thread is created.
   * \see initialize Initialization and query of the number of devices
   */
  static void start();

  /**
   * \brief stop the execution
   * All created thread by the start method are destroyed.
   * \see start
   */
  static void stop();

  /**
   * \brief wait for all work to be completed.
   */
  static void finish();

  static int get_num_cpu_threads();

  static int current_thread_id();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:WorkScheduler")
#endif
};

}  // namespace blender::compositor
