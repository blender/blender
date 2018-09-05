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

#include <list>
#include <stdio.h>

#include "COM_compositor.h"
#include "COM_WorkScheduler.h"
#include "COM_CPUDevice.h"
#include "COM_OpenCLDevice.h"
#include "COM_OpenCLKernels.cl.h"
#include "clew.h"
#include "COM_WriteBufferOperation.h"

#include "MEM_guardedalloc.h"

#include "PIL_time.h"
#include "BLI_threads.h"

#include "BKE_global.h"

#if COM_CURRENT_THREADING_MODEL == COM_TM_NOTHREAD
#  ifndef DEBUG  /* test this so we dont get warnings in debug builds */
#    warning COM_CURRENT_THREADING_MODEL COM_TM_NOTHREAD is activated. Use only for debugging.
#  endif
#elif COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
   /* do nothing - default */
#else
#  error COM_CURRENT_THREADING_MODEL No threading model selected
#endif


/// \brief list of all CPUDevices. for every hardware thread an instance of CPUDevice is created
static vector<CPUDevice*> g_cpudevices;
static ThreadLocal(CPUDevice *) g_thread_device;

#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
/// \brief list of all thread for every CPUDevice in cpudevices a thread exists
static ListBase g_cputhreads;
static bool g_cpuInitialized = false;
/// \brief all scheduled work for the cpu
static ThreadQueue *g_cpuqueue;
static ThreadQueue *g_gpuqueue;
#ifdef COM_OPENCL_ENABLED
static cl_context g_context;
static cl_program g_program;
/// \brief list of all OpenCLDevices. for every OpenCL GPU device an instance of OpenCLDevice is created
static vector<OpenCLDevice *> g_gpudevices;
/// \brief list of all thread for every GPUDevice in cpudevices a thread exists
static ListBase g_gputhreads;
/// \brief all scheduled work for the gpu
#ifdef COM_OPENCL_ENABLED
static bool g_openclActive = false;
static bool g_openclInitialized = false;
#endif
#endif
#endif

#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
void *WorkScheduler::thread_execute_cpu(void *data)
{
	CPUDevice *device = (CPUDevice *)data;
	WorkPackage *work;
	BLI_thread_local_set(g_thread_device, device);
	while ((work = (WorkPackage *)BLI_thread_queue_pop(g_cpuqueue))) {
		device->execute(work);
		delete work;
	}

	return NULL;
}

void *WorkScheduler::thread_execute_gpu(void *data)
{
	Device *device = (Device *)data;
	WorkPackage *work;

	while ((work = (WorkPackage *)BLI_thread_queue_pop(g_gpuqueue))) {
		device->execute(work);
		delete work;
	}

	return NULL;
}
#endif



void WorkScheduler::schedule(ExecutionGroup *group, int chunkNumber)
{
	WorkPackage *package = new WorkPackage(group, chunkNumber);
#if COM_CURRENT_THREADING_MODEL == COM_TM_NOTHREAD
	CPUDevice device(0);
	device.execute(package);
	delete package;
#elif COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
#ifdef COM_OPENCL_ENABLED
	if (group->isOpenCL() && g_openclActive) {
		BLI_thread_queue_push(g_gpuqueue, package);
	}
	else {
		BLI_thread_queue_push(g_cpuqueue, package);
	}
#else
	BLI_thread_queue_push(g_cpuqueue, package);
#endif
#endif
}

void WorkScheduler::start(CompositorContext &context)
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	unsigned int index;
	g_cpuqueue = BLI_thread_queue_init();
	BLI_threadpool_init(&g_cputhreads, thread_execute_cpu, g_cpudevices.size());
	for (index = 0; index < g_cpudevices.size(); index++) {
		Device *device = g_cpudevices[index];
		BLI_threadpool_insert(&g_cputhreads, device);
	}
#ifdef COM_OPENCL_ENABLED
	if (context.getHasActiveOpenCLDevices()) {
		g_gpuqueue = BLI_thread_queue_init();
		BLI_threadpool_init(&g_gputhreads, thread_execute_gpu, g_gpudevices.size());
		for (index = 0; index < g_gpudevices.size(); index++) {
			Device *device = g_gpudevices[index];
			BLI_threadpool_insert(&g_gputhreads, device);
		}
		g_openclActive = true;
	}
	else {
		g_openclActive = false;
	}
#endif
#endif
}
void WorkScheduler::finish()
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
#ifdef COM_OPENCL_ENABLED
	if (g_openclActive) {
		BLI_thread_queue_wait_finish(g_gpuqueue);
		BLI_thread_queue_wait_finish(g_cpuqueue);
	}
	else {
		BLI_thread_queue_wait_finish(g_cpuqueue);
	}
#else
	BLI_thread_queue_wait_finish(cpuqueue);
#endif
#endif
}
void WorkScheduler::stop()
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	BLI_thread_queue_nowait(g_cpuqueue);
	BLI_threadpool_end(&g_cputhreads);
	BLI_thread_queue_free(g_cpuqueue);
	g_cpuqueue = NULL;
#ifdef COM_OPENCL_ENABLED
	if (g_openclActive) {
		BLI_thread_queue_nowait(g_gpuqueue);
		BLI_threadpool_end(&g_gputhreads);
		BLI_thread_queue_free(g_gpuqueue);
		g_gpuqueue = NULL;
	}
#endif
#endif
}

bool WorkScheduler::hasGPUDevices()
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
#ifdef COM_OPENCL_ENABLED
	return g_gpudevices.size() > 0;
#else
	return 0;
#endif
#else
	return 0;
