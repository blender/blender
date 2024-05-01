/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "COM_WorkScheduler.h"

#include "COM_CPUDevice.h"

#include "MEM_guardedalloc.h"

#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_vector.hh"

#include "BKE_global.hh"

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

  int num_cpu_threads;
} g_work_scheduler;

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

void WorkScheduler::start()
{
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

void WorkScheduler::initialize(int num_cpu_threads)
{
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
