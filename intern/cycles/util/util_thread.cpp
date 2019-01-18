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

thread::thread(function<void()> run_cb, int node)
  : run_cb_(run_cb),
    joined_(false),
	node_(node)
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
	if (self->node_ != -1) {
		system_cpu_run_thread_on_node(self->node_);
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
