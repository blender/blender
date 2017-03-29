/*
 * Copyright 2011-2015 Blender Foundation
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

#ifndef  __KERNEL_SPLIT_H__
#define  __KERNEL_SPLIT_H__

#include "kernel/kernel_math.h"
#include "kernel/kernel_types.h"

#include "kernel/split/kernel_split_data.h"

#include "kernel/kernel_globals.h"

#ifdef __OSL__
#  include "kernel/osl/osl_shader.h"
#endif

#ifdef __KERNEL_OPENCL__
#  include "kernel/kernel_image_opencl.h"
#endif
#ifdef __KERNEL_CPU__
#  include "kernel/kernels/cpu/kernel_cpu_image.h"
#endif

#include "util/util_atomic.h"

#include "kernel/kernel_random.h"
#include "kernel/kernel_projection.h"
#include "kernel/kernel_montecarlo.h"
#include "kernel/kernel_differential.h"
#include "kernel/kernel_camera.h"

#include "kernel/geom/geom.h"
#include "kernel/bvh/bvh.h"

#include "kernel/kernel_accumulate.h"
#include "kernel/kernel_shader.h"
#include "kernel/kernel_light.h"
#include "kernel/kernel_passes.h"

#ifdef __SUBSURFACE__
#  include "kernel/kernel_subsurface.h"
#endif

#ifdef __VOLUME__
#  include "kernel/kernel_volume.h"
#endif

#include "kernel/kernel_path_state.h"
#include "kernel/kernel_shadow.h"
#include "kernel/kernel_emission.h"
#include "kernel/kernel_path_common.h"
#include "kernel/kernel_path_surface.h"
#include "kernel/kernel_path_volume.h"
#include "kernel/kernel_path_subsurface.h"

#ifdef __KERNEL_DEBUG__
#  include "kernel/kernel_debug.h"
#endif

#include "kernel/kernel_queues.h"
#include "kernel/kernel_work_stealing.h"

#endif  /* __KERNEL_SPLIT_H__ */
