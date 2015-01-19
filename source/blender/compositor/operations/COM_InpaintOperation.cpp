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
 *
 * Contributor: Peter Schlaile
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "MEM_guardedalloc.h"

#include "COM_InpaintOperation.h"
#include "COM_OpenCLDevice.h"

#include "BLI_math.h"

#define ASSERT_XY_RANGE(x, y)  \
	BLI_assert(x >= 0 && x < this->getWidth() && \
	           y >= 0 && y < this->getHeight())


// Inpaint (simple convolve using average of known pixels)
InpaintSimpleOperation::InpaintSimpleOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);
	this->m_inputImageProgram = NULL;
	this->m_pixelorder = NULL;
	this->m_manhatten_distance = NULL;
	this->m_cached_buffer = NULL;
	this->m_cached_buffer_ready = false;
}
void InpaintSimpleOperation::initExecution()
{
	this->m_inputImageProgram = this->getInputSocketReader(0);

	this->m_pixelorder = NULL;
	this->m_manhatten_distance = NULL;
	this->m_cached_buffer = NULL;
	this->m_cached_buffer_ready = false;

	this->initMutex();
}

void InpaintSimpleOperation::clamp_xy(int &x, int &y)
{
	int width = this->getWidth();
	int height = this->getHeight();

	if (x < 0) {
		x = 0;
	}
	else if (x >= width) {
		x = width - 1;
	}

	if (y < 0) {
		y = 0;
	}
	else if (y >= height) {
		y = height - 1;
	}
}

float *InpaintSimpleOperation::get_pixel(int x, int y)
{
	int width = this->getWidth();

	ASSERT_XY_RANGE(x, y);

	return &this->m_cached_buffer[
			y * width * COM_NUM_CHANNELS_COLOR +
			x * COM_NUM_CHANNELS_COLOR];
}

int InpaintSimpleOperation::mdist(int x, int y) 
{
	int width = this->getWidth();

	ASSERT_XY_RANGE(x, y);

	return this->m_manhatten_distance[y * width + x];
}

bool InpaintSimpleOperation::next_pixel(int &x, int &y, int & curr, int iters)
{
	int width = this->getWidth();

	if (curr >= this->m_area_size) {
		return false;
	}
	
	int r = this->m_pixelorder[curr++];

	x = r % width;
	y = r / width;

	if (this->mdist(x, y) > iters) {
		return false;
	}
	
	return true;
}

void InpaintSimpleOperation::calc_manhatten_distance() 
{
	int width = this->getWidth();
	int height = this->getHeight();
	short *m = this->m_manhatten_distance = (short *)MEM_mallocN(sizeof(short) * width * height, __func__);
	int *offsets;

	offsets = (int *)MEM_callocN(sizeof(int) * (width + height + 1), "InpaintSimpleOperation offsets");

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			int r = 0;
			/* no need to clamp here */
			if (this->get_pixel(i, j)[3] < 1.0f) {
				r = width + height;
				if (i > 0) 
					r = min_ii(r, m[j * width + i - 1] + 1);
				if (j > 0) 
					r = min_ii(r, m[(j - 1) * width + i] + 1);
			}
			m[j * width + i] = r;
		}
	}

	for (int j = height - 1; j >= 0; j--) {
		for (int i = width - 1; i >= 0; i--) {
			int r = m[j * width + i];
			
			if (i + 1 < width) 
				r = min_ii(r, m[j * width + i + 1] + 1);
			if (j + 1 < height) 
				r = min_ii(r, m[(j + 1) * width + i] + 1);
			
			m[j * width + i] = r;
			
			offsets[r]++;
		}
	}
	
	offsets[0] = 0;
	
	for (int i = 1; i < width + height + 1; i++) {
		offsets[i] += offsets[i - 1];
	}
	
	this->m_area_size = offsets[width + height];
	this->m_pixelorder = (int *)MEM_mallocN(sizeof(int) * this->m_area_size, __func__);
	
	for (int i = 0; i < width * height; i++) {
		if (m[i] > 0) {
			this->m_pixelorder[offsets[m[i] - 1]++] = i;
		}
	}

	MEM_freeN(offsets);
}

void InpaintSimpleOperation::pix_step(int x, int y)
{
	const int d = this->mdist(x, y);
	float pix[3] = {0.0f, 0.0f, 0.0f};
	float pix_divider = 0.0f;

	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			/* changing to both != 0 gives dithering artifacts */
			if (dx != 0 || dy != 0) {
				int x_ofs = x + dx;
				int y_ofs = y + dy;

				this->clamp_xy(x_ofs, y_ofs);

				if (this->mdist(x_ofs, y_ofs) < d) {

					float weight;

					if (dx == 0 || dy == 0) {
						weight = 1.0f;
					}
					else {
						weight = M_SQRT1_2;  /* 1.0f / sqrt(2) */
					}

					madd_v3_v3fl(pix, this->get_pixel(x_ofs, y_ofs), weight);
					pix_divider += weight;
				}
			}
		}
	}

	float *output = this->get_pixel(x, y);
	if (pix_divider != 0.0f) {
		mul_v3_fl(pix, 1.0f / pix_divider);
		/* use existing pixels alpha to blend into */
		interp_v3_v3v3(output, pix, output, output[3]);
		output[3] = 1.0f;
	}
}

void *InpaintSimpleOperation::initializeTileData(rcti *rect)
{
	if (this->m_cached_buffer_ready) {
		return this->m_cached_buffer;
	}
	lockMutex();
	if (!this->m_cached_buffer_ready) {
		MemoryBuffer *buf = (MemoryBuffer *)this->m_inputImageProgram->initializeTileData(rect);
		this->m_cached_buffer = (float *)MEM_dupallocN(buf->getBuffer());

		this->calc_manhatten_distance();

		int curr = 0;
		int x, y;

	
		while (this->next_pixel(x, y, curr, this->m_iterations)) {
			this->pix_step(x, y);
		}
		this->m_cached_buffer_ready = true;
	}

	unlockMutex();
	return this->m_cached_buffer;
}

void InpaintSimpleOperation::executePixel(float output[4], int x, int y, void *data)
{
	this->clamp_xy(x, y);
	copy_v4_v4(output, this->get_pixel(x, y));
}

void InpaintSimpleOperation::deinitExecution()
{
	this->m_inputImageProgram = NULL;
	this->deinitMutex();
	if (this->m_cached_buffer) {
		MEM_freeN(this->m_cached_buffer);
		this->m_cached_buffer = NULL;
	}

	if (this->m_pixelorder) {
		MEM_freeN(this->m_pixelorder);
		this->m_pixelorder = NULL;
	}

	if (this->m_manhatten_distance) {
		MEM_freeN(this->m_manhatten_distance);
		this->m_manhatten_distance = NULL;
	}
	this->m_cached_buffer_ready = false;
}

bool InpaintSimpleOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	if (this->m_cached_buffer_ready) {
		return false;
	}
	else {
		rcti newInput;
	
		newInput.xmax = getWidth();
		newInput.xmin = 0;
		newInput.ymax = getHeight();
		newInput.ymin = 0;
	
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}

