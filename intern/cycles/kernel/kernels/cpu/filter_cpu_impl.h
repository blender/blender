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

/* Templated common implementation part of all CPU kernels.
 *
 * The idea is that particular .cpp files sets needed optimization flags and
 * simply includes this file without worry of copying actual implementation over.
 */

#include "kernel/kernel_compat_cpu.h"

#include "kernel/filter/filter_kernel.h"

#ifdef KERNEL_STUB
#  include "util/util_debug.h"
#  define STUB_ASSERT(arch, name) assert(!(#name " kernel stub for architecture " #arch " was called!"))
#endif

CCL_NAMESPACE_BEGIN


/* Denoise filter */

void KERNEL_FUNCTION_FULL_NAME(filter_divide_shadow)(int sample,
                                                     TilesInfo *tiles,
                                                     int x,
                                                     int y,
                                                     float *unfilteredA,
                                                     float *unfilteredB,
                                                     float *sampleVariance,
                                                     float *sampleVarianceV,
                                                     float *bufferVariance,
                                                     int* prefilter_rect,
                                                     int buffer_pass_stride,
                                                     int buffer_denoising_offset,
                                                     bool use_split_variance)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_divide_shadow);
#else
	kernel_filter_divide_shadow(sample, tiles,
	                            x, y,
	                            unfilteredA,
	                            unfilteredB,
	                            sampleVariance,
	                            sampleVarianceV,
	                            bufferVariance,
	                            load_int4(prefilter_rect),
	                            buffer_pass_stride,
	                            buffer_denoising_offset,
	                            use_split_variance);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_get_feature)(int sample,
                                                   TilesInfo *tiles,
                                                   int m_offset,
                                                   int v_offset,
                                                   int x,
                                                   int y,
                                                   float *mean, float *variance,
                                                   int* prefilter_rect,
                                                   int buffer_pass_stride,
                                                   int buffer_denoising_offset,
                                                   bool use_split_variance)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_get_feature);
#else
	kernel_filter_get_feature(sample, tiles,
	                          m_offset, v_offset,
	                          x, y,
	                          mean, variance,
	                          load_int4(prefilter_rect),
	                          buffer_pass_stride,
	                          buffer_denoising_offset,
	                          use_split_variance);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_combine_halves)(int x, int y,
                                                      float *mean,
                                                      float *variance,
                                                      float *a,
                                                      float *b,
                                                      int* prefilter_rect,
                                                      int r)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_combine_halves);
#else
	kernel_filter_combine_halves(x, y, mean, variance, a, b, load_int4(prefilter_rect), r);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_construct_transform)(float* buffer,
                                                           int x,
                                                           int y,
                                                           int storage_ofs,
                                                           float *transform,
                                                           int *rank,
                                                           int* prefilter_rect,
                                                           int pass_stride,
                                                           int radius,
                                                           float pca_threshold)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_construct_transform);
#else
  rank += storage_ofs;
  transform += storage_ofs*TRANSFORM_SIZE;
	kernel_filter_construct_transform(buffer,
	                                  x, y,
	                                  load_int4(prefilter_rect),
	                                  pass_stride,
	                                  transform,
	                                  rank,
	                                  radius,
	                                  pca_threshold);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_calc_difference)(int dx,
                                                           int dy,
                                                           float *weightImage,
                                                           float *variance,
                                                           float *differenceImage,
                                                           int *rect,
                                                           int w,
                                                           int channel_offset,
                                                           float a,
                                                           float k_2)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_calc_difference);
#else
	kernel_filter_nlm_calc_difference(dx, dy, weightImage, variance, differenceImage, load_int4(rect), w, channel_offset, a, k_2);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_blur)(float *differenceImage,
                                                float *outImage,
                                                int *rect,
                                                int w,
                                                int f)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_blur);
#else
	kernel_filter_nlm_blur(differenceImage, outImage, load_int4(rect), w, f);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_calc_weight)(float *differenceImage,
                                                       float *outImage,
                                                       int *rect,
                                                       int w,
                                                       int f)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_calc_weight);
#else
	kernel_filter_nlm_calc_weight(differenceImage, outImage, load_int4(rect), w, f);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_update_output)(int dx,
                                                         int dy,
                                                         float *differenceImage,
                                                         float *image,
                                                         float *outImage,
                                                         float *accumImage,
                                                         int *rect,
                                                         int w,
                                                         int f)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_update_output);
#else
	kernel_filter_nlm_update_output(dx, dy, differenceImage, image, outImage, accumImage, load_int4(rect), w, f);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_construct_gramian)(int dx,
                                                             int dy,
                                                             float *differenceImage,
                                                             float *buffer,
                                                             float *color_pass,
                                                             float *variance_pass,
                                                             float *transform,
                                                             int *rank,
                                                             float *XtWX,
                                                             float3 *XtWY,
                                                             int *rect,
                                                             int *filter_rect,
                                                             int w,
                                                             int h,
                                                             int f,
                                                             int pass_stride)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_construct_gramian);
#else
    kernel_filter_nlm_construct_gramian(dx, dy, differenceImage, buffer, color_pass, variance_pass, transform, rank, XtWX, XtWY, load_int4(rect), load_int4(filter_rect), w, h, f, pass_stride);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_nlm_normalize)(float *outImage,
                                                     float *accumImage,
                                                     int *rect,
                                                     int w)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_nlm_normalize);
#else
	kernel_filter_nlm_normalize(outImage, accumImage, load_int4(rect), w);
#endif
}

void KERNEL_FUNCTION_FULL_NAME(filter_finalize)(int x,
                                                int y,
                                                int storage_ofs,
                                                int w,
                                                int h,
                                                float *buffer,
                                                int *rank,
                                                float *XtWX,
                                                float3 *XtWY,
                                                int *buffer_params,
                                                int sample)
{
#ifdef KERNEL_STUB
	STUB_ASSERT(KERNEL_ARCH, filter_finalize);
#else
	XtWX += storage_ofs*XTWX_SIZE;
	XtWY += storage_ofs*XTWY_SIZE;
	rank += storage_ofs;
	kernel_filter_finalize(x, y, w, h, buffer, rank, 1, XtWX, XtWY, load_int4(buffer_params), sample);
#endif
}

#undef KERNEL_STUB
#undef STUB_ASSERT
#undef KERNEL_ARCH

CCL_NAMESPACE_END
