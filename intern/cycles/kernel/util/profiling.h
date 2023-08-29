/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#ifndef __KERNEL_GPU__
#  include "util/profiling.h"
#endif

CCL_NAMESPACE_BEGIN

#ifndef __KERNEL_GPU__
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
#endif /* !__KERNEL_GPU__ */

CCL_NAMESPACE_END
