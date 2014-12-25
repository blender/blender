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

#ifndef __UTIL_LOGGING_H__
#define __UTIL_LOGGING_H__

#if defined(WITH_CYCLES_LOGGING) && !defined(__KERNEL_GPU__)
#  include <glog/logging.h>
#else
#  include <iostream>
#endif

CCL_NAMESPACE_BEGIN

#if !defined(WITH_CYCLES_LOGGING) || defined(__KERNEL_GPU__)
class StubStream : public std::ostream {
 public:
	StubStream() : std::ostream(NULL) { }
};

class LogMessageVoidify {
public:
	LogMessageVoidify() { }
	void operator&(::std::ostream&) { }
};

#  define LOG_SUPPRESS() (true) ? (void) 0 : LogMessageVoidify() & StubStream()
#  define LOG(severity) LOG_SUPPRESS()
#  define VLOG(severity) LOG_SUPPRESS()

#endif

struct float3;

std::ostream& operator <<(std::ostream &os,
                          const float3 &value);

CCL_NAMESPACE_END

#endif /* __UTIL_LOGGING_H__ */
