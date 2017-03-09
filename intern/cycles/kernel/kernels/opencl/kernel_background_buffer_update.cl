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

#include "kernel_compat_opencl.h"
#include "split/kernel_split_common.h"
#include "split/kernel_background_buffer_update.h"

__kernel void kernel_ocl_path_trace_background_buffer_update(
        KernelGlobals *kg,
        ccl_constant KernelData *data)
{
	kernel_background_buffer_update(kg);
}
