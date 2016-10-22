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

#include "kernel_compat_opencl.h"
#include "kernel_math.h"
#include "kernel_types.h"
#include "kernel_globals.h"
#include "kernel_image_opencl.h"

#include "util_atomic.h"

#include "kernel_random.h"
#include "kernel_projection.h"
#include "kernel_montecarlo.h"
#include "kernel_differential.h"
#include "kernel_camera.h"

#include "geom/geom.h"
#include "bvh/bvh.h"

#include "kernel_accumulate.h"
#include "kernel_shader.h"
#include "kernel_light.h"
#include "kernel_passes.h"

#ifdef __SUBSURFACE__
#include "kernel_subsurface.h"
#endif

#ifdef __VOLUME__
#include "kernel_volume.h"
#endif

#include "kernel_path_state.h"
#include "kernel_shadow.h"
#include "kernel_emission.h"
#include "kernel_path_common.h"
#include "kernel_path_surface.h"
#include "kernel_path_volume.h"

#ifdef __KERNEL_DEBUG__
#include "kernel_debug.h"
#endif

#include "kernel_queues.h"
#include "kernel_work_stealing.h"

#endif  /* __KERNEL_SPLIT_H__ */
