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
 * limitations under the License
 */

#ifndef __UTIL_TIME_H__
#define __UTIL_TIME_H__

CCL_NAMESPACE_BEGIN

/* Give current time in seconds in double precision, with good accuracy. */

double time_dt();

/* Sleep for the specified number of seconds */

void time_sleep(double t);

CCL_NAMESPACE_END

#endif

