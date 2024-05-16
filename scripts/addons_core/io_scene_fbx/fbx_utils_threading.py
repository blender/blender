# SPDX-FileCopyrightText: 2023 Blender Foundation
#
# SPDX-License-Identifier: GPL-2.0-or-later

from contextlib import contextmanager, nullcontext
import os
from queue import SimpleQueue

# Note: `bpy` cannot be imported here because this module is also used by the fbx2json.py and json2fbx.py scripts.

# For debugging/profiling purposes, can be modified at runtime to force single-threaded execution.
_MULTITHREADING_ENABLED = True
# The concurrent.futures module may not work or may not be available on WebAssembly platforms wasm32-emscripten and
# wasm32-wasi.
try:
    from concurrent.futures import ThreadPoolExecutor
except ModuleNotFoundError:
    _MULTITHREADING_ENABLED = False
    ThreadPoolExecutor = None
else:
    try:
        # The module may be available, but not be fully functional. An error may be raised when attempting to start a
        # new thread.
        with ThreadPoolExecutor() as tpe:
            # Attempt to start a thread by submitting a callable.
            tpe.submit(lambda: None)
    except Exception:
        # Assume that multithreading is not supported and fall back to single-threaded execution.
        _MULTITHREADING_ENABLED = False


def get_cpu_count():
    """Get the number of cpus assigned to the current process if that information is available on this system.
    If not available, get the total number of cpus.
    If the cpu count is indeterminable, it is assumed that there is only 1 cpu available."""
    sched_getaffinity = getattr(os, "sched_getaffinity", None)
    if sched_getaffinity is not None:
        # Return the number of cpus assigned to the current process.
        return len(sched_getaffinity(0))
    count = os.cpu_count()
    return count if count is not None else 1


