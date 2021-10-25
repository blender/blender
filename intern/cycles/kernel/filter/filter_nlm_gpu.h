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

CCL_NAMESPACE_BEGIN

ccl_device_inline void kernel_filter_nlm_calc_difference(int x, int y,
                                                         int dx, int dy,
                                                         const ccl_global float *ccl_restrict weight_image,
                                                         const ccl_global float *ccl_restrict variance_image,
                                                         ccl_global float *difference_image,
                                                         int4 rect, int w,
                                                         int channel_offset,
                                                         float a, float k_2)
{
	float diff = 0.0f;
	int numChannels = channel_offset? 3 : 1;
	for(int c = 0; c < numChannels; c++) {
		float cdiff = weight_image[c*channel_offset + y*w+x] - weight_image[c*channel_offset + (y+dy)*w+(x+dx)];
		float pvar = variance_image[c*channel_offset + y*w+x];
		float qvar = variance_image[c*channel_offset + (y+dy)*w+(x+dx)];
		diff += (cdiff*cdiff - a*(pvar + min(pvar, qvar))) / (1e-8f + k_2*(pvar+qvar));
	}
	if(numChannels > 1) {
		diff *= 1.0f/numChannels;
	}
	difference_image[y*w+x] = diff;
}

ccl_device_inline void kernel_filter_nlm_blur(int x, int y,
                                              const ccl_global float *ccl_restrict difference_image,
                                              ccl_global float *out_image,
                                              int4 rect, int w, int f)
{
	float sum = 0.0f;
	const int low = max(rect.y, y-f);
	const int high = min(rect.w, y+f+1);
	for(int y1 = low; y1 < high; y1++) {
		sum += difference_image[y1*w+x];
	}
	sum *= 1.0f/(high-low);
	out_image[y*w+x] = sum;
}

ccl_device_inline void kernel_filter_nlm_calc_weight(int x, int y,
                                                     const ccl_global float *ccl_restrict difference_image,
                                                     ccl_global float *out_image,
                                                     int4 rect, int w, int f)
{
	float sum = 0.0f;
	const int low = max(rect.x, x-f);
	const int high = min(rect.z, x+f+1);
	for(int x1 = low; x1 < high; x1++) {
		sum += difference_image[y*w+x1];
	}
	sum *= 1.0f/(high-low);
	out_image[y*w+x] = fast_expf(-max(sum, 0.0f));
}

ccl_device_inline void kernel_filter_nlm_update_output(int x, int y,
                                                       int dx, int dy,
                                                       const ccl_global float *ccl_restrict difference_image,
                                                       const ccl_global float *ccl_restrict image,
                                                       ccl_global float *out_image,
                                                       ccl_global float *accum_image,
                                                       int4 rect, int w, int f)
{
	float sum = 0.0f;
	const int low = max(rect.x, x-f);
	const int high = min(rect.z, x+f+1);
	for(int x1 = low; x1 < high; x1++) {
		sum += difference_image[y*w+x1];
	}
	sum *= 1.0f/(high-low);
	if(out_image) {
		accum_image[y*w+x] += sum;
		out_image[y*w+x] += sum*image[(y+dy)*w+(x+dx)];
	}
	else {
		accum_image[y*w+x] = sum;
	}
}

ccl_device_inline void kernel_filter_nlm_construct_gramian(int fx, int fy,
                                                           int dx, int dy,
                                                           const ccl_global float *ccl_restrict difference_image,
                                                           const ccl_global float *ccl_restrict buffer,
                                                           const ccl_global float *ccl_restrict transform,
                                                           ccl_global int *rank,
                                                           ccl_global float *XtWX,
                                                           ccl_global float3 *XtWY,
                                                           int4 rect,
                                                           int4 filter_rect,
                                                           int w, int h, int f,
                                                           int pass_stride,
                                                           int localIdx)
{
	int y = fy + filter_rect.y;
	int x = fx + filter_rect.x;
	const int low = max(rect.x, x-f);
	const int high = min(rect.z, x+f+1);
	float sum = 0.0f;
	for(int x1 = low; x1 < high; x1++) {
		sum += difference_image[y*w+x1];
	}
	float weight = sum * (1.0f/(high - low));

	int storage_ofs = fy*filter_rect.z + fx;
	transform += storage_ofs;
	rank += storage_ofs;
	XtWX += storage_ofs;
	XtWY += storage_ofs;

	kernel_filter_construct_gramian(x, y,
	                                filter_rect.z*filter_rect.w,
	                                dx, dy, w, h,
	                                pass_stride,
	                                buffer,
	                                transform, rank,
	                                weight, XtWX, XtWY,
	                                localIdx);
}

ccl_device_inline void kernel_filter_nlm_normalize(int x, int y,
                                                   ccl_global float *out_image,
                                                   const ccl_global float *ccl_restrict accum_image,
                                                   int4 rect, int w)
{
	out_image[y*w+x] /= accum_image[y*w+x];
}

CCL_NAMESPACE_END
