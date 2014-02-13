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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_GaussianYBlurOperation.h"
#include "BLI_math.h"
#include "MEM_guardedalloc.h"

extern "C" {
#  include "RE_pipeline.h"
}

GaussianYBlurOperation::GaussianYBlurOperation() : BlurBaseOperation(COM_DT_COLOR)
{
	this->m_gausstab = NULL;
	this->m_filtersize = 0;
}

void *GaussianYBlurOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (!this->m_sizeavailable) {
		updateGauss();
	}
	void *buffer = getInputOperation(0)->initializeTileData(NULL);
	unlockMutex();
	return buffer;
}

void GaussianYBlurOperation::initExecution()
{
	BlurBaseOperation::initExecution();

	initMutex();

	if (this->m_sizeavailable) {
		float rad = max_ff(m_size * m_data->sizey, 0.0f);
		m_filtersize = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);
		
		this->m_gausstab = BlurBaseOperation::make_gausstab(rad, m_filtersize);
	}
}

void GaussianYBlurOperation::updateGauss()
{
	if (this->m_gausstab == NULL) {
		updateSize();
		float rad = max_ff(m_size * m_data->sizey, 0.0f);
		m_filtersize = min_ii(ceil(rad), MAX_GAUSSTAB_RADIUS);
		
		this->m_gausstab = BlurBaseOperation::make_gausstab(rad, m_filtersize);
	}
}

void GaussianYBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	float color_accum[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	float multiplier_accum = 0.0f;
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();
	int bufferwidth = inputBuffer->getWidth();
	int bufferstartx = inputBuffer->getRect()->xmin;
	int bufferstarty = inputBuffer->getRect()->ymin;

	rcti &rect = *inputBuffer->getRect();
	int xmin = max_ii(x,                    rect.xmin);
	int ymin = max_ii(y - m_filtersize,     rect.ymin);
	int ymax = min_ii(y + m_filtersize + 1, rect.ymax);

	int index;
	int step = getStep();
	const int bufferIndexx = ((xmin - bufferstartx) * 4);
	for (int ny = ymin; ny < ymax; ny += step) {
		index = (ny - y) + this->m_filtersize;
		int bufferindex = bufferIndexx + ((ny - bufferstarty) * 4 * bufferwidth);
		const float multiplier = this->m_gausstab[index];
		madd_v4_v4fl(color_accum, &buffer[bufferindex], multiplier);
		multiplier_accum += multiplier;
	}
	mul_v4_v4fl(output, color_accum, 1.0f / multiplier_accum);
}

void GaussianYBlurOperation::deinitExecution()
{
	BlurBaseOperation::deinitExecution();
	if (this->m_gausstab) {
		MEM_freeN(this->m_gausstab);
		this->m_gausstab = NULL;
	}

	deinitMutex();
}

bool GaussianYBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	
	if (!m_sizeavailable) {
		rcti sizeInput;
		sizeInput.xmin = 0;
		sizeInput.ymin = 0;
		sizeInput.xmax = 5;
		sizeInput.ymax = 5;
		NodeOperation *operation = this->getInputOperation(1);
		if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
			return true;
		}
	}
	{
		if (this->m_sizeavailable && this->m_gausstab != NULL) {
			newInput.xmax = input->xmax;
			newInput.xmin = input->xmin;
			newInput.ymax = input->ymax + this->m_filtersize + 1;
			newInput.ymin = input->ymin - this->m_filtersize - 1;
		}
		else {
			newInput.xmax = this->getWidth();
			newInput.xmin = 0;
			newInput.ymax = this->getHeight();
			newInput.ymin = 0;
		}
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}
