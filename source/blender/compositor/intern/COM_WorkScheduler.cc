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

#include <cstdio>
#include <list>

#include "COM_CPUDevice.h"
#include "COM_OpenCLDevice.h"
#include "COM_OpenCLKernels.cl.h"
#include "COM_WorkScheduler.h"
#include "COM_WriteBufferOperation.h"
#include "COM_compositor.h"
#include "clew.h"

#include "MEM_guardedalloc.h"

#include "BLI_threads.h"
#include "PIL_time.h"

#include "BKE_global.h"

#if COM_CURRENT_THREADING_MODEL == COM_TM_NOTHREAD
#  ifndef DEBUG /* test this so we dont get warnings in debug builds */
#    warning COM_CURRENT_THREADING_MODEL COM_TM_NOTHREAD is activated. Use only for debugging.
#  endif
#elif COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
/* do nothing - default */
#else
#  error COM_CURRENT_THREADING_MODEL No threading model selected
#endif

static ThreadLocal(CPUDevice *) g_thread_device;
static struct {
  /** \brief list of all CPUDevices. for every hardware thread an instance of CPUDevice is created
   */
  std::vector<CPUDevice *> cpu_devices;

#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
  /** \brief list of all thread for every CPUDevice in cpudevices a thread exists. */
  ListBase cpu_threads;
  bool cpu_initialized = false;
  /** \brief all scheduled work for the cpu */
  ThreadQueue *cpu_queue;
  ThreadQueue *gpu_queue;
#  ifdef COM_OPENCL_ENABLED
  cl_context opencl_context;
  cl_program opencl_program;
  /** \brief list of all OpenCLDevices. for every OpenCL GPU device an instance of OpenCLDevice is
   * created. */
  std::vector<OpenCLDevice *> gpu_devices;
  /** \brief list of all thread for every GPUDevice in cpudevices a thread exists. */
  ListBase gpu_threads;
  /** \brief all scheduled work for the GPU. */
  bool opencl_active = false;
  bool opencl_initialized = false;
#  endif
#endif

} g_work_scheduler;

#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
void *WorkScheduler::thread_execute_cpu(void *data)
{
  CPUDevice *device = (CPUDevice *)data;
  WorkPackage *work;
  BLI_thread_local_set(g_thread_device, device);
  while ((work = (WorkPackage *)BLI_thread_queue_pop(g_work_scheduler.cpu_queue))) {
    device->execute(work);
    delete work;
  }

  return nullptr;
}

void *WorkScheduler::thread_execute_gpu(void *data)
{
  Device *device = (Device *)data;
  WorkPackage *work;

  while ((work = (WorkPackage *)BLI_thread_queue_pop(g_work_scheduler.gpu_queue))) {
    device->execute(work);
    delete work;
  }

  return nullptr;
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
#  ifdef COM_OPENCL_ENABLED
  if (group->isOpenCL() && g_work_scheduler.opencl_active) {
    BLI_thread_queue_push(g_work_scheduler.gpu_queue, package);
  }
  else {
    BLI_thread_queue_push(g_work_scheduler.cpu_queue, package);
  }
#  else
  BLI_thread_queue_push(g_work_scheduler.cpu_queue, package);
#  endif
#endif
}

void WorkScheduler::start(CompositorContext &context)
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
  unsigned int index;
  g_work_scheduler.cpu_queue = BLI_thread_queue_init();
  BLI_threadpool_init(
      &g_work_scheduler.cpu_threads, thread_execute_cpu, g_work_scheduler.cpu_devices.size());
  for (index = 0; index < g_work_scheduler.cpu_devices.size(); index++) {
    Device *device = g_work_scheduler.cpu_devices[index];
    BLI_threadpool_insert(&g_work_scheduler.cpu_threads, device);
  }
#  ifdef COM_OPENCL_ENABLED
  if (context.getHasActiveOpenCLDevices()) {
    g_work_scheduler.gpu_queue = BLI_thread_queue_init();
    BLI_threadpool_init(
        &g_work_scheduler.gpu_threads, thread_execute_gpu, g_work_scheduler.gpu_devices.size());
    for (index = 0; index < g_work_scheduler.gpu_devices.size(); index++) {
      Device *device = g_work_scheduler.gpu_devices[index];
      BLI_threadpool_insert(&g_work_scheduler.gpu_threads, device);
    }
    g_work_scheduler.opencl_active = true;
  }
  else {
    g_work_scheduler.opencl_active = false;
  }
