/* SPDX-FileCopyrightText: 2011 Blender Foundation.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_WorkScheduler.h"

#include "COM_CPUDevice.h"
#include "COM_CompositorContext.h"
#include "COM_ExecutionGroup.h"
#include "COM_OpenCLDevice.h"
#include "COM_OpenCLKernels.cl.h"
#include "COM_WriteBufferOperation.h"

#include "clew.h"

#include "MEM_guardedalloc.h"

#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "BKE_global.h"

namespace blender::compositor {

enum class ThreadingModel {
  /** Everything is executed in the caller thread. easy for debugging. */
  SingleThreaded,
  /** Multi-threaded model, which uses the BLI_thread_queue pattern. */
  Queue,
  /** Uses BLI_task as threading backend. */
  Task
};

/**
 * Returns the active threading model.
 *
 * Default is `ThreadingModel::Queue`.
 */
constexpr ThreadingModel COM_threading_model()
{
  return ThreadingModel::Queue;
}

/**
 * Does the active threading model support opencl?
 */
constexpr bool COM_is_opencl_enabled()
{
  return COM_threading_model() != ThreadingModel::SingleThreaded;
}

static ThreadLocal(CPUDevice *) g_thread_device;
static struct {
  struct {
    /** \brief list of all CPUDevices. for every hardware thread an instance of CPUDevice is
     * created
     */
    Vector<CPUDevice> devices;

    /** \brief list of all thread for every CPUDevice in cpudevices a thread exists. */
    ListBase threads;
    bool initialized = false;
    /** \brief all scheduled work for the cpu */
    ThreadQueue *queue;
  } queue;

  struct {
    TaskPool *pool;
  } task;

  struct {
    ThreadQueue *queue;
    cl_context context;
    cl_program program;
    /** \brief list of all OpenCLDevices. for every OpenCL GPU device an instance of OpenCLDevice
     * is created. */
    Vector<OpenCLDevice> devices;
    /** \brief list of all thread for every GPUDevice in cpudevices a thread exists. */
    ListBase threads;
    /** \brief all scheduled work for the GPU. */
    bool active = false;
    bool initialized = false;
  } opencl;

  int num_cpu_threads;
} g_work_scheduler;

/* -------------------------------------------------------------------- */
/** \name OpenCL Scheduling
 * \{ */

static void CL_CALLBACK cl_context_error(const char *errinfo,
                                         const void * /*private_info*/,
                                         size_t /*cb*/,
                                         void * /*user_data*/)
{
  printf("OPENCL error: %s\n", errinfo);
}

static void *thread_execute_gpu(void *data)
{
  Device *device = (Device *)data;
  WorkPackage *work;

  while ((work = (WorkPackage *)BLI_thread_queue_pop(g_work_scheduler.opencl.queue))) {
    device->execute(work);
  }

  return nullptr;
}

static void opencl_start(const CompositorContext &context)
{
  if (context.get_has_active_opencl_devices()) {
    g_work_scheduler.opencl.queue = BLI_thread_queue_init();
    BLI_threadpool_init(&g_work_scheduler.opencl.threads,
                        thread_execute_gpu,
                        g_work_scheduler.opencl.devices.size());
    for (Device &device : g_work_scheduler.opencl.devices) {
      BLI_threadpool_insert(&g_work_scheduler.opencl.threads, &device);
    }
    g_work_scheduler.opencl.active = true;
  }
  else {
    g_work_scheduler.opencl.active = false;
  }
}

static bool opencl_schedule(WorkPackage *package)
{
  if (package->type == eWorkPackageType::Tile && package->execution_group->get_flags().open_cl &&
      g_work_scheduler.opencl.active)
  {
    BLI_thread_queue_push(g_work_scheduler.opencl.queue, package);
    return true;
  }
  return false;
}

static void opencl_finish()
{
  if (g_work_scheduler.opencl.active) {
    BLI_thread_queue_wait_finish(g_work_scheduler.opencl.queue);
  }
}

