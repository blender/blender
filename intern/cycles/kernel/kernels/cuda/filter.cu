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
kernel_cuda_filter_copy_input(float *buffer,
                              CCL_FILTER_TILE_INFO,
                              int4 prefilter_rect,
                              int buffer_pass_stride)
{
	int x = prefilter_rect.x + blockDim.x*blockIdx.x + threadIdx.x;
	int y = prefilter_rect.y + blockDim.y*blockIdx.y + threadIdx.y;
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		int xtile = (x < tile_info->x[1]) ? 0 : ((x < tile_info->x[2]) ? 1 : 2);
		int ytile = (y < tile_info->y[1]) ? 0 : ((y < tile_info->y[2]) ? 1 : 2);
		int itile = ytile * 3 + xtile;
		float *const in = ((float *)ccl_get_tile_buffer(itile)) +
			(tile_info->offsets[itile] + y * tile_info->strides[itile] + x) * buffer_pass_stride;
		buffer += ((y - prefilter_rect.y) * (prefilter_rect.z - prefilter_rect.x) + (x - prefilter_rect.x)) * buffer_pass_stride;
		for (int i = 0; i < buffer_pass_stride; ++i)
			buffer[i] = in[i];
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_convert_to_rgb(float *rgb, float *buf, int sw, int sh, int stride, int pass_stride, int3 pass_offset, int num_inputs, int num_samples)
{
	int x = blockDim.x*blockIdx.x + threadIdx.x;
	int y = blockDim.y*blockIdx.y + threadIdx.y;
	if(x < sw && y < sh) {
		if (num_inputs > 0) {
			float *in = buf + x * pass_stride + (y * stride + pass_offset.x) / sizeof(float);
			float *out = rgb + (x + y * sw) * 3;
			out[0] = clamp(in[0] / num_samples, 0.0f, 10000.0f);
			out[1] = clamp(in[1] / num_samples, 0.0f, 10000.0f);
			out[2] = clamp(in[2] / num_samples, 0.0f, 10000.0f);
		}
		if (num_inputs > 1) {
			float *in = buf + x * pass_stride + (y * stride + pass_offset.y) / sizeof(float);
			float *out = rgb + (x + y * sw) * 3 + (sw * sh) * 3;
			out[0] = in[0] / num_samples;
			out[1] = in[1] / num_samples;
			out[2] = in[2] / num_samples;
		}
		if (num_inputs > 2) {
			float *in = buf + x * pass_stride + (y * stride + pass_offset.z) / sizeof(float);
			float *out = rgb + (x + y * sw) * 3 + (sw * sh * 2) * 3;
			out[0] = in[0] / num_samples;
			out[1] = in[1] / num_samples;
			out[2] = in[2] / num_samples;
		}
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_convert_from_rgb(float *rgb, float *buf, int ix, int iy, int iw, int ih, int sx, int sy, int sw, int sh, int offset, int stride, int pass_stride, int num_samples)
{
	int x = blockDim.x*blockIdx.x + threadIdx.x;
	int y = blockDim.y*blockIdx.y + threadIdx.y;
	if(x < sw && y < sh) {
		float *in = rgb + ((ix + x) + (iy + y) * iw) * 3;
		float *out = buf + (offset + (sx + x) + (sy + y) * stride) * pass_stride;
		out[0] = in[0] * num_samples;
		out[1] = in[1] * num_samples;
		out[2] = in[2] * num_samples;
	}
}


extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_divide_shadow(int sample,
                                 CCL_FILTER_TILE_INFO,
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
                               CCL_FILTER_TILE_INFO,
                               int m_offset,
                               int v_offset,
                               float *mean,
                               float *variance,
                               float scale,
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
		                          scale,
		                          prefilter_rect,
		                          buffer_pass_stride,
		                          buffer_denoising_offset);
	}
}

extern "C" __global__ void
CUDA_LAUNCH_BOUNDS(CUDA_THREADS_BLOCK_WIDTH, CUDA_KERNEL_MAX_REGISTERS)
kernel_cuda_filter_write_feature(int sample,
                                 int4 buffer_params,
                                 int4 filter_area,
                                 float *from,
                                 float *buffer,
                                 int out_offset,
                                 int4 prefilter_rect)
{
	int x = blockDim.x*blockIdx.x + threadIdx.x;
	int y = blockDim.y*blockIdx.y + threadIdx.y;
	if(x < filter_area.z && y < filter_area.w) {
		kernel_filter_write_feature(sample,
	                                x + filter_area.x,
	                                y + filter_area.y,
	                                buffer_params,
	                                from,
	                                buffer,
	                                out_offset,
	                                prefilter_rect);
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
                                       CCL_FILTER_TILE_INFO,
                                       float *transform, int *rank,
                                       int4 filter_area, int4 rect,
                                       int radius, float pca_threshold,
                                       int pass_stride, int frame_stride,
                                       bool use_time)
{
	int x = blockDim.x*blockIdx.x + threadIdx.x;
	int y = blockDim.y*blockIdx.y + threadIdx.y;
	if(x < filter_area.z && y < filter_area.w) {
		int *l_rank = rank + y*filter_area.z + x;
		float *l_transform = transform + y*filter_area.z + x;
		kernel_filter_construct_transform(buffer,
		                                  tile_info,
		                                  x + filter_area.x, y + filter_area.y,
		                                  rect,
		                                  pass_stride, frame_stride,
		                                  use_time,
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
                                       const float *ccl_restrict scale_image,
                                       float *difference_image,
                                       int w,
                                       int h,
                                       int stride,
                                       int pass_stride,
                                       int r,
                                       int channel_offset,
                                       int frame_offset,
                                       float a,
                                       float k_2)
{
	int4 co, rect;
	int ofs;
	if(get_nlm_coords(w, h, r, pass_stride, &rect, &co, &ofs)) {
		kernel_filter_nlm_calc_difference(co.x, co.y, co.z, co.w,
		                                  weight_image,
		                                  variance_image,
		                                  scale_image,
		                                  difference_image + ofs,
		                                  rect, stride,
		                                  channel_offset,
		                                  frame_offset,
		                                  a, k_2);
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
                                     int channel_offset,
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
		                                rect,
		                                channel_offset,
		                                stride, f);
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
kernel_cuda_filter_nlm_construct_gramian(int t,
                                         const float *ccl_restrict difference_image,
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
                                         int f,
                                         int frame_offset,
                                         bool use_time)
{
	int4 co, rect;
	int ofs;
	if(get_nlm_coords_window(w, h, r, pass_stride, &rect, &co, &ofs, filter_window)) {
		kernel_filter_nlm_construct_gramian(co.x, co.y,
		                                    co.z, co.w,
		                                    t,
		                                    difference_image + ofs,
		                                    buffer,
		                                    transform, rank,
		                                    XtWX, XtWY,
		                                    rect, filter_window,
		                                    stride, f,
		                                    pass_stride,
		                                    frame_offset,
		                                    use_time,
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

