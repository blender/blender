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

/* CUDA kernel entry points */

#ifdef __CUDA_ARCH__

#include "kernel_config.h"

#include "kernel/kernel_compat_cuda.h"

#include "kernel/filter/filter_kernel.h"

/* kernels */

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_divide_shadow(int sample,
                                 TileInfo *tile_info,
                                 float *unfilteredA,
                                 float *unfilteredB,
                                 float *sampleVariance,
                                 float *sampleVarianceV,
                                 float *bufferVariance,
                                 int4 prefilter_rect,
                                 int buffer_pass_stride,
                                 int buffer_denoising_offset)
{
	int x = prefilter_rect.x + blockDim.x*blockIdx.x + threadIdx.x;
	int y = prefilter_rect.y + blockDim.y*blockIdx.y + threadIdx.y;
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		kernel_filter_divide_shadow(sample,
		                            tile_info,
		                            x, y,
		                            unfilteredA,
		                            unfilteredB,
		                            sampleVariance,
		                            sampleVarianceV,
		                            bufferVariance,
		                            prefilter_rect,
		                            buffer_pass_stride,
		                            buffer_denoising_offset);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_get_feature(int sample,
                               TileInfo *tile_info,
                               int m_offset,
                               int v_offset,
                               float *mean,
                               float *variance,
                               int4 prefilter_rect,
                               int buffer_pass_stride,
                               int buffer_denoising_offset)
{
	int x = prefilter_rect.x + blockDim.x*blockIdx.x + threadIdx.x;
	int y = prefilter_rect.y + blockDim.y*blockIdx.y + threadIdx.y;
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		kernel_filter_get_feature(sample,
		                          tile_info,
		                          m_offset, v_offset,
		                          x, y,
		                          mean, variance,
		                          prefilter_rect,
		                          buffer_pass_stride,
		                          buffer_denoising_offset);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_detect_outliers(float *image,
                                   float *variance,
                                   float *depth,
                                   float *output,
                                   int4 prefilter_rect,
                                   int pass_stride)
{
	int x = prefilter_rect.x + blockDim.x*blockIdx.x + threadIdx.x;
	int y = prefilter_rect.y + blockDim.y*blockIdx.y + threadIdx.y;
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		kernel_filter_detect_outliers(x, y, image, variance, depth, output, prefilter_rect, pass_stride);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_combine_halves(float *mean, float *variance, float *a, float *b, int4 prefilter_rect, int r)
{
	int x = prefilter_rect.x + blockDim.x*blockIdx.x + threadIdx.x;
	int y = prefilter_rect.y + blockDim.y*blockIdx.y + threadIdx.y;
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		kernel_filter_combine_halves(x, y, mean, variance, a, b, prefilter_rect, r);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_construct_transform(float const* __restrict__ buffer,
                                       float *transform, int *rank,
                                       int4 filter_area, int4 rect,
                                       int radius, float pca_threshold,
                                       int pass_stride)
{
	int x = blockDim.x*blockIdx.x + threadIdx.x;
	int y = blockDim.y*blockIdx.y + threadIdx.y;
	if(x < filter_area.z && y < filter_area.w) {
		int *l_rank = rank + y*filter_area.z + x;
		float *l_transform = transform + y*filter_area.z + x;
		kernel_filter_construct_transform(buffer,
		                                  x + filter_area.x, y + filter_area.y,
		                                  rect, pass_stride,
		                                  l_transform, l_rank,
		                                  radius, pca_threshold,
		                                  filter_area.z*filter_area.w,
		                                  threadIdx.y*blockDim.x + threadIdx.x);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_nlm_calc_difference(const float *ccl_restrict weight_image,
                                       const float *ccl_restrict variance_image,
                                       float *difference_image,
                                       int w,
                                       int h,
                                       int stride,
                                       int pass_stride,
                                       int r,
                                       int channel_offset,
                                       float a,
                                       float k_2)
{
	int4 co, rect;
	int ofs;
	if(get_nlm_coords(w, h, r, pass_stride, &rect, &co, &ofs)) {
		kernel_filter_nlm_calc_difference(co.x, co.y, co.z, co.w,
		                                  weight_image,
		                                  variance_image,
		                                  difference_image + ofs,
		                                  rect, stride,
		                                  channel_offset, a, k_2);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_nlm_blur(const float *ccl_restrict difference_image,
                            float *out_image,
                            int w,
                            int h,
                            int stride,
                            int pass_stride,
                            int r,
                            int f)
{
	int4 co, rect;
	int ofs;
	if(get_nlm_coords(w, h, r, pass_stride, &rect, &co, &ofs)) {
		kernel_filter_nlm_blur(co.x, co.y,
		                       difference_image + ofs,
		                       out_image + ofs,
		                       rect, stride, f);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_nlm_calc_weight(const float *ccl_restrict difference_image,
                                   float *out_image,
                                   int w,
                                   int h,
                                   int stride,
                                   int pass_stride,
                                   int r,
                                   int f)
{
	int4 co, rect;
	int ofs;
	if(get_nlm_coords(w, h, r, pass_stride, &rect, &co, &ofs)) {
		kernel_filter_nlm_calc_weight(co.x, co.y,
		                              difference_image + ofs,
		                              out_image + ofs,
		                              rect, stride, f);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_nlm_update_output(const float *ccl_restrict difference_image,
                                     const float *ccl_restrict image,
                                     float *out_image,
                                     float *accum_image,
                                     int w,
                                     int h,
                                     int stride,
                                     int pass_stride,
                                     int r,
                                     int f)
{
	int4 co, rect;
	int ofs;
	if(get_nlm_coords(w, h, r, pass_stride, &rect, &co, &ofs)) {
		kernel_filter_nlm_update_output(co.x, co.y, co.z, co.w,
		                                difference_image + ofs,
		                                image,
		                                out_image,
		                                accum_image,
		                                rect, stride, f);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_nlm_normalize(float *out_image,
                                 const float *ccl_restrict accum_image,
                                 int w,
                                 int h,
                                 int stride)
{
	int x = blockDim.x*blockIdx.x + threadIdx.x;
	int y = blockDim.y*blockIdx.y + threadIdx.y;
	if(x < w && y < h) {
		kernel_filter_nlm_normalize(x, y, out_image, accum_image, stride);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_nlm_construct_gramian(const float *ccl_restrict difference_image,
                                         const float *ccl_restrict buffer,
                                         float const* __restrict__ transform,
                                         int *rank,
                                         float *XtWX,
                                         float3 *XtWY,
                                         int4 filter_window,
                                         int w,
                                         int h,
                                         int stride,
                                         int pass_stride,
                                         int r,
                                         int f)
{
	int4 co, rect;
	int ofs;
	if(get_nlm_coords_window(w, h, r, pass_stride, &rect, &co, &ofs, filter_window)) {
		kernel_filter_nlm_construct_gramian(co.x, co.y,
		                                    co.z, co.w,
		                                    difference_image + ofs,
		                                    buffer,
		                                    transform, rank,
		                                    XtWX, XtWY,
		                                    rect, filter_window,
		                                    stride, f,
		                                    pass_stride,
		                                    threadIdx.y*blockDim.x + threadIdx.x);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_finalize(float *buffer,
                            int *rank,
                            float *XtWX,
                            float3 *XtWY,
                            int4 filter_area,
                            int4 buffer_params,
                            int sample)
{
	int x = blockDim.x*blockIdx.x + threadIdx.x;
	int y = blockDim.y*blockIdx.y + threadIdx.y;
	if(x < filter_area.z && y < filter_area.w) {
		int storage_ofs = y*filter_area.z+x;
		rank += storage_ofs;
		XtWX += storage_ofs;
		XtWY += storage_ofs;
		kernel_filter_finalize(x, y, buffer, rank,
		                       filter_area.z*filter_area.w,
		                       XtWX, XtWY,
		                       buffer_params, sample);
	}
}

#endif

