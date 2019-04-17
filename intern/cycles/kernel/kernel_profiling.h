/*
 * Copyright 2011-2018 Blender Foundation
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

#ifndef __KERNEL_PROFILING_H__
#define __KERNEL_PROFILING_H__

#ifdef __KERNEL_CPU__
#  include "util/util_profiling.h"
#endif

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_CPU__
#  define PROFILING_INIT(kg, event) ProfilingHelper profiling_helper(&kg->profiler, event)
#  define PROFILING_EVENT(event) profiling_helper.set_event(event)
#  define PROFILING_SHADER(shader) \
    if ((shader) != SHADER_NONE) { \
      profiling_helper.set_shader((shader)&SHADER_MASK); \
    }
#  define PROFILING_OBJECT(object) \
    if ((object) != PRIM_NONE) { \
      profiling_helper.set_object(object); \
    }
#else
#  define PROFILING_INIT(kg, event)
#  define PROFILING_EVENT(event)
#  define PROFILING_SHADER(shader)
#  define PROFILING_OBJECT(object)
#endif /* __KERNEL_CPU__ */

CCL_NAMESPACE_END

#endif /* __KERNEL_PROFILING_H__ */
