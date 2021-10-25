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

#include <stdlib.h>

#include "render/buffers.h"
#include "device/device.h"

#include "util/util_debug.h"
#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_image.h"
#include "util/util_math.h"
#include "util/util_opengl.h"
#include "util/util_time.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

/* Buffer Params */

BufferParams::BufferParams()
{
	width = 0;
	height = 0;

	full_x = 0;
	full_y = 0;
	full_width = 0;
	full_height = 0;

	denoising_data_pass = false;
	denoising_clean_pass = false;

	Pass::add(PASS_COMBINED, passes);
}

void BufferParams::get_offset_stride(int& offset, int& stride)
{
	offset = -(full_x + full_y*width);
	stride = width;
}

bool BufferParams::modified(const BufferParams& params)
{
	return !(full_x == params.full_x
		&& full_y == params.full_y
		&& width == params.width
		&& height == params.height
		&& full_width == params.full_width
		&& full_height == params.full_height
		&& Pass::equals(passes, params.passes));
}

int BufferParams::get_passes_size()
{
	int size = 0;

	for(size_t i = 0; i < passes.size(); i++)
		size += passes[i].components;

	if(denoising_data_pass) {
		size += DENOISING_PASS_SIZE_BASE;
		if(denoising_clean_pass) size += DENOISING_PASS_SIZE_CLEAN;
	}

	return align_up(size, 4);
}

int BufferParams::get_denoising_offset()
{
	int offset = 0;

	for(size_t i = 0; i < passes.size(); i++)
		offset += passes[i].components;

	return offset;
}

/* Render Buffer Task */

RenderTile::RenderTile()
{
	x = 0;
	y = 0;
	w = 0;
	h = 0;

	sample = 0;
	start_sample = 0;
	num_samples = 0;
	resolution = 0;

	offset = 0;
	stride = 0;

	buffer = 0;
	rng_state = 0;

	buffers = NULL;
}

/* Render Buffers */

RenderBuffers::RenderBuffers(Device *device_)
{
	device = device_;
}

RenderBuffers::~RenderBuffers()
{
	device_free();
}

void RenderBuffers::device_free()
{
	if(buffer.device_pointer) {
		device->mem_free(buffer);
		buffer.clear();
	}

	if(rng_state.device_pointer) {
		device->mem_free(rng_state);
		rng_state.clear();
	}
}

void RenderBuffers::reset(Device *device, BufferParams& params_)
{
	params = params_;

	/* free existing buffers */
	device_free();
	
	/* allocate buffer */
	buffer.resize(params.width*params.height*params.get_passes_size());
	device->mem_alloc("render_buffer", buffer, MEM_READ_WRITE);
	device->mem_zero(buffer);

	/* allocate rng state */
	rng_state.resize(params.width, params.height);

	device->mem_alloc("rng_state", rng_state, MEM_READ_WRITE);
}

bool RenderBuffers::copy_from_device(Device *from_device)
{
	if(!buffer.device_pointer)
		return false;

	if(!from_device) {
		from_device = device;
	}

	from_device->mem_copy_from(buffer, 0, params.width, params.height, params.get_passes_size()*sizeof(float));

	return true;
}

bool RenderBuffers::get_denoising_pass_rect(int offset, float exposure, int sample, int components, float *pixels)
{
	float scale = 1.0f/sample;

	if(offset == DENOISING_PASS_COLOR) {
		scale *= exposure;
	}
	else if(offset == DENOISING_PASS_COLOR_VAR) {
		scale *= exposure*exposure;
	}

	offset += params.get_denoising_offset();
	float *in = (float*)buffer.data_pointer + offset;
	int pass_stride = params.get_passes_size();
	int size = params.width*params.height;

	if(components == 1) {
		for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
			pixels[0] = in[0]*scale;
		}
	}
	else if(components == 3) {
		for(int i = 0; i < size; i++, in += pass_stride, pixels += 3) {
			pixels[0] = in[0]*scale;
			pixels[1] = in[1]*scale;
			pixels[2] = in[2]*scale;
		}
	}
	else {
		return false;
	}

	return true;
}

bool RenderBuffers::get_pass_rect(PassType type, float exposure, int sample, int components, float *pixels)
{
	int pass_offset = 0;

	for(size_t j = 0; j < params.passes.size(); j++) {
		Pass& pass = params.passes[j];

		if(pass.type != type) {
			pass_offset += pass.components;
			continue;
		}

		float *in = (float*)buffer.data_pointer + pass_offset;
		int pass_stride = params.get_passes_size();

		float scale = (pass.filter)? 1.0f/(float)sample: 1.0f;
		float scale_exposure = (pass.exposure)? scale*exposure: scale;

		int size = params.width*params.height;

		if(components == 1) {
			assert(pass.components == components);

			/* scalar */
			if(type == PASS_DEPTH) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = (f == 0.0f)? 1e10f: f*scale_exposure;
				}
			}
			else if(type == PASS_MIST) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = saturate(f*scale_exposure);
				}
			}
#ifdef WITH_CYCLES_DEBUG
			else if(type == PASS_BVH_TRAVERSED_NODES ||
			        type == PASS_BVH_TRAVERSED_INSTANCES ||
			        type == PASS_BVH_INTERSECTIONS ||
			        type == PASS_RAY_BOUNCES)
			{
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = f*scale;
				}
			}