#  endif
#endif
}
void WorkScheduler::finish()
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
#  ifdef COM_OPENCL_ENABLED
  if (g_work_scheduler.opencl_active) {
    BLI_thread_queue_wait_finish(g_work_scheduler.gpu_queue);
    BLI_thread_queue_wait_finish(g_work_scheduler.cpu_queue);
  }
  else {
    BLI_thread_queue_wait_finish(g_work_scheduler.cpu_queue);
  }
#  else
  BLI_thread_queue_wait_finish(cpuqueue);
#  endif
#endif
}
void WorkScheduler::stop()
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
  BLI_thread_queue_nowait(g_work_scheduler.cpu_queue);
  BLI_threadpool_end(&g_work_scheduler.cpu_threads);
  BLI_thread_queue_free(g_work_scheduler.cpu_queue);
  g_work_scheduler.cpu_queue = nullptr;
#  ifdef COM_OPENCL_ENABLED
  if (g_work_scheduler.opencl_active) {
    BLI_thread_queue_nowait(g_work_scheduler.gpu_queue);
    BLI_threadpool_end(&g_work_scheduler.gpu_threads);
    BLI_thread_queue_free(g_work_scheduler.gpu_queue);
    g_work_scheduler.gpu_queue = nullptr;
  }
#  endif
#endif
}

bool WorkScheduler::has_gpu_devices()
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
#  ifdef COM_OPENCL_ENABLED
  return !g_work_scheduler.gpu_devices.empty();
#  else
  return false;
#  endif
#else
  return false;
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
  if (g_work_scheduler.cpu_devices.size() != num_cpu_threads) {
    Device *device;

    while (!g_work_scheduler.cpu_devices.empty()) {
      device = g_work_scheduler.cpu_devices.back();
      g_work_scheduler.cpu_devices.pop_back();
      device->deinitialize();
      delete device;
    }
    if (g_work_scheduler.cpu_initialized) {
      BLI_thread_local_delete(g_thread_device);
    }
    g_work_scheduler.cpu_initialized = false;
  }

  /* initialize CPU threads */
  if (!g_work_scheduler.cpu_initialized) {
    for (int index = 0; index < num_cpu_threads; index++) {
      CPUDevice *device = new CPUDevice(index);
      device->initialize();
      g_work_scheduler.cpu_devices.push_back(device);
    }
    BLI_thread_local_create(g_thread_device);
    g_work_scheduler.cpu_initialized = true;
  }

