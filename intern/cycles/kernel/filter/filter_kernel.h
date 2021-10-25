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

#include "util/util_color.h"
#include "util/util_math.h"
#include "util/util_math_fast.h"
#include "util/util_texture.h"

#include "util/util_atomic.h"
#include "util/util_math_matrix.h"

#include "kernel/filter/filter_defines.h"

#include "kernel/filter/filter_features.h"
#ifdef __KERNEL_SSE3__
#  include "kernel/filter/filter_features_sse.h"
#endif

#include "kernel/filter/filter_prefilter.h"

#ifdef __KERNEL_GPU__
#  include "kernel/filter/filter_transform_gpu.h"
#else
#  ifdef __KERNEL_SSE3__
#    include "kernel/filter/filter_transform_sse.h"
#  else
#    include "kernel/filter/filter_transform.h"
#  endif
#endif

#include "kernel/filter/filter_reconstruction.h"

#ifdef __KERNEL_CPU__
#  include "kernel/filter/filter_nlm_cpu.h"
#else
#  include "kernel/filter/filter_nlm_gpu.h"
#endif
