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

#ifndef __UTIL_TIME_H__
#define __UTIL_TIME_H__

CCL_NAMESPACE_BEGIN

/* Give current time in seconds in double precision, with good accuracy. */

double time_dt();

/* Sleep for the specified number of seconds */

void time_sleep(double t);

class scoped_timer {
public:
	explicit scoped_timer(double *value = NULL) : value_(value)
	{
		time_start_ = time_dt();
	}

	~scoped_timer()
	{
		if(value_ != NULL) {
			*value_ = get_time();
		}
	}

	double get_start() const
	{
		return time_start_;
	}

	double get_time() const
	{
		return time_dt() - time_start_;
	}

protected:
	double *value_;
	double time_start_;
};

CCL_NAMESPACE_END

#endif
