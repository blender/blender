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

#ifndef __DEVICE_DENOISING_H__
#define __DEVICE_DENOISING_H__

#include "device/device.h"

#include "render/buffers.h"

#include "kernel/filter/filter_defines.h"

CCL_NAMESPACE_BEGIN

class DenoisingTask {
public:
	/* Parameters of the denoising algorithm. */
	int radius;
	float nlm_k_2;
	float pca_threshold;

	/* Pointer and parameters of the RenderBuffers. */
	struct RenderBuffers {
		int denoising_data_offset;
		int denoising_clean_offset;
		int pass_stride;
		int offset;
		int stride;
		device_ptr ptr;
		int samples;
	} render_buffer;

	TilesInfo *tiles;
	device_vector<int> tiles_mem;
	void tiles_from_rendertiles(RenderTile *rtiles);

	int4 rect;
	int4 filter_area;

	struct DeviceFunctions {
		function<bool(device_ptr image_ptr,    /* Contains the values that are smoothed. */
		              device_ptr guide_ptr,    /* Contains the values that are used to calculate weights. */
		              device_ptr variance_ptr, /* Contains the variance of the guide image. */
		              device_ptr out_ptr       /* The filtered output is written into this image. */
		              )> non_local_means;
		function<bool(device_ptr color_ptr,
		              device_ptr color_variance_ptr,
		              device_ptr output_ptr
		              )> reconstruct;
		function<bool()> construct_transform;

		function<bool(device_ptr a_ptr,
		              device_ptr b_ptr,
		              device_ptr mean_ptr,
		              device_ptr variance_ptr,
		              int r,
		              int4 rect
		              )> combine_halves;
		function<bool(device_ptr a_ptr,
		              device_ptr b_ptr,
		              device_ptr sample_variance_ptr,
		              device_ptr sv_variance_ptr,
		              device_ptr buffer_variance_ptr
		              )> divide_shadow;
		function<bool(int mean_offset,
		              int variance_offset,
		              device_ptr mean_ptr,
		              device_ptr variance_ptr
		              )> get_feature;
		function<bool(device_ptr image_ptr,
		              device_ptr variance_ptr,
		              device_ptr depth_ptr,
		              device_ptr output_ptr
		              )> detect_outliers;
		function<bool(device_ptr*)> set_tiles;
	} functions;

	/* Stores state of the current Reconstruction operation,
	 * which is accessed by the device in order to perform the operation. */
	struct ReconstructionState {
		device_ptr temporary_1_ptr; /* There two images are used as temporary storage. */
		device_ptr temporary_2_ptr;

		int4 filter_rect;
		int4 buffer_params;

		int source_w;
		int source_h;
	} reconstruction_state;

	/* Stores state of the current NLM operation,
	 * which is accessed by the device in order to perform the operation. */
	struct NLMState {
		device_ptr temporary_1_ptr; /* There three images are used as temporary storage. */
		device_ptr temporary_2_ptr;
		device_ptr temporary_3_ptr;

		int r;      /* Search radius of the filter. */
		int f;      /* Patch size of the filter. */
		float a;    /* Variance compensation factor in the MSE estimation. */
		float k_2;  /* Squared value of the k parameter of the filter. */

		void set_parameters(int r_, int f_, float a_, float k_2_) { r = r_; f = f_; a = a_, k_2 = k_2_; }
	} nlm_state;

	struct Storage {
		device_only_memory<float>  transform;
		device_only_memory<int>    rank;
		device_only_memory<float>  XtWX;
		device_only_memory<float3> XtWY;
		int w;
		int h;
	} storage;

	DenoisingTask(Device *device) : device(device) {}

	void init_from_devicetask(const DeviceTask &task);

	bool run_denoising();

	struct DenoiseBuffers {
		int pass_stride;
		int passes;
		int w;
		int h;
		device_only_memory<float> mem;
	} buffer;

protected:
	Device *device;
};

CCL_NAMESPACE_END

#endif /* __DEVICE_DENOISING_H__ */
