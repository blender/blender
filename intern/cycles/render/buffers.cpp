/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <stdlib.h>

#include "buffers.h"
#include "device.h"

#include "util_debug.h"
#include "util_hash.h"
#include "util_image.h"
#include "util_math.h"
#include "util_opengl.h"
#include "util_time.h"
#include "util_types.h"

CCL_NAMESPACE_BEGIN

/* Render Buffers */

RenderBuffers::RenderBuffers(Device *device_)
{
	device = device_;
	width = 0;
	height = 0;
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

void RenderBuffers::reset(Device *device, int width_, int height_)
{
	width = width_;
	height = height_;

	/* free existing buffers */
	device_free();
	
	/* allocate buffer */
	buffer.resize(width, height);
	device->mem_alloc(buffer, MEM_READ_WRITE);
	device->mem_zero(buffer);

	/* allocate rng state */
	rng_state.resize(width, height);

	uint *init_state = rng_state.resize(width, height);
	int x, y;
	
	for(x=0; x<width; x++)
		for(y=0; y<height; y++)
			init_state[x + y*width] = hash_int_2d(x, y);

	device->mem_alloc(rng_state, MEM_READ_WRITE);
	device->mem_copy_to(rng_state);
}

float4 *RenderBuffers::copy_from_device(float exposure, int sample)
{
	if(!buffer.device_pointer)
		return NULL;

	device->mem_copy_from(buffer, 0, buffer.memory_size());

	float4 *out = new float4[width*height];
	float4 *in = (float4*)buffer.data_pointer;
	float scale = 1.0f/(float)sample;
	
	for(int i = width*height - 1; i >= 0; i--) {
		float4 rgba = in[i]*scale;

		rgba.x = rgba.x*exposure;
		rgba.y = rgba.y*exposure;
		rgba.z = rgba.z*exposure;

		/* clamp since alpha might be > 1.0 due to russian roulette */
		rgba.w = clamp(rgba.w, 0.0f, 1.0f);

		out[i] = rgba;
	}

	return out;
}

/* Display Buffer */

DisplayBuffer::DisplayBuffer(Device *device_)
{
	device = device_;
	width = 0;
	height = 0;
	draw_width = 0;
	draw_height = 0;
	transparent = true; /* todo: determine from background */
}

DisplayBuffer::~DisplayBuffer()
{
	device_free();
}

void DisplayBuffer::device_free()
{
	if(rgba.device_pointer) {
		device->pixels_free(rgba);
		rgba.clear();
	}
}

void DisplayBuffer::reset(Device *device, int width_, int height_)
{
	draw_width = 0;
	draw_height = 0;

	width = width_;
	height = height_;

	/* free existing buffers */
	device_free();

	/* allocate display pixels */
	rgba.resize(width, height);
	device->pixels_alloc(rgba);
}

void DisplayBuffer::draw_set(int width_, int height_)
{
	assert(width_ <= width && height_ <= height);

	draw_width = width_;
	draw_height = height_;
}

void DisplayBuffer::draw_transparency_grid()
{
	GLubyte checker_stipple_sml[32*32/8] = {
		255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0, \
		255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0, \
		0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255, \
		0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255, \
		255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0, \
		255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0, \
		0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255, \
		0,255,0,255,0,255,0,255,0,255,0,255,0,255,0,255, \
	};

	glColor4ub(50, 50, 50, 255);
	glRectf(0, 0, width, height);
	glEnable(GL_POLYGON_STIPPLE);
	glColor4ub(55, 55, 55, 255);
	glPolygonStipple(checker_stipple_sml);
	glRectf(0, 0, width, height);
	glDisable(GL_POLYGON_STIPPLE);
}

void DisplayBuffer::draw(Device *device)
{
	if(draw_width != 0 && draw_height != 0) {
		if(transparent)
			draw_transparency_grid();

		device->draw_pixels(rgba, 0, draw_width, draw_height, width, height, transparent);
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

	/* read buffer from device */
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

CCL_NAMESPACE_END