static void opencl_stop()
{
  if (g_work_scheduler.opencl.active) {
    BLI_thread_queue_nowait(g_work_scheduler.opencl.queue);
    BLI_threadpool_end(&g_work_scheduler.opencl.threads);
    BLI_thread_queue_free(g_work_scheduler.opencl.queue);
    g_work_scheduler.opencl.queue = nullptr;
  }
}

static bool opencl_has_gpu_devices()
{
  return !g_work_scheduler.opencl.devices.is_empty();
}

static void opencl_initialize(const bool use_opencl)
{
  /* deinitialize OpenCL GPU's */
  if (use_opencl && !g_work_scheduler.opencl.initialized) {
    g_work_scheduler.opencl.context = nullptr;
    g_work_scheduler.opencl.program = nullptr;

    /* This will check for errors and skip if already initialized. */
    if (clewInit() != CLEW_SUCCESS) {
      return;
    }

    if (clCreateContextFromType) {
      cl_uint number_of_platforms = 0;
      cl_int error;
      error = clGetPlatformIDs(0, nullptr, &number_of_platforms);
      if (error == -1001) {
      } /* GPU not supported */
      else if (error != CL_SUCCESS) {
        printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
      }
      if (G.f & G_DEBUG) {
        printf("%u number of platforms\n", number_of_platforms);
      }
      cl_platform_id *platforms = (cl_platform_id *)MEM_mallocN(
          sizeof(cl_platform_id) * number_of_platforms, __func__);
      error = clGetPlatformIDs(number_of_platforms, platforms, nullptr);
      uint index_platform;
      for (index_platform = 0; index_platform < number_of_platforms; index_platform++) {
        cl_platform_id platform = platforms[index_platform];
        cl_uint number_of_devices = 0;
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 0, nullptr, &number_of_devices);
        if (number_of_devices <= 0) {
          continue;
        }

        cl_device_id *cldevices = (cl_device_id *)MEM_mallocN(
            sizeof(cl_device_id) * number_of_devices, __func__);
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, number_of_devices, cldevices, nullptr);

        g_work_scheduler.opencl.context = clCreateContext(
            nullptr, number_of_devices, cldevices, cl_context_error, nullptr, &error);
        if (error != CL_SUCCESS) {
          printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
        }
        const char *cl_str[2] = {datatoc_COM_OpenCLKernels_cl, nullptr};
        g_work_scheduler.opencl.program = clCreateProgramWithSource(
            g_work_scheduler.opencl.context, 1, cl_str, nullptr, &error);
        error = clBuildProgram(g_work_scheduler.opencl.program,
                               number_of_devices,
                               cldevices,
                               nullptr,
                               nullptr,
                               nullptr);
        if (error != CL_SUCCESS) {
          cl_int error2;
          size_t ret_val_size = 0;
          printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
          error2 = clGetProgramBuildInfo(g_work_scheduler.opencl.program,
                                         cldevices[0],
                                         CL_PROGRAM_BUILD_LOG,
                                         0,
                                         nullptr,
                                         &ret_val_size);
          if (error2 != CL_SUCCESS) {
            printf("CLERROR[%d]: %s\n", error, clewErrorString(error));
          }
          char *build_log = (char *)MEM_mallocN(sizeof(char) * ret_val_size + 1, __func__);
          error2 = clGetProgramBuildInfo(g_work_scheduler.opencl.program,
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
          uint index_devices;
          for (index_devices = 0; index_devices < number_of_devices; index_devices++) {
            cl_device_id device = cldevices[index_devices];
            cl_int vendorID = 0;
            cl_int error2 = clGetDeviceInfo(
                device, CL_DEVICE_VENDOR_ID, sizeof(cl_int), &vendorID, nullptr);
            if (error2 != CL_SUCCESS) {
              printf("CLERROR[%d]: %s\n", error2, clewErrorString(error2));
            }
            g_work_scheduler.opencl.devices.append_as(g_work_scheduler.opencl.context,
                                                      device,
                                                      g_work_scheduler.opencl.program,
                                                      vendorID);
          }
        }
        MEM_freeN(cldevices);
      }
      MEM_freeN(platforms);
    }

    g_work_scheduler.opencl.initialized = true;
  }
}

