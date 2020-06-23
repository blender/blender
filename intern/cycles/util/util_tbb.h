/*
 * Copyright 2011-2020 Blender Foundation
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

#ifndef __UTIL_TBB_H__
#define __UTIL_TBB_H__

#define TBB_SUPPRESS_DEPRECATED_MESSAGES 1
#include <tbb/tbb.h>

#if TBB_INTERFACE_VERSION_MAJOR >= 10
#  define WITH_TBB_GLOBAL_CONTROL
#endif

CCL_NAMESPACE_BEGIN

using tbb::blocked_range;
using tbb::enumerable_thread_specific;
using tbb::parallel_for;

CCL_NAMESPACE_END

#endif /* __UTIL_TBB_H__ */
