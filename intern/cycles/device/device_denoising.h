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

	/* Parameters of the RenderBuffers. */
	struct RenderBuffers {
		int offset;
		int pass_stride;
		int samples;
	} render_buffer;

	/* Pointer and parameters of the target buffer. */
	struct TargetBuffer {
		int offset;
		int stride;
		int pass_stride;
		int denoising_clean_offset;
		device_ptr ptr;
	} target_buffer;

	TileInfo *tile_info;
	device_vector<int> tile_info_mem;

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
		function<void(RenderTile *rtiles)> map_neighbor_tiles;
		function<void(RenderTile *rtiles)> unmap_neighbor_tiles;
	} functions;

	/* Stores state of the current Reconstruction operation,
	 * which is accessed by the device in order to perform the operation. */
	struct ReconstructionState {
		device_ptr temporary_1_ptr; /* There two images are used as temporary storage. */
		device_ptr temporary_2_ptr;

		int4 filter_window;
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
		device_only_memory<float>  temporary_1;
		device_only_memory<float>  temporary_2;
		device_only_memory<float>  temporary_color;
		int w;
		int h;

		Storage(Device *device)
		: transform(device, "denoising transform"),
		  rank(device, "denoising rank"),
		  XtWX(device, "denoising XtWX"),
		  XtWY(device, "denoising XtWY"),
		  temporary_1(device, "denoising NLM temporary 1"),
		  temporary_2(device, "denoising NLM temporary 2"),
		  temporary_color(device, "denoising temporary color")
		{}
	} storage;

	DenoisingTask(Device *device, const DeviceTask &task);
	~DenoisingTask();

	void run_denoising(RenderTile *tile);

	struct DenoiseBuffers {
		int pass_stride;
		int passes;
		int stride;
		int h;
		int width;
		device_only_memory<float> mem;

		DenoiseBuffers(Device *device)
		: mem(device, "denoising pixel buffer")
	    {}
	} buffer;

protected:
	Device *device;

	void set_render_buffer(RenderTile *rtiles);
	void setup_denoising_buffer();
	void prefilter_shadowing();
	void prefilter_features();
	void prefilter_color();
	void construct_transform();
	void reconstruct();
};

CCL_NAMESPACE_END

#endif /* __DEVICE_DENOISING_H__ */
