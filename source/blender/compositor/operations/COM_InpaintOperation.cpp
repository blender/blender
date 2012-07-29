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

#include "COM_InpaintOperation.h"
#include "BLI_math.h"
#include "COM_OpenCLDevice.h"

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

	this->m_cached_buffer = NULL;
	this->m_pixelorder = NULL;
	this->m_manhatten_distance = NULL;
	this->m_cached_buffer = NULL;
	this->m_cached_buffer_ready = false;

	this->initMutex();
}

void InpaintSimpleOperation::clamp_xy(int & x, int & y) 
{
	int width = this->getWidth();
	int height = this->getHeight();

	if (x < 0) {
		x = 0;
	}
	if (x >= width) {
		x = width - 1;
	}
	if (y < 0) {
		y = 0;
	}
	if (y >= height) {
		y = height - 1;
	}
}

float InpaintSimpleOperation::get(int x, int y, int component) 
{
	int width = this->getWidth();

	clamp_xy(x, y);
	return this->m_cached_buffer[
	           y * width * COM_NUMBER_OF_CHANNELS
	           + x * COM_NUMBER_OF_CHANNELS + component];
}

void InpaintSimpleOperation::set(int x, int y, int component, float v) 
{
	int width = this->getWidth();

	this->m_cached_buffer[
	    y * width * COM_NUMBER_OF_CHANNELS
	    + x * COM_NUMBER_OF_CHANNELS + component] = v;
}

int InpaintSimpleOperation::mdist(int x, int y) 
{
	int width = this->getWidth();
	clamp_xy(x, y);
	return this->m_manhatten_distance[y * width + x];
}

bool InpaintSimpleOperation::next_pixel(int & x, int & y, int & curr, int iters)
{
	int width = this->getWidth();

	if (curr >= this->m_area_size) {
		return false;
	}
	
	int r = this->m_pixelorder[curr++];

	x = r % width;
	y = r / width;

	if (mdist(x, y) > iters) {
		return false;
	}
	
	return true;
}

void InpaintSimpleOperation::calc_manhatten_distance() 
{
	int width = this->getWidth();
	int height = this->getHeight();
	short *m = this->m_manhatten_distance = new short[width * height];
	int offsets[width + height + 1];

	memset(offsets, 0, sizeof(offsets));

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			int r = 0;
			if (get(i, j, 3) < 1.0f) {
				r = width + height;
				if (i > 0) 
					r = mini(r, m[j * width + i - 1] + 1);
				if (j > 0) 
					r = mini(r, m[(j - 1) * width + i] + 1);
			}
			m[j * width + i] = r;
		}
	}
	
	for (int j = height - 1; j >= 0; j--) {
		for (int i = width; i >= 0; i--) {
			int r = m[j * width + i];
			
			if (i + 1 < width) 
				r = mini(r, m[j * width + i + 1] + 1);
			if (j + 1 < height) 
				r = mini(r, m[(j + 1) * width + i] + 1);
			
			m[j * width + i] = r;
			
			offsets[r]++;
		}
	}
	
	offsets[0] = 0;
	
	for (int i = 1; i < width + height + 1; i++) {
		offsets[i] += offsets[i - 1];
	}
	
	this->m_area_size = offsets[width + height];
	this->m_pixelorder = new int[this->m_area_size];
	
	for (int i = 0; i < width * height; i++) {
		if (m[i] > 0) {
			this->m_pixelorder[offsets[m[i] - 1]++] = i;
		}
	}
}

void InpaintSimpleOperation::pix_step(int x, int y)
{
	int d = this->mdist(x, y);

	float n = 0;

	float pix[3];

	memset(pix, 0, sizeof(pix));

	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			if (dx != 0 && dy != 0 && 
			    this->mdist(x + dx, y + dy) < d) {
				float weight = M_SQRT1_2;   /* 1.0f / sqrt(2) */

				if (dx == 0 || dy == 0) {
					weight = 1.0f;
				}
				
				for (int c = 0; c < 3; c++) {
					float fk = this->get(x + dx, y + dy, c);

					pix[c] += fk * weight;
				}
				n += weight;
			}
		}
	}

	for (int c = 0; c < 3; c++) {
		this->set(x, y, c, pix[c] / n);
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

		this->m_cached_buffer = new float[this->getWidth() * this->getHeight() * COM_NUMBER_OF_CHANNELS];
		memcpy(this->m_cached_buffer, buf->getBuffer(), this->getWidth() * this->getHeight() * COM_NUMBER_OF_CHANNELS * sizeof(float));

		calc_manhatten_distance();

		int curr = 0;
		int x, y;

	
		while (next_pixel(x, y, curr, this->m_iterations)) {
			pix_step(x, y);
		}
		this->m_cached_buffer_ready = true;
	}

	unlockMutex();
	return this->m_cached_buffer;
}

void InpaintSimpleOperation::executePixel(float *color, int x, int y, void *data)
{
	for (int c = 0; c < 3; c++) {
		color[c] = get(x, y, c);
	}
	color[3] = 1.0f;
}

void InpaintSimpleOperation::deinitExecution()
{
	this->m_inputImageProgram = NULL;
	this->deinitMutex();
	if (this->m_cached_buffer) {
		delete [] this->m_cached_buffer;
		this->m_cached_buffer = NULL;
	}

	if (this->m_pixelorder) {
		delete [] this->m_pixelorder;
		this->m_pixelorder = NULL;
	}

	if (this->m_manhatten_distance) {
		delete [] this->m_manhatten_distance;
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

