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

#pragma once

#include "COM_ExecutionGroup.h"

#include "COM_Device.h"
#include "COM_WorkPackage.h"
#include "COM_defines.h"

namespace blender::compositor {

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
  static void start(CompositorContext &context);

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
   * \see CompositorContext.getHasActiveOpenCLDevices
   */
  static bool has_gpu_devices();

  static int current_thread_id();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("COM:WorkScheduler")
#endif
};

}  // namespace blender::compositor
