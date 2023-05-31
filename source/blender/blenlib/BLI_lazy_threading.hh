/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bli
 *
 * The goal of "lazy threading" is to avoid using threads unless one can reasonably assume that it
 * is worth distributing work over multiple threads. Using threads can lead to worse overall
 * performance by introducing inter-thread communication overhead. Keeping all work on a single
 * thread reduces this overhead to zero and also makes better use of the CPU cache.
 *
 * Functions like #parallel_for also solve this to some degree by using a "grain size". When the
 * number of individual tasks is too small, no multi-threading is used. This works very well when
 * there are many homogeneous tasks that can be expected to take approximately the same time.
 *
 * The situation becomes more difficult when:
 * - The individual tasks are not homogeneous, i.e. they take different amounts of time to compute.
 * - It is practically impossible to guess how long each task will take in advance.
 *
 * Given those constraints, a single grain size cannot be determined. One could just schedule all
 * tasks individually but that would create a lot of overhead when the tasks happen to be very
 * small. While TBB will keep all tasks on a single thread if the other threads are busy, if they
 * are idle they will start stealing the work even if that's not beneficial for overall
 * performance.
 *
 * This file provides a simple API that allows a task scheduler to properly handle tasks whose size
 * is not known in advance. The key idea is this:
 *
 * > By default, all work stays on a single thread. If an individual task notices that it is about
 * > start a computation that will take a while, it notifies the task scheduler further up on the
 * > stack. The scheduler then allows other threads to take over other tasks that were originally
 * > meant for the current thread.
 *
 * This way, when all tasks are small, no threading overhead has to be paid for. Whenever there is
 * a task that keeps the current thread busy for a while, the other tasks are moved to a separate
 * thread so that they can be executed without waiting for the long computation to finish.
 *
 * Consequently, the earlier a task knows during it execution that it will take a while, the
 * better. That's because if it is blocking anyway, it's more efficient to move the other tasks to
 * another thread earlier.
 *
 * To make this work, three things have to be solved:
 * 1. The task scheduler has to be able to start single-threaded and become multi-threaded after
 *    tasks have started executing. This has to be solved in the specific task scheduler.
 * 2. There has to be a way for the currently running task to tell the task scheduler that it is
 *    about to perform a computation that will take a while and that it would be reasonable to move
 *    other tasks to other threads. This part is implemented in the API provided by this file.
 * 3. Individual tasks have to decide when a computation is long enough to justify talking to the
 *    scheduler. This is always based on heuristics that have to be fine tuned over time. One could
 *    assume that this means adding new work-size checks to many parts in Blender, but that's
 *    actually not necessary, because these checks exist already in the form of grain sizes passed
 *    to e.g. #parallel_for. The assumption here is that when the task thinks the current work load
 *    is big enough to justify using threads, it's also big enough to justify using another thread
 *    for waiting tasks on the current thread.
 */

#include "BLI_function_ref.hh"

namespace blender::lazy_threading {

/**
 * Tell task schedulers on the current thread that it is about to start a long computation
 * and that other waiting tasks should better be moved to another thread if possible.
 */
void send_hint();

/**
 * Used by the task scheduler to receive hints from current tasks that they will take a while.
 * This should only be allocated on the stack.
 */
class HintReceiver {
 public:
  /**
   * The passed in function is called when a task signals that it will take a while.
   * \note The function has to stay alive after the call to the constructor. So one must not pass a
   * lambda directly into this constructor but store it in a separate variable on the stack first.
   */
  HintReceiver(FunctionRef<void()> fn);
  ~HintReceiver();
};

/**
 * Used to make sure that lazy-threading hints don't propagate through task isolation. This is
 * necessary to avoid deadlocks when isolated regions are used together with e.g. task pools. For
 * more info see the comment on #BLI_task_isolate.
 */
class ReceiverIsolation {
 public:
  ReceiverIsolation();
  ~ReceiverIsolation();
};

}  // namespace blender::lazy_threading
