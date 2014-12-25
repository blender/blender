/*
 * Copyright 2011-2013 Blender Foundation
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

#include <stdlib.h>

#include "util_time.h"

#ifdef _WIN32

#include <windows.h>

CCL_NAMESPACE_BEGIN

double time_dt()
{
	__int64 frequency, counter;

	QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
	QueryPerformanceCounter((LARGE_INTEGER*)&counter);

	return (double)counter/(double)frequency;
}

void time_sleep(double t)
{
	Sleep((int)(t*1000));
}

CCL_NAMESPACE_END

#else

#include <sys/time.h>
#include <unistd.h>

CCL_NAMESPACE_BEGIN

double time_dt()
{
	struct timeval now;
	gettimeofday(&now, NULL);

	return now.tv_sec + now.tv_usec*1e-6;
}

/* sleep t seconds */
void time_sleep(double t)
{
	/* get whole seconds */
	int s = (int)t;

	if(s >= 1) {
		sleep(s);

		/* adjust parameter to remove whole seconds */
		t -= s;
	}

	/* get microseconds */
	int us = (int)(t * 1e6);
	if (us > 0)
		usleep(us);
}

CCL_NAMESPACE_END

#endif

