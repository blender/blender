/*
 * Copyright 2011-2016 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "testing/testing.h"

#include "util/util_task.h"

CCL_NAMESPACE_BEGIN

namespace {

void task_run() {
}

}  // namespace

TEST(util_task, basic) {
	TaskScheduler::init(0);
	TaskPool pool;
	for(int i = 0; i < 100; ++i) {
		pool.push(function_bind(task_run));
	}
	TaskPool::Summary summary;
	pool.wait_work(&summary);
	TaskScheduler::exit();
	EXPECT_EQ(summary.num_tasks_handled, 100);
}

TEST(util_task, multiple_times) {
	for(int N = 0; N < 1000; ++N) {
		TaskScheduler::init(0);
		TaskPool pool;
		for(int i = 0; i < 100; ++i) {
			pool.push(function_bind(task_run));
		}
		TaskPool::Summary summary;
		pool.wait_work(&summary);
		TaskScheduler::exit();
		EXPECT_EQ(summary.num_tasks_handled, 100);
	}
}

CCL_NAMESPACE_END
