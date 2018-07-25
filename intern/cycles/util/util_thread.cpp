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

#include "util/util_thread.h"

#include "util/util_system.h"
#include "util/util_windows.h"

CCL_NAMESPACE_BEGIN

thread::thread(function<void(void)> run_cb, int group)
  : run_cb_(run_cb),
    joined_(false),
	group_(group)
{
	thread_ = std::thread(&thread::run, this);
}

thread::~thread()
{
	if(!joined_) {
		join();
	}
}

void *thread::run(void *arg)
{
	thread *self = (thread*)(arg);
	if(self->group_ != -1) {
#ifdef _WIN32
		HANDLE thread_handle = GetCurrentThread();
		GROUP_AFFINITY group_affinity = { 0 };
		int num_threads = system_cpu_group_thread_count(self->group_);
		group_affinity.Group = self->group_;
		group_affinity.Mask = (num_threads == 64)
		                              ? -1
		                              :  (1ull << num_threads) - 1;
		if(SetThreadGroupAffinity(thread_handle, &group_affinity, NULL) == 0) {
			fprintf(stderr, "Error setting thread affinity.\n");
		}
#endif
	}
	self->run_cb_();
	return NULL;
}

bool thread::join()
{
	joined_ = true;
	try {
		thread_.join();
		return true;
	}
	catch (const std::system_error&) {
		return false;
	}
}

CCL_NAMESPACE_END