class MultiThreadedTaskConsumer:
    """Helper class that encapsulates everything needed to run a function on separate threads, with a single-threaded
    fallback if multithreading is not available.

    Lower overhead than typical use of ThreadPoolExecutor because no Future objects are returned, which makes this class
    more suitable to running many smaller tasks.

    As with any threaded parallelization, because of Python's Global Interpreter Lock, only one thread can execute
    Python code at a time, so threaded parallelization is only useful when the functions used release the GIL, such as
    many IO related functions."""
    # A special task value used to signal task consumer threads to shut down.
    _SHUT_DOWN_THREADS = object()

    __slots__ = ("_consumer_function", "_shared_task_queue", "_task_consumer_futures", "_executor",
                 "_max_consumer_threads", "_shutting_down", "_max_queue_per_consumer")

    def __init__(self, consumer_function, max_consumer_threads, max_queue_per_consumer=5):
        # It's recommended to use MultiThreadedTaskConsumer.new_cpu_bound_cm() instead of creating new instances
        # directly.
        # __init__ should only be called after checking _MULTITHREADING_ENABLED.
        assert(_MULTITHREADING_ENABLED)
        # The function that will be called on separate threads to consume tasks.
        self._consumer_function = consumer_function
        # All the threads share a single queue. This is a simplistic approach, but it is unlikely to be problematic
        # unless the main thread is expected to wait a long time for the consumer threads to finish.
        self._shared_task_queue = SimpleQueue()
        # Reference to each thread is kept through the returned Future objects. This is used as part of determining when
        # new threads should be started and is used to be able to receive and handle exceptions from the threads.
        self._task_consumer_futures = []
        # Create the executor.
        self._executor = ThreadPoolExecutor(max_workers=max_consumer_threads)
        # Technically the max workers of the executor is accessible through its `._max_workers`, but since it's private,
        # meaning it could be changed without warning, we'll store the max workers/consumers ourselves.
        self._max_consumer_threads = max_consumer_threads
        # The maximum task queue size (before another consumer thread is started) increases by this amount with every
        # additional consumer thread.
        self._max_queue_per_consumer = max_queue_per_consumer
        # When shutting down the threads, this is set to True as an extra safeguard to prevent new tasks being
        # scheduled.
        self._shutting_down = False

    @classmethod
    def new_cpu_bound_cm(cls, consumer_function, other_cpu_bound_threads_in_use=1, hard_max_threads=32):
        """Return a context manager that, when entered, returns a wrapper around `consumer_function` that schedules
        `consumer_function` to be run on a separate thread.

        If the system can't use multithreading, then the context manager's returned function will instead be the input
        `consumer_function` argument, causing tasks to be run immediately on the calling thread.

        When exiting the context manager, it waits for all scheduled tasks to complete and prevents the creation of new
        tasks, similar to calling ThreadPoolExecutor.shutdown(). For these reasons, the wrapped function should only be
        called from the thread that entered the context manager, otherwise there is no guarantee that all tasks will get
        scheduled before the context manager exits.

        Any task that fails with an exception will cause all task consumer threads to stop.

        The maximum number of threads used matches the number of cpus available up to a maximum of `hard_max_threads`.
        `hard_max_threads`'s default of 32 matches ThreadPoolExecutor's default behaviour.

        The maximum number of threads used is decreased by `other_cpu_bound_threads_in_use`. Defaulting to `1`, assuming
        that the calling thread will also be doing CPU-bound work.

        Most IO-bound tasks can probably use a ThreadPoolExecutor directly instead because there will typically be fewer
        tasks and, on average, each individual task will take longer.
        If needed, `cls.new_cpu_bound_cm(consumer_function, -4)` could be suitable for lots of small IO-bound tasks,
        because it ensures a minimum of 5 threads, like the default ThreadPoolExecutor."""
        if _MULTITHREADING_ENABLED:
            max_threads = get_cpu_count() - other_cpu_bound_threads_in_use
            max_threads = min(max_threads, hard_max_threads)
            if max_threads > 0:
                return cls(consumer_function, max_threads)._wrap_executor_cm()
        # Fall back to single-threaded.
        return nullcontext(consumer_function)

    def _task_consumer_callable(self):
        """Callable that is run by each task consumer thread.
        Signals the other task consumer threads to stop when stopped intentionally or when an exception occurs."""
        try:
            while True:
                # Blocks until it can get a task.
                task_args = self._shared_task_queue.get()

                if task_args is self._SHUT_DOWN_THREADS:
                    # This special value signals that it's time for all the threads to stop.
                    break
                else:
                    # Call the task consumer function.
                    self._consumer_function(*task_args)
        finally:
            # Either the thread has been told to shut down because it received _SHUT_DOWN_THREADS or an exception has
            # occurred.
            # Add _SHUT_DOWN_THREADS to the queue so that the other consumer threads will also shut down.
            self._shared_task_queue.put(self._SHUT_DOWN_THREADS)

    def _schedule_task(self, *args):
        """Task consumer threads are only started as tasks are added.

        To mitigate starting lots of threads if many tasks are scheduled in quick succession, new threads are only
        started if the number of queued tasks grows too large.

        This function is a slight misuse of ThreadPoolExecutor. Normally each task to be scheduled would be submitted
        through ThreadPoolExecutor.submit, but doing so is noticeably slower for small tasks. We could start new Thread
        instances manually without using ThreadPoolExecutor, but ThreadPoolExecutor gives us a higher level API for
        waiting for threads to finish and handling exceptions without having to implement an API using Thread ourselves.
        """
        if self._shutting_down:
            # Shouldn't occur through normal usage.
            raise RuntimeError("Cannot schedule new tasks after shutdown")
        # Schedule the task by adding it to the task queue.
        self._shared_task_queue.put(args)
        # Check if more consumer threads need to be added to account for the rate at which tasks are being scheduled
        # compared to the rate at which tasks are being consumed.
        current_consumer_count = len(self._task_consumer_futures)
        if current_consumer_count < self._max_consumer_threads:
            # The max queue size increases as new threads are added, otherwise, by the time the next task is added, it's
            # likely that the queue size will still be over the max, causing another new thread to be added immediately.
            # Increasing the max queue size whenever a new thread is started gives some time for the new thread to start
            # up and begin consuming tasks before it's determined that another thread is needed.
            max_queue_size_for_current_consumers = self._max_queue_per_consumer * current_consumer_count

            if self._shared_task_queue.qsize() > max_queue_size_for_current_consumers:
                # Add a new consumer thread because the queue has grown too large.
                self._task_consumer_futures.append(self._executor.submit(self._task_consumer_callable))

    @contextmanager
    def _wrap_executor_cm(self):
        """Wrap the executor's context manager to instead return self._schedule_task and such that the threads
        automatically start shutting down before the executor itself starts shutting down."""
        # .__enter__()
        # Exiting the context manager of the executor will wait for all threads to finish and prevent new
        # threads from being created, as if its shutdown() method had been called.
        with self._executor:
            try:
                yield self._schedule_task
            finally:
                # .__exit__()
                self._shutting_down = True
                # Signal all consumer threads to finish up and shut down so that the executor can shut down.
                # When this is run on the same thread that schedules new tasks, this guarantees that no more tasks will
                # be scheduled after the consumer threads start to shut down.
                self._shared_task_queue.put(self._SHUT_DOWN_THREADS)

                # Because `self._executor` was entered with a context manager, it will wait for all the consumer threads
                # to finish even if we propagate an exception from one of the threads here.
                for future in self._task_consumer_futures:
                    # .exception() waits for the future to finish and returns its raised exception or None.
                    ex = future.exception()
                    if ex is not None:
                        # If one of the threads raised an exception, propagate it to the main thread.
                        # Only the first exception will be propagated if there were multiple.
                        raise ex
