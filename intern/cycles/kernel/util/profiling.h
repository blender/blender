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

#pragma once

#ifdef __KERNEL_CPU__
#  include "util/profiling.h"
#endif

CCL_NAMESPACE_BEGIN

#ifdef __KERNEL_CPU__
#  define PROFILING_INIT(kg, event) \
    ProfilingHelper profiling_helper((ProfilingState *)&kg->profiler, event)
#  define PROFILING_EVENT(event) profiling_helper.set_event(event)
#  define PROFILING_INIT_FOR_SHADER(kg, event) \
    ProfilingWithShaderHelper profiling_helper((ProfilingState *)&kg->profiler, event)
#  define PROFILING_SHADER(object, shader) \
    profiling_helper.set_shader(object, (shader)&SHADER_MASK);
#else
#  define PROFILING_INIT(kg, event)
#  define PROFILING_EVENT(event)
#  define PROFILING_INIT_FOR_SHADER(kg, event)
#  define PROFILING_SHADER(object, shader)
#endif /* __KERNEL_CPU__ */

CCL_NAMESPACE_END
