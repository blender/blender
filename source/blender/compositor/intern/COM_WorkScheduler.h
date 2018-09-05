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

#ifndef __COM_WORKSCHEDULER_H__
#define __COM_WORKSCHEDULER_H__

#include "COM_ExecutionGroup.h"
extern "C" {
#  include "BLI_threads.h"
}
#include "COM_WorkPackage.h"
#include "COM_defines.h"
#include "COM_Device.h"

/** \brief the workscheduler
 * \ingroup execution
 */
class WorkScheduler {

#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	/**
	 * \brief are we being stopped.
	 */
	static bool isStopping();

	/**
	 * \brief main thread loop for cpudevices
	 * inside this loop new work is queried and being executed
	 */
	static void *thread_execute_cpu(void *data);

	/**
	 * \brief main thread loop for gpudevices
	 * inside this loop new work is queried and being executed
	 */
	static void *thread_execute_gpu(void *data);
#endif
public:
	/**
	 * \brief schedule a chunk of a group to be calculated.
	 * An execution group schedules a chunk in the WorkScheduler
	 * when ExecutionGroup.isOpenCL is set the work will be handled by a OpenCLDevice
	 * otherwise the work is scheduled for an CPUDevice
	 * \see ExecutionGroup.execute
	 * \param group the execution group
	 * \param chunkNumber the number of the chunk in the group to be executed
	 */
	static void schedule(ExecutionGroup *group, int chunkNumber);

	/**
	 * \brief initialize the WorkScheduler
	 *
	 * during initialization the mutexes are initialized.
	 * there are two mutexes (for every device type one)
	 * After mutex initialization the system is queried in order to count the number of CPUDevices and GPUDevices to be created.
	 * For every hardware thread a CPUDevice and for every OpenCL GPU device a OpenCLDevice is created.
	 * these devices are stored in a separate list (cpudevices & gpudevices)
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
	static bool hasGPUDevices();

	static int current_thread_id();

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:WorkScheduler")
#endif
};

#endif /* __COM_WORKSCHEDULER_H__ */