#  ifdef COM_OPENCL_ENABLED
  /* deinitialize OpenCL GPU's */
  if (use_opencl && !g_work_scheduler.opencl_initialized) {
    g_work_scheduler.opencl_context = nullptr;
    g_work_scheduler.opencl_program = nullptr;

    /* This will check for errors and skip if already initialized. */
    if (clewInit() != CLEW_SUCCESS) {
      return;
    }

    if (clCreateContextFromType) {
      cl_uint numberOfPlatforms = 0;
      cl_int error;
      error = clGetPlatformIDs(0, nullptr, &numberOfPlatforms);
      if (error == -1001) {
      } /* GPU not supported */
      else if (error != CL_SUCCESS) {
        printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
      }
      if (G.f & G_DEBUG) {
        printf("%u number of platforms\n", numberOfPlatforms);
      }
      cl_platform_id *platforms = (cl_platform_id *)MEM_mallocN(
          sizeof(cl_platform_id) * numberOfPlatforms, __func__);
      error = clGetPlatformIDs(numberOfPlatforms, platforms, nullptr);
      unsigned int indexPlatform;
      for (indexPlatform = 0; indexPlatform < numberOfPlatforms; indexPlatform++) {
        cl_platform_id platform = platforms[indexPlatform];
        cl_uint numberOfDevices = 0;
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &numberOfDevices);
        if (numberOfDevices <= 0) {
          continue;
        }

        cl_device_id *cldevices = (cl_device_id *)MEM_mallocN(
            sizeof(cl_device_id) * numberOfDevices, __func__);
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, numberOfDevices, cldevices, nullptr);

        g_work_scheduler.opencl_context = clCreateContext(
            nullptr, numberOfDevices, cldevices, clContextError, nullptr, &error);
        if (error != CL_SUCCESS) {
          printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
        }
        const char *cl_str[2] = {datatoc_COM_OpenCLKernels_cl, nullptr};
        g_work_scheduler.opencl_program = clCreateProgramWithSource(
            g_work_scheduler.opencl_context, 1, cl_str, nullptr, &error);
        error = clBuildProgram(g_work_scheduler.opencl_program,
                               numberOfDevices,
                               cldevices,
                               nullptr,
                               nullptr,
                               nullptr);
        if (error != CL_SUCCESS) {
          cl_int error2;
          size_t ret_val_size = 0;
          printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
          error2 = clGetProgramBuildInfo(g_work_scheduler.opencl_program,
                                         cldevices[0],
                                         CL_PROGRAM_BUILD_LOG,
                                         0,
                                         nullptr,
                                         &ret_val_size);
          if (error2 != CL_SUCCESS) {
            printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
          }
          char *build_log = (char *)MEM_mallocN(sizeof(char) * ret_val_size + 1, __func__);
          error2 = clGetProgramBuildInfo(g_work_scheduler.opencl_program,
                                         cldevices[0],
                                         CL_PROGRAM_BUILD_LOG,
                                         ret_val_size,
                                         build_log,
                                         nullptr);
          if (error2 != CL_SUCCESS) {
            printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
          }
          build_log[ret_val_size] = '\0';
          printf("%s", build_log);
          MEM_freeN(build_log);
        }
        else {
          unsigned int indexDevices;
          for (indexDevices = 0; indexDevices < numberOfDevices; indexDevices++) {
            cl_device_id device = cldevices[indexDevices];
            cl_int vendorID = 0;
            cl_int error2 = clGetDeviceInfo(
                device, CL_DEVICE_VENDOR_ID, sizeof(cl_int), &vendorID, nullptr);
            if (error2 != CL_SUCCESS) {
              printf("CLERROR[%d]: %s\n", error2, clewErrorString(error2));
            }
            OpenCLDevice *clDevice = new OpenCLDevice(g_work_scheduler.opencl_context,
                                                      device,
                                                      g_work_scheduler.opencl_program,
                                                      vendorID);
            clDevice->initialize();
            g_work_scheduler.gpu_devices.push_back(clDevice);
          }
        }
        MEM_freeN(cldevices);
      }
      MEM_freeN(platforms);
    }

    g_work_scheduler.opencl_initialized = true;
  }
#  endif
#endif
}

void WorkScheduler::deinitialize()
{
#if COM_CURRENT_THREADING_MODEL == COM_TM_QUEUE
  /* deinitialize CPU threads */
  if (g_work_scheduler.cpu_initialized) {
    Device *device;
    while (!g_work_scheduler.cpu_devices.empty()) {
      device = g_work_scheduler.cpu_devices.back();
      g_work_scheduler.cpu_devices.pop_back();
      device->deinitialize();
      delete device;
    }
    BLI_thread_local_delete(g_thread_device);
    g_work_scheduler.cpu_initialized = false;
  }

#  ifdef COM_OPENCL_ENABLED
  /* deinitialize OpenCL GPU's */
  if (g_work_scheduler.opencl_initialized) {
    Device *device;
    while (!g_work_scheduler.gpu_devices.empty()) {
      device = g_work_scheduler.gpu_devices.back();
      g_work_scheduler.gpu_devices.pop_back();
      device->deinitialize();
      delete device;
    }
    if (g_work_scheduler.opencl_program) {
      clReleaseProgram(g_work_scheduler.opencl_program);
      g_work_scheduler.opencl_program = nullptr;
    }
    if (g_work_scheduler.opencl_context) {
      clReleaseContext(g_work_scheduler.opencl_context);
      g_work_scheduler.opencl_context = nullptr;
    }

    g_work_scheduler.opencl_initialized = false;
  }
#  endif
#endif
}

int WorkScheduler::current_thread_id()
{
  CPUDevice *device = (CPUDevice *)BLI_thread_local_get(g_thread_device);
  return device->thread_id();
}
