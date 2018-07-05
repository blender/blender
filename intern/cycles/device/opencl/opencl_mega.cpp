/*
 * Copyright 2011-2013 Blender Foundation
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

#ifdef WITH_OPENCL

#include "device/opencl/opencl.h"

#include "render/buffers.h"

#include "kernel/kernel_types.h"

#include "util/util_md5.h"
#include "util/util_path.h"
#include "util/util_time.h"

CCL_NAMESPACE_BEGIN

class OpenCLDeviceMegaKernel : public OpenCLDeviceBase
{
public:
	OpenCLProgram path_trace_program;

	OpenCLDeviceMegaKernel(DeviceInfo& info, Stats &stats, bool background_)
	: OpenCLDeviceBase(info, stats, background_),
	  path_trace_program(this, "megakernel", "kernel.cl", "-D__COMPILE_ONLY_MEGAKERNEL__ ")
	{
	}

	virtual bool show_samples() const {
		return true;
	}

	virtual bool load_kernels(const DeviceRequestedFeatures& /*requested_features*/,
	                          vector<OpenCLProgram*> &programs)
	{
		path_trace_program.add_kernel(ustring("path_trace"));
		programs.push_back(&path_trace_program);
		return true;
	}

	~OpenCLDeviceMegaKernel()
	{
		task_pool.stop();
		path_trace_program.release();
	}

	void path_trace(RenderTile& rtile, int sample)
	{
		scoped_timer timer(&rtile.buffers->render_time);

		/* Cast arguments to cl types. */
		cl_mem d_data = CL_MEM_PTR(const_mem_map["__data"]->device_pointer);
		cl_mem d_buffer = CL_MEM_PTR(rtile.buffer);
		cl_int d_x = rtile.x;
		cl_int d_y = rtile.y;
		cl_int d_w = rtile.w;
		cl_int d_h = rtile.h;
		cl_int d_offset = rtile.offset;
		cl_int d_stride = rtile.stride;

		/* Sample arguments. */
		cl_int d_sample = sample;

		cl_kernel ckPathTraceKernel = path_trace_program(ustring("path_trace"));

		cl_uint start_arg_index =
			kernel_set_args(ckPathTraceKernel,
			                0,
			                d_data,
			                d_buffer);

		set_kernel_arg_buffers(ckPathTraceKernel, &start_arg_index);

		start_arg_index += kernel_set_args(ckPathTraceKernel,
		                                   start_arg_index,
		                                   d_sample,
		                                   d_x,
		                                   d_y,
		                                   d_w,
		                                   d_h,
		                                   d_offset,
		                                   d_stride);

		enqueue_kernel(ckPathTraceKernel, d_w, d_h);
	}

	void thread_run(DeviceTask *task)
	{
		if(task->type == DeviceTask::FILM_CONVERT) {
			film_convert(*task, task->buffer, task->rgba_byte, task->rgba_half);
		}
		else if(task->type == DeviceTask::SHADER) {
			shader(*task);
		}
		else if(task->type == DeviceTask::RENDER) {
			RenderTile tile;
			DenoisingTask denoising(this, *task);

			/* Keep rendering tiles until done. */
			while(task->acquire_tile(this, tile)) {
				if(tile.task == RenderTile::PATH_TRACE) {
					int start_sample = tile.start_sample;
					int end_sample = tile.start_sample + tile.num_samples;

					for(int sample = start_sample; sample < end_sample; sample++) {
						if(task->get_cancel()) {
							if(task->need_finish_queue == false)
								break;
						}

						path_trace(tile, sample);

						tile.sample = sample + 1;

						task->update_progress(&tile, tile.w*tile.h);
					}

					/* Complete kernel execution before release tile */
					/* This helps in multi-device render;
					 * The device that reaches the critical-section function
					 * release_tile waits (stalling other devices from entering
					 * release_tile) for all kernels to complete. If device1 (a
					 * slow-render device) reaches release_tile first then it would
					 * stall device2 (a fast-render device) from proceeding to render
					 * next tile.
					 */
					clFinish(cqCommandQueue);
				}
				else if(tile.task == RenderTile::DENOISE) {
					tile.sample = tile.start_sample + tile.num_samples;
					denoise(tile, denoising);
					task->update_progress(&tile, tile.w*tile.h);
				}

				task->release_tile(tile);
			}
		}
	}

	bool is_split_kernel()
	{
		return false;
	}
};

Device *opencl_create_mega_device(DeviceInfo& info, Stats& stats, bool background)
{
	return new OpenCLDeviceMegaKernel(info, stats, background);
}

CCL_NAMESPACE_END

#endif
