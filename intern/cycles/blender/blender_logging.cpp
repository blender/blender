/*
 * Copyright 2011-2014 Blender Foundation
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

#include "blender/CCL_api.h"
#include "util/util_logging.h"

void CCL_init_logging(const char *argv0)
{
	ccl::util_logging_init(argv0);
}

void CCL_start_debug_logging(void)
{
	ccl::util_logging_start();
}

void CCL_logging_verbosity_set(int verbosity)
{
	ccl::util_logging_verbosity_set(verbosity);
}