#endif
}

#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
static void CL_CALLBACK clContextError(const char *errinfo,
                                       const void * /*private_info*/,
                                       size_t /*cb*/,
                                       void * /*user_data*/)
{
	printf("OPENCL error: %s\n", errinfo);
}
#endif

void WorkScheduler::initialize(bool use_opencl, int num_cpu_threads)
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	/* deinitialize if number of threads doesn't match */
	if (g_cpudevices.size() != num_cpu_threads) {
		Device *device;

		while (g_cpudevices.size() > 0) {
			device = g_cpudevices.back();
			g_cpudevices.pop_back();
			device->deinitialize();
			delete device;
		}
		if (g_cpuInitialized) {
			BLI_thread_local_delete(g_thread_device);
		}
		g_cpuInitialized = false;
	}

	/* initialize CPU threads */
	if (!g_cpuInitialized) {
		for (int index = 0; index < num_cpu_threads; index++) {
			CPUDevice *device = new CPUDevice(index);
			device->initialize();
			g_cpudevices.push_back(device);
		}
		BLI_thread_local_create(g_thread_device);
		g_cpuInitialized = true;
	}

#ifdef COM_OPENCL_ENABLED
	/* deinitialize OpenCL GPU's */
	if (use_opencl && !g_openclInitialized) {
		g_context = NULL;
		g_program = NULL;

		if (clewInit() != CLEW_SUCCESS) /* this will check for errors and skip if already initialized */
			return;

		if (clCreateContextFromType) {
			cl_uint numberOfPlatforms = 0;
			cl_int error;
			error = clGetPlatformIDs(0, 0, &numberOfPlatforms);
			if (error == -1001) { }   /* GPU not supported */
			else if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
			if (G.f & G_DEBUG) printf("%u number of platforms\n", numberOfPlatforms);
			cl_platform_id *platforms = (cl_platform_id *)MEM_mallocN(sizeof(cl_platform_id) * numberOfPlatforms, __func__);
			error = clGetPlatformIDs(numberOfPlatforms, platforms, 0);
			unsigned int indexPlatform;
			for (indexPlatform = 0; indexPlatform < numberOfPlatforms; indexPlatform++) {
				cl_platform_id platform = platforms[indexPlatform];
				cl_uint numberOfDevices = 0;
				clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, 0, &numberOfDevices);
				if (numberOfDevices <= 0)
					continue;

				cl_device_id *cldevices = (cl_device_id *)MEM_mallocN(sizeof(cl_device_id) * numberOfDevices, __func__);
				clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, numberOfDevices, cldevices, 0);

				g_context = clCreateContext(NULL, numberOfDevices, cldevices, clContextError, NULL, &error);
				if (error != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error));  }
				const char *cl_str[2] = {datatoc_COM_OpenCLKernels_cl, NULL};
				g_program = clCreateProgramWithSource(g_context, 1, cl_str, 0, &error);
				error = clBuildProgram(g_program, numberOfDevices, cldevices, 0, 0, 0);
				if (error != CL_SUCCESS) {
					cl_int error2;
					size_t ret_val_size = 0;
					printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
					error2 = clGetProgramBuildInfo(g_program, cldevices[0], CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
					if (error2 != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error)); }
					char *build_log = (char *)MEM_mallocN(sizeof(char) * ret_val_size + 1, __func__);
					error2 = clGetProgramBuildInfo(g_program, cldevices[0], CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
					if (error2 != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error, clewErrorString(error)); }
					build_log[ret_val_size] = '\0';
					printf("%s", build_log);
					MEM_freeN(build_log);
				}
				else {
					unsigned int indexDevices;
					for (indexDevices = 0; indexDevices < numberOfDevices; indexDevices++) {
						cl_device_id device = cldevices[indexDevices];
						cl_int vendorID = 0;
						cl_int error2 = clGetDeviceInfo(device, CL_DEVICE_VENDOR_ID, sizeof(cl_int), &vendorID, NULL);
						if (error2 != CL_SUCCESS) { printf("CLERROR[%d]: %s\n", error2, clewErrorString(error2)); }
						OpenCLDevice *clDevice = new OpenCLDevice(g_context, device, g_program, vendorID);
						clDevice->initialize();
						g_gpudevices.push_back(clDevice);
					}
				}
				MEM_freeN(cldevices);
			}
			MEM_freeN(platforms);
		}

		g_openclInitialized = true;
	}
#endif
#endif
}

void WorkScheduler::deinitialize()
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
	/* deinitialize CPU threads */
	if (g_cpuInitialized) {
		Device *device;
		while (g_cpudevices.size() > 0) {
			device = g_cpudevices.back();
			g_cpudevices.pop_back();
			device->deinitialize();
			delete device;
		}
		BLI_thread_local_delete(g_thread_device);
		g_cpuInitialized = false;
	}

#ifdef COM_OPENCL_ENABLED
	/* deinitialize OpenCL GPU's */
	if (g_openclInitialized) {
		Device *device;
		while (g_gpudevices.size() > 0) {
			device = g_gpudevices.back();
			g_gpudevices.pop_back();
			device->deinitialize();
			delete device;
		}
		if (g_program) {
			clReleaseProgram(g_program);
			g_program = NULL;
		}
		if (g_context) {
			clReleaseContext(g_context);
			g_context = NULL;
		}

		g_openclInitialized = false;
	}
#endif
#endif
}

int WorkScheduler::current_thread_id()
{
	CPUDevice *device = (CPUDevice *)BLI_thread_local_get(g_thread_device);
	return device->thread_id();
}
