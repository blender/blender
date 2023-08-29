/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace blender::compositor {

struct WorkPackage;

class CompositorContext;

/** \brief the workscheduler
 * \ingroup execution
 */
struct WorkScheduler {
  /**
   * \brief schedule a chunk of a group to be calculated.
   * An execution group schedules a chunk in the WorkScheduler
   * when ExecutionGroup.get_flags().open_cl is set the work will be handled by a OpenCLDevice
   * otherwise the work is scheduled for an CPUDevice
   * \see ExecutionGroup.execute
   */
  static void schedule(WorkPackage *package);

  /**
   * \brief initialize the WorkScheduler
   *
   * during initialization the mutexes are initialized.
   * there are two mutexes (for every device type one)
   * After mutex initialization the system is queried in order to count the number of CPUDevices
   * and GPUDevices to be created. For every hardware thread a CPUDevice and for every OpenCL GPU
   * device a OpenCLDevice is created. these devices are stored in a separate list (cpudevices &
   * gpudevices)
   *
   * This function can be called multiple times to lazily initialize OpenCL.
   */
  static void initialize(bool use_opencl, int num_cpu_threads);

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
  static void start(const CompositorContext &context);

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

  /**
   * \brief Are there OpenCL capable GPU devices initialized?
   * the result of this method is stored in the CompositorContext
   * A node can generate a different operation tree when OpenCLDevices exists.
   * \see CompositorContext.get_has_active_opencl_devices
   */
  static bool has_gpu_devices();

  static int get_num_cpu_threads();

  static int current_thread_id();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:WorkScheduler")
#endif
};

}  // namespace blender::compositor
