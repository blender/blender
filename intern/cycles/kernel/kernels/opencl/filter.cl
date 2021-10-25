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

/* OpenCL kernel entry points */

#include "kernel/kernel_compat_opencl.h"

#include "kernel/filter/filter_kernel.h"

/* kernels */

__kernel void kernel_ocl_filter_divide_shadow(int sample,
                                              ccl_global TilesInfo *tiles,
                                              ccl_global float *unfilteredA,
                                              ccl_global float *unfilteredB,
                                              ccl_global float *sampleVariance,
                                              ccl_global float *sampleVarianceV,
                                              ccl_global float *bufferVariance,
                                              int4 prefilter_rect,
                                              int buffer_pass_stride,
                                              int buffer_denoising_offset,
                                              char use_split_variance)
{
	int x = prefilter_rect.x + get_global_id(0);
	int y = prefilter_rect.y + get_global_id(1);
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		kernel_filter_divide_shadow(sample,
		                            tiles,
		                            x, y,
		                            unfilteredA,
		                            unfilteredB,
		                            sampleVariance,
		                            sampleVarianceV,
		                            bufferVariance,
		                            prefilter_rect,
		                            buffer_pass_stride,
		                            buffer_denoising_offset,
		                            use_split_variance);
	}
}

__kernel void kernel_ocl_filter_get_feature(int sample,
                                            ccl_global TilesInfo *tiles,
                                            int m_offset,
                                            int v_offset,
                                            ccl_global float *mean,
                                            ccl_global float *variance,
                                            int4 prefilter_rect,
                                            int buffer_pass_stride,
                                            int buffer_denoising_offset,
                                            char use_split_variance)
{
	int x = prefilter_rect.x + get_global_id(0);
	int y = prefilter_rect.y + get_global_id(1);
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		kernel_filter_get_feature(sample,
		                          tiles,
		                          m_offset, v_offset,
		                          x, y,
		                          mean, variance,
		                          prefilter_rect,
		                          buffer_pass_stride,
		                          buffer_denoising_offset,
		                          use_split_variance);
	}
}

__kernel void kernel_ocl_filter_detect_outliers(ccl_global float *image,
                                                ccl_global float *variance,
                                                ccl_global float *depth,
                                                ccl_global float *output,
                                                int4 prefilter_rect,
                                                int pass_stride)
{
	int x = prefilter_rect.x + get_global_id(0);
	int y = prefilter_rect.y + get_global_id(1);
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		kernel_filter_detect_outliers(x, y, image, variance, depth, output, prefilter_rect, pass_stride);
	}
}

__kernel void kernel_ocl_filter_combine_halves(ccl_global float *mean,
                                               ccl_global float *variance,
                                               ccl_global float *a,
                                               ccl_global float *b,
                                               int4 prefilter_rect,
                                               int r)
{
	int x = prefilter_rect.x + get_global_id(0);
	int y = prefilter_rect.y + get_global_id(1);
	if(x < prefilter_rect.z && y < prefilter_rect.w) {
		kernel_filter_combine_halves(x, y, mean, variance, a, b, prefilter_rect, r);
	}
}

__kernel void kernel_ocl_filter_construct_transform(const ccl_global float *ccl_restrict buffer,
                                                    ccl_global float *transform,
                                                    ccl_global int *rank,
                                                    int4 filter_area,
                                                    int4 rect,
                                                    int pass_stride,
                                                    int radius,
                                                    float pca_threshold)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	if(x < filter_area.z && y < filter_area.w) {
		ccl_global int *l_rank = rank + y*filter_area.z + x;
		ccl_global float *l_transform = transform + y*filter_area.z + x;
		kernel_filter_construct_transform(buffer,
		                                  x + filter_area.x, y + filter_area.y,
		                                  rect, pass_stride,
		                                  l_transform, l_rank,
		                                  radius, pca_threshold,
		                                  filter_area.z*filter_area.w,
		                                  get_local_id(1)*get_local_size(0) + get_local_id(0));
	}
}

__kernel void kernel_ocl_filter_nlm_calc_difference(int dx,
                                                    int dy,
                                                    const ccl_global float *ccl_restrict weight_image,
                                                    const ccl_global float *ccl_restrict variance_image,
                                                    ccl_global float *difference_image,
                                                    int4 rect,
                                                    int w,
                                                    int channel_offset,
                                                    float a,
                                                    float k_2)
{
	int x = get_global_id(0) + rect.x;
	int y = get_global_id(1) + rect.y;
	if(x < rect.z && y < rect.w) {
		kernel_filter_nlm_calc_difference(x, y, dx, dy, weight_image, variance_image, difference_image, rect, w, channel_offset, a, k_2);
	}
}

