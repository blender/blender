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

#ifndef __FILTER_DEFINES_H__
#define __FILTER_DEFINES_H__

#define DENOISE_FEATURES 10
#define TRANSFORM_SIZE (DENOISE_FEATURES*DENOISE_FEATURES)
#define XTWX_SIZE      (((DENOISE_FEATURES+1)*(DENOISE_FEATURES+2))/2)
#define XTWY_SIZE      (DENOISE_FEATURES+1)

typedef struct TilesInfo {
	int offsets[9];
	int strides[9];
	int x[4];
	int y[4];
	/* TODO(lukas): CUDA doesn't have uint64_t... */
#ifdef __KERNEL_OPENCL__
	ccl_global float *buffers[9];
#else
	long long int buffers[9];
#endif
} TilesInfo;

#endif /* __FILTER_DEFINES_H__*/
