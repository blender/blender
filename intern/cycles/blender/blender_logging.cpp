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
 * limitations under the License
 */

#include "CCL_api.h"

#include <stdio.h>

#include "util_logging.h"

#ifdef _MSC_VER
#  define snprintf _snprintf
#endif

void CCL_init_logging(const char *argv0)
{
#ifdef WITH_CYCLES_LOGGING
	/* Make it so FATAL messages are always print into console. */
	char severity_fatal[32];
	snprintf(severity_fatal, sizeof(severity_fatal), "%d",
	         google::GLOG_FATAL);

	google::InitGoogleLogging(argv0);
	google::SetCommandLineOption("logtostderr", "1");
	google::SetCommandLineOption("v", "0");
	google::SetCommandLineOption("stderrthreshold", severity_fatal);
	google::SetCommandLineOption("minloglevel", severity_fatal);
#else
	(void) argv0;
#endif
}

void CCL_start_debug_logging(void)
{
#ifdef WITH_CYCLES_LOGGING
	google::SetCommandLineOption("logtostderr", "1");
	google::SetCommandLineOption("v", "2");
	google::SetCommandLineOption("stderrthreshold", "1");
	google::SetCommandLineOption("minloglevel", "0");
#endif
}

void CCL_logging_verbosity_set(int verbosity)
{
#ifdef WITH_CYCLES_LOGGING
	char val[10];
	snprintf(val, sizeof(val), "%d", verbosity);

	google::SetCommandLineOption("v", val);
#else
	(void) verbosity;
#endif
}
