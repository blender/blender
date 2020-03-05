/*
 * Copyright 2011-2017 Blender Foundation
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

#include "kernel/kernel_compat_opencl.h"  // PRECOMPILED
#include "kernel/split/kernel_split_common.h"  // PRECOMPILED

#include "kernel/kernels/opencl/kernel_data_init.cl"
#include "kernel/kernels/opencl/kernel_path_init.cl"
#include "kernel/kernels/opencl/kernel_state_buffer_size.cl"
#include "kernel/kernels/opencl/kernel_scene_intersect.cl"
#include "kernel/kernels/opencl/kernel_queue_enqueue.cl"
#include "kernel/kernels/opencl/kernel_shader_setup.cl"
#include "kernel/kernels/opencl/kernel_shader_sort.cl"
#include "kernel/kernels/opencl/kernel_enqueue_inactive.cl"
#include "kernel/kernels/opencl/kernel_next_iteration_setup.cl"
#include "kernel/kernels/opencl/kernel_indirect_subsurface.cl"
#include "kernel/kernels/opencl/kernel_buffer_update.cl"
#include "kernel/kernels/opencl/kernel_adaptive_stopping.cl"
#include "kernel/kernels/opencl/kernel_adaptive_filter_x.cl"
#include "kernel/kernels/opencl/kernel_adaptive_filter_y.cl"
#include "kernel/kernels/opencl/kernel_adaptive_adjust_samples.cl"