__kernel void kernel_ocl_filter_nlm_blur(const ccl_global float *ccl_restrict difference_image,
                                         ccl_global float *out_image,
                                         int4 rect,
                                         int w,
                                         int f)
{
	int x = get_global_id(0) + rect.x;
	int y = get_global_id(1) + rect.y;
	if(x < rect.z && y < rect.w) {
		kernel_filter_nlm_blur(x, y, difference_image, out_image, rect, w, f);
	}
}

__kernel void kernel_ocl_filter_nlm_calc_weight(const ccl_global float *ccl_restrict difference_image,
                                                ccl_global float *out_image,
                                                int4 rect,
                                                int w,
                                                int f)
{
	int x = get_global_id(0) + rect.x;
	int y = get_global_id(1) + rect.y;
	if(x < rect.z && y < rect.w) {
		kernel_filter_nlm_calc_weight(x, y, difference_image, out_image, rect, w, f);
	}
}

__kernel void kernel_ocl_filter_nlm_update_output(int dx,
                                                  int dy,
                                                  const ccl_global float *ccl_restrict difference_image,
                                                  const ccl_global float *ccl_restrict image,
                                                  ccl_global float *out_image,
                                                  ccl_global float *accum_image,
                                                  int4 rect,
                                                  int w,
                                                  int f)
{
	int x = get_global_id(0) + rect.x;
	int y = get_global_id(1) + rect.y;
	if(x < rect.z && y < rect.w) {
		kernel_filter_nlm_update_output(x, y, dx, dy, difference_image, image, out_image, accum_image, rect, w, f);
	}
}

__kernel void kernel_ocl_filter_nlm_normalize(ccl_global float *out_image,
                                              const ccl_global float *ccl_restrict accum_image,
                                              int4 rect,
                                              int w)
{
	int x = get_global_id(0) + rect.x;
	int y = get_global_id(1) + rect.y;
	if(x < rect.z && y < rect.w) {
		kernel_filter_nlm_normalize(x, y, out_image, accum_image, rect, w);
	}
}

__kernel void kernel_ocl_filter_nlm_construct_gramian(int dx,
                                                      int dy,
                                                      const ccl_global float *ccl_restrict difference_image,
                                                      const ccl_global float *ccl_restrict buffer,
                                                      const ccl_global float *ccl_restrict transform,
                                                      ccl_global int *rank,
                                                      ccl_global float *XtWX,
                                                      ccl_global float3 *XtWY,
                                                      int4 rect,
                                                      int4 filter_rect,
                                                      int w,
                                                      int h,
                                                      int f,
                                                      int pass_stride)
{
	int x = get_global_id(0) + max(0, rect.x-filter_rect.x);
	int y = get_global_id(1) + max(0, rect.y-filter_rect.y);
	if(x < min(filter_rect.z, rect.z-filter_rect.x) && y < min(filter_rect.w, rect.w-filter_rect.y)) {
		kernel_filter_nlm_construct_gramian(x, y,
		                                    dx, dy,
		                                    difference_image,
		                                    buffer,
		                                    transform, rank,
		                                    XtWX, XtWY,
		                                    rect, filter_rect,
		                                    w, h, f,
		                                    pass_stride,
		                                    get_local_id(1)*get_local_size(0) + get_local_id(0));
	}
}

__kernel void kernel_ocl_filter_finalize(int w,
	                                     int h,
                                         ccl_global float *buffer,
                                         ccl_global int *rank,
                                         ccl_global float *XtWX,
                                         ccl_global float3 *XtWY,
                                         int4 filter_area,
                                         int4 buffer_params,
                                         int sample)
{
	int x = get_global_id(0);
	int y = get_global_id(1);
	if(x < filter_area.z && y < filter_area.w) {
		int storage_ofs = y*filter_area.z+x;
		rank += storage_ofs;
		XtWX += storage_ofs;
		XtWY += storage_ofs;
		kernel_filter_finalize(x, y, w, h, buffer, rank, filter_area.z*filter_area.w, XtWX, XtWY, buffer_params, sample);
	}
}

__kernel void kernel_ocl_filter_set_tiles(ccl_global TilesInfo* tiles,
                                          ccl_global float *buffer_1,
                                          ccl_global float *buffer_2,
                                          ccl_global float *buffer_3,
                                          ccl_global float *buffer_4,
                                          ccl_global float *buffer_5,
                                          ccl_global float *buffer_6,
                                          ccl_global float *buffer_7,
                                          ccl_global float *buffer_8,
                                          ccl_global float *buffer_9)
{
	if((get_global_id(0) == 0) && (get_global_id(1) == 0)) {
		tiles->buffers[0] = buffer_1;
		tiles->buffers[1] = buffer_2;
		tiles->buffers[2] = buffer_3;
		tiles->buffers[3] = buffer_4;
		tiles->buffers[4] = buffer_5;
		tiles->buffers[5] = buffer_6;
		tiles->buffers[6] = buffer_7;
		tiles->buffers[7] = buffer_8;
		tiles->buffers[8] = buffer_9;
	}
}
