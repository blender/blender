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

typedef struct TileInfo {
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
} TileInfo;

#ifdef __KERNEL_OPENCL__
#  define CCL_FILTER_TILE_INFO ccl_global TileInfo* tile_info,  \
                               ccl_global float *tile_buffer_1, \
                               ccl_global float *tile_buffer_2, \
                               ccl_global float *tile_buffer_3, \
                               ccl_global float *tile_buffer_4, \
                               ccl_global float *tile_buffer_5, \
                               ccl_global float *tile_buffer_6, \
                               ccl_global float *tile_buffer_7, \
                               ccl_global float *tile_buffer_8, \
                               ccl_global float *tile_buffer_9
#  define CCL_FILTER_TILE_INFO_ARG tile_info, \
                                   tile_buffer_1, tile_buffer_2, tile_buffer_3, \
                                   tile_buffer_4, tile_buffer_5, tile_buffer_6, \
                                   tile_buffer_7, tile_buffer_8, tile_buffer_9
#  define ccl_get_tile_buffer(id) (id == 0 ? tile_buffer_1 \
                                   : id == 1 ? tile_buffer_2 \
                                   : id == 2 ? tile_buffer_3 \
                                   : id == 3 ? tile_buffer_4 \
                                   : id == 4 ? tile_buffer_5 \
                                   : id == 5 ? tile_buffer_6 \
                                   : id == 6 ? tile_buffer_7 \
                                   : id == 7 ? tile_buffer_8 \
                                   : tile_buffer_9)
#else
#  ifdef __KERNEL_CUDA__
#    define CCL_FILTER_TILE_INFO ccl_global TileInfo* tile_info
#  else
#    define CCL_FILTER_TILE_INFO TileInfo* tile_info
#  endif
#  define ccl_get_tile_buffer(id) (tile_info->buffers[id])
#endif

#endif /* __FILTER_DEFINES_H__*/
