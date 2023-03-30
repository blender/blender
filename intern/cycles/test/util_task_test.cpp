/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "testing/testing.h"

#include "util/task.h"

CCL_NAMESPACE_BEGIN

namespace {

void task_run() {}

}  // namespace

TEST(util_task, basic)
{
  TaskScheduler::init(0);
  TaskPool pool;
  for (int i = 0; i < 100; ++i) {
    pool.push(function_bind(task_run));
  }
  TaskPool::Summary summary;
  pool.wait_work(&summary);
  TaskScheduler::exit();
  EXPECT_EQ(summary.num_tasks_handled, 100);
}

TEST(util_task, multiple_times)
{
  for (int N = 0; N < 1000; ++N) {
    TaskScheduler::init(0);
    TaskPool pool;
    for (int i = 0; i < 100; ++i) {
      pool.push(function_bind(task_run));
    }
    TaskPool::Summary summary;
    pool.wait_work(&summary);
    TaskScheduler::exit();
    EXPECT_EQ(summary.num_tasks_handled, 100);
  }
}

CCL_NAMESPACE_END