#endif
			else {
				for(int i = 0; i < size; i++, in += pass_stride, pixels++) {
					float f = *in;
					pixels[0] = f*scale_exposure;
				}
			}
		}
		else if(components == 3) {
			assert(pass.components == 4);

			/* RGBA */
			if(type == PASS_SHADOW) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 3) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float invw = (f.w > 0.0f)? 1.0f/f.w: 1.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
				}
			}
			else if(pass.divide_type != PASS_NONE) {
				/* RGB lighting passes that need to divide out color */
				pass_offset = 0;
				for(size_t k = 0; k < params.passes.size(); k++) {
					Pass& color_pass = params.passes[k];
					if(color_pass.type == pass.divide_type)
						break;
					pass_offset += color_pass.components;
				}

				float *in_divide = (float*)buffer.data_pointer + pass_offset;

				for(int i = 0; i < size; i++, in += pass_stride, in_divide += pass_stride, pixels += 3) {
					float3 f = make_float3(in[0], in[1], in[2]);
					float3 f_divide = make_float3(in_divide[0], in_divide[1], in_divide[2]);

					f = safe_divide_even_color(f*exposure, f_divide);

					pixels[0] = f.x;
					pixels[1] = f.y;
					pixels[2] = f.z;
				}
			}
			else {
				/* RGB/vector */
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 3) {
					float3 f = make_float3(in[0], in[1], in[2]);

					pixels[0] = f.x*scale_exposure;
					pixels[1] = f.y*scale_exposure;
					pixels[2] = f.z*scale_exposure;
				}
			}
		}
		else if(components == 4) {
			assert(pass.components == components);

			/* RGBA */
			if(type == PASS_SHADOW) {
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float invw = (f.w > 0.0f)? 1.0f/f.w: 1.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
					pixels[3] = 1.0f;
				}
			}
			else if(type == PASS_MOTION) {
				/* need to normalize by number of samples accumulated for motion */
				pass_offset = 0;
				for(size_t k = 0; k < params.passes.size(); k++) {
					Pass& color_pass = params.passes[k];
					if(color_pass.type == PASS_MOTION_WEIGHT)
						break;
					pass_offset += color_pass.components;
				}

				float *in_weight = (float*)buffer.data_pointer + pass_offset;

				for(int i = 0; i < size; i++, in += pass_stride, in_weight += pass_stride, pixels += 4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);
					float w = in_weight[0];
					float invw = (w > 0.0f)? 1.0f/w: 0.0f;

					pixels[0] = f.x*invw;
					pixels[1] = f.y*invw;
					pixels[2] = f.z*invw;
					pixels[3] = f.w*invw;
				}
			}
			else {
				for(int i = 0; i < size; i++, in += pass_stride, pixels += 4) {
					float4 f = make_float4(in[0], in[1], in[2], in[3]);

					pixels[0] = f.x*scale_exposure;
					pixels[1] = f.y*scale_exposure;
					pixels[2] = f.z*scale_exposure;

					/* clamp since alpha might be > 1.0 due to russian roulette */
					pixels[3] = saturate(f.w*scale);
				}
			}
		}

		return true;
	}

	return false;
}

/* Display Buffer */

DisplayBuffer::DisplayBuffer(Device *device_, bool linear)
{
	device = device_;
	draw_width = 0;
	draw_height = 0;
	transparent = true; /* todo: determine from background */
	half_float = linear;
}

DisplayBuffer::~DisplayBuffer()
{
	device_free();
}

void DisplayBuffer::device_free()
{
	if(rgba_byte.device_pointer) {
		device->pixels_free(rgba_byte);
		rgba_byte.clear();
	}
	if(rgba_half.device_pointer) {
		device->pixels_free(rgba_half);
		rgba_half.clear();
	}
}

void DisplayBuffer::reset(Device *device, BufferParams& params_)
{
	draw_width = 0;
	draw_height = 0;

	params = params_;

	/* free existing buffers */
	device_free();

	/* allocate display pixels */
	if(half_float) {
		rgba_half.resize(params.width, params.height);
		device->pixels_alloc(rgba_half);
	}
	else {
		rgba_byte.resize(params.width, params.height);
		device->pixels_alloc(rgba_byte);
	}
}

void DisplayBuffer::draw_set(int width, int height)
{
	assert(width <= params.width && height <= params.height);

	draw_width = width;
	draw_height = height;
}

void DisplayBuffer::draw(Device *device, const DeviceDrawParams& draw_params)
{
	if(draw_width != 0 && draw_height != 0) {
		device_memory& rgba = rgba_data();

		device->draw_pixels(rgba, 0, draw_width, draw_height, params.full_x, params.full_y, params.width, params.height, transparent, draw_params);
	}
}

bool DisplayBuffer::draw_ready()
{
	return (draw_width != 0 && draw_height != 0);
}

void DisplayBuffer::write(Device *device, const string& filename)
{
	int w = draw_width;
	int h = draw_height;

	if(w == 0 || h == 0)
		return;
	
	if(half_float)
		return;

	/* read buffer from device */
	device_memory& rgba = rgba_data();
	device->pixels_copy_from(rgba, 0, w, h);

	/* write image */
	ImageOutput *out = ImageOutput::create(filename);
	ImageSpec spec(w, h, 4, TypeDesc::UINT8);
	int scanlinesize = w*4*sizeof(uchar);

	out->open(filename, spec);

	/* conversion for different top/bottom convention */
	out->write_image(TypeDesc::UINT8,
		(uchar*)rgba.data_pointer + (h-1)*scanlinesize,
		AutoStride,
		-scanlinesize,
		AutoStride);

	out->close();

	delete out;
}

device_memory& DisplayBuffer::rgba_data()
{
	if(half_float)
		return rgba_half;
	else
		return rgba_byte;
}

CCL_NAMESPACE_END

