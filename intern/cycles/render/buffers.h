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

#ifndef __BUFFERS_H__
#define __BUFFERS_H__

#include "device_memory.h"

#include "film.h"

#include "kernel_types.h"

#include "util_half.h"
#include "util_string.h"
#include "util_thread.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

class Device;
struct DeviceDrawParams;
struct float4;

/* Buffer Parameters
 * Size of render buffer and how it fits in the full image (border render). */

class BufferParams {
public:
	/* width/height of the physical buffer */
	int width;
	int height;

	/* offset into and width/height of the full buffer */
	int full_x;
	int full_y;
	int full_width;
	int full_height;

	/* passes */
	vector<Pass> passes;

	/* functions */
	BufferParams();

	void get_offset_stride(int& offset, int& stride);
	bool modified(const BufferParams& params);
	void add_pass(PassType type);
	int get_passes_size();
};

/* Render Buffers */

class RenderBuffers {
public:
	/* buffer parameters */
	BufferParams params;

	/* float buffer */
	device_vector<float> buffer;
	/* random number generator state */
	device_vector<uint> rng_state;

	RenderBuffers(Device *device);
	~RenderBuffers();

	void reset(Device *device, BufferParams& params);

	bool copy_from_device();
	bool get_pass_rect(PassType type, float exposure, int sample, int components, float *pixels);

protected:
	void device_free();

	Device *device;
};

/* Display Buffer
 *
 * The buffer used for drawing during render, filled by converting the render
 * buffers to byte of half float storage */

class DisplayBuffer {
public:
	/* buffer parameters */
	BufferParams params;
	/* dimensions for how much of the buffer is actually ready for display.
	 * with progressive render we can be using only a subset of the buffer.
	 * if these are zero, it means nothing can be drawn yet */
	int draw_width, draw_height;
	/* draw alpha channel? */
	bool transparent;
	/* use half float? */
	bool half_float;
	/* byte buffer for converted result */
	device_vector<uchar4> rgba_byte;
	device_vector<half4> rgba_half;

	DisplayBuffer(Device *device, bool linear = false);
	~DisplayBuffer();

	void reset(Device *device, BufferParams& params);
	void write(Device *device, const string& filename);

	void draw_set(int width, int height);
	void draw(Device *device, const DeviceDrawParams& draw_params);
	bool draw_ready();

	device_memory& rgba_data();

protected:
	void device_free();

	Device *device;
};

/* Render Tile
 * Rendering task on a buffer */

class RenderTile {
public:
	int x, y, w, h;
	int start_sample;
	int num_samples;
	int sample;
	int resolution;
	int offset;
	int stride;

	device_ptr buffer;
	device_ptr rng_state;

	RenderBuffers *buffers;

	RenderTile();
};

CCL_NAMESPACE_END

#endif /* __BUFFERS_H__ */