static void opencl_deinitialize()
{
  g_work_scheduler.opencl.devices.clear_and_shrink();

  if (g_work_scheduler.opencl.program) {
    clReleaseProgram(g_work_scheduler.opencl.program);
    g_work_scheduler.opencl.program = nullptr;
  }

  if (g_work_scheduler.opencl.context) {
    clReleaseContext(g_work_scheduler.opencl.context);
    g_work_scheduler.opencl.context = nullptr;
  }

  g_work_scheduler.opencl.initialized = false;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Single threaded Scheduling
 * \{ */

static void threading_model_single_thread_execute(WorkPackage *package)
{
  CPUDevice device(0);
  device.execute(package);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Queue Scheduling
 * \{ */

static void *threading_model_queue_execute(void *data)
{
  CPUDevice *device = (CPUDevice *)data;
  WorkPackage *work;
  BLI_thread_local_set(g_thread_device, device);
  while ((work = (WorkPackage *)BLI_thread_queue_pop(g_work_scheduler.queue.queue))) {
    device->execute(work);
  }

  return nullptr;
}

static void threading_model_queue_schedule(WorkPackage *package)
{
  BLI_thread_queue_push(g_work_scheduler.queue.queue, package);
}

static void threading_model_queue_start()
{
  g_work_scheduler.queue.queue = BLI_thread_queue_init();
  BLI_threadpool_init(&g_work_scheduler.queue.threads,
                      threading_model_queue_execute,
                      g_work_scheduler.queue.devices.size());
  for (Device &device : g_work_scheduler.queue.devices) {
    BLI_threadpool_insert(&g_work_scheduler.queue.threads, &device);
  }
}

static void threading_model_queue_finish()
{
  BLI_thread_queue_wait_finish(g_work_scheduler.queue.queue);
}

static void threading_model_queue_stop()
{
  BLI_thread_queue_nowait(g_work_scheduler.queue.queue);
  BLI_threadpool_end(&g_work_scheduler.queue.threads);
  BLI_thread_queue_free(g_work_scheduler.queue.queue);
  g_work_scheduler.queue.queue = nullptr;
}

static void threading_model_queue_initialize(const int num_cpu_threads)
{
  /* Reinitialize if number of threads doesn't match. */
  if (g_work_scheduler.queue.devices.size() != num_cpu_threads) {
    g_work_scheduler.queue.devices.clear();
    if (g_work_scheduler.queue.initialized) {
      BLI_thread_local_delete(g_thread_device);
      g_work_scheduler.queue.initialized = false;
    }
  }

  /* Initialize CPU threads. */
  if (!g_work_scheduler.queue.initialized) {
    for (int index = 0; index < num_cpu_threads; index++) {
      g_work_scheduler.queue.devices.append_as(index);
    }
    BLI_thread_local_create(g_thread_device);
    g_work_scheduler.queue.initialized = true;
  }
}
static void threading_model_queue_deinitialize()
{
  /* deinitialize CPU threads */
  if (g_work_scheduler.queue.initialized) {
    g_work_scheduler.queue.devices.clear_and_shrink();

    BLI_thread_local_delete(g_thread_device);
    g_work_scheduler.queue.initialized = false;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Task Scheduling
 * \{ */

static void threading_model_task_execute(TaskPool *__restrict /*pool*/, void *task_data)
{
  WorkPackage *package = static_cast<WorkPackage *>(task_data);
  CPUDevice device(BLI_task_parallel_thread_id(nullptr));
  BLI_thread_local_set(g_thread_device, &device);
  device.execute(package);
}

static void threading_model_task_schedule(WorkPackage *package)
{
  BLI_task_pool_push(
      g_work_scheduler.task.pool, threading_model_task_execute, package, false, nullptr);
}

static void threading_model_task_start()
{
  BLI_thread_local_create(g_thread_device);
  g_work_scheduler.task.pool = BLI_task_pool_create(nullptr, TASK_PRIORITY_HIGH);
}

static void threading_model_task_finish()
{
  BLI_task_pool_work_and_wait(g_work_scheduler.task.pool);
}

static void threading_model_task_stop()
{
  BLI_task_pool_free(g_work_scheduler.task.pool);
  g_work_scheduler.task.pool = nullptr;
  BLI_thread_local_delete(g_thread_device);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void WorkScheduler::schedule(WorkPackage *package)
{
  if (COM_is_opencl_enabled()) {
    if (opencl_schedule(package)) {
      return;
    }
  }

  switch (COM_threading_model()) {
    case ThreadingModel::SingleThreaded: {
      threading_model_single_thread_execute(package);
      break;
    }

    case ThreadingModel::Queue: {
      threading_model_queue_schedule(package);
      break;
    }

    case ThreadingModel::Task: {
      threading_model_task_schedule(package);
      break;
    }
  }
}

void WorkScheduler::start(const CompositorContext &context)
{
  if (COM_is_opencl_enabled()) {
    opencl_start(context);
  }

  switch (COM_threading_model()) {
    case ThreadingModel::SingleThreaded:
      /* Nothing to do. */
      break;

    case ThreadingModel::Queue:
      threading_model_queue_start();
      break;

    case ThreadingModel::Task:
      threading_model_task_start();
      break;
  }
}

void WorkScheduler::finish()
{
  if (COM_is_opencl_enabled()) {
    opencl_finish();
  }

  switch (COM_threading_model()) {
    case ThreadingModel::SingleThreaded:
      /* Nothing to do. */
      break;

    case ThreadingModel::Queue:
      threading_model_queue_finish();
      break;

    case ThreadingModel::Task:
      threading_model_task_finish();
      break;
  }
}

void WorkScheduler::stop()
{
  if (COM_is_opencl_enabled()) {
    opencl_stop();
  }

  switch (COM_threading_model()) {
    case ThreadingModel::SingleThreaded:
      /* Nothing to do. */
      break;

    case ThreadingModel::Queue:
      threading_model_queue_stop();
      break;

    case ThreadingModel::Task:
      threading_model_task_stop();
      break;
  }
}

bool WorkScheduler::has_gpu_devices()
{
  if (COM_is_opencl_enabled()) {
    return opencl_has_gpu_devices();
  }
  return false;
}

void WorkScheduler::initialize(bool use_opencl, int num_cpu_threads)
{
  if (COM_is_opencl_enabled()) {
    opencl_initialize(use_opencl);
  }

  g_work_scheduler.num_cpu_threads = num_cpu_threads;
  switch (COM_threading_model()) {
    case ThreadingModel::SingleThreaded:
      g_work_scheduler.num_cpu_threads = 1;
      /* Nothing to do. */
      break;
    case ThreadingModel::Queue:
      threading_model_queue_initialize(num_cpu_threads);
      break;

    case ThreadingModel::Task:
      /* Nothing to do. */
      break;
  }
}

void WorkScheduler::deinitialize()
{
  if (COM_is_opencl_enabled()) {
    opencl_deinitialize();
  }

  switch (COM_threading_model()) {
    case ThreadingModel::SingleThreaded:
      /* Nothing to do. */
      break;

    case ThreadingModel::Queue:
      threading_model_queue_deinitialize();
      break;

    case ThreadingModel::Task:
      /* Nothing to do. */
      break;
  }
}

int WorkScheduler::get_num_cpu_threads()
{
  return g_work_scheduler.num_cpu_threads;
}

int WorkScheduler::current_thread_id()
{
  if (COM_threading_model() == ThreadingModel::SingleThreaded) {
    return 0;
  }

  CPUDevice *device = (CPUDevice *)BLI_thread_local_get(g_thread_device);
  return device->thread_id();
}

/** \} */

}  // namespace blender::compositor
