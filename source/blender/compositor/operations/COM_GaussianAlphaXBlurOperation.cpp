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
 *		Campbell Barton
 */

#include "COM_GaussianAlphaXBlurOperation.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

GaussianAlphaXBlurOperation::GaussianAlphaXBlurOperation() : BlurBaseOperation(COM_DT_VALUE)
{
	this->gausstab = NULL;
	this->rad = 0;
}

void *GaussianAlphaXBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	lockMutex();
	if (!this->sizeavailable) {
		updateGauss(memoryBuffers);
	}
	void *buffer = getInputOperation(0)->initializeTileData(NULL, memoryBuffers);
	unlockMutex();
	return buffer;
}

void GaussianAlphaXBlurOperation::initExecution()
{
	/* BlurBaseOperation::initExecution(); */ /* until we suppoer size input - comment this */

	initMutex();

	if (this->sizeavailable) {
		float rad = size * this->data->sizex;
		if (rad < 1)
			rad = 1;

		this->rad = rad;
		this->gausstab = BlurBaseOperation::make_gausstab(rad);
		this->distbuf_inv = BlurBaseOperation::make_dist_fac_inverse(rad, this->falloff);
	}
}

void GaussianAlphaXBlurOperation::updateGauss(MemoryBuffer **memoryBuffers)
{
	if (this->gausstab == NULL) {
		updateSize(memoryBuffers);
		float rad = size * this->data->sizex;
		if (rad < 1)
			rad = 1;

		this->rad = rad;
		this->gausstab = BlurBaseOperation::make_gausstab(rad);	
	}

	if (this->distbuf_inv == NULL) {
		updateSize(memoryBuffers);
		float rad = size * this->data->sizex;
		if (rad < 1)
			rad = 1;

		this->rad = rad;
		this->distbuf_inv = BlurBaseOperation::make_dist_fac_inverse(rad, this->falloff);
	}
}

BLI_INLINE float finv_test(const float f, const bool test)
{
	return (LIKELY(test == false)) ? f : 1.0f - f;
}

void GaussianAlphaXBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	const bool do_invert = this->do_subtract;
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();
	int bufferwidth = inputBuffer->getWidth();
	int bufferstartx = inputBuffer->getRect()->xmin;
	int bufferstarty = inputBuffer->getRect()->ymin;

	int miny = y;
	int maxy = y;
	int minx = x - this->rad;
	int maxx = x + this->rad;
	miny = max(miny, inputBuffer->getRect()->ymin);
	minx = max(minx, inputBuffer->getRect()->xmin);
	maxy = min(maxy, inputBuffer->getRect()->ymax);
	maxx = min(maxx, inputBuffer->getRect()->xmax);

	/* *** this is the main part which is different to 'GaussianXBlurOperation'  *** */
	int step = getStep();
	int offsetadd = getOffsetAdd();
	int bufferindex = ((minx - bufferstartx) * 4) + ((miny - bufferstarty) * 4 * bufferwidth);

	/* gauss */
	float alpha_accum = 0.0f;
	float multiplier_accum = 0.0f;

	/* dilate */
	float value_max = finv_test(buffer[(x * 4) + (y * 4 * bufferwidth)], do_invert); /* init with the current color to avoid unneeded lookups */
	float distfacinv_max = 1.0f; /* 0 to 1 */

	for (int nx = minx; nx < maxx; nx += step) {
		const int index = (nx - x) + this->rad;
		float value = finv_test(buffer[bufferindex], do_invert);
		float multiplier;

		/* gauss */
		{
			multiplier = gausstab[index];
			alpha_accum += value * multiplier;
			multiplier_accum += multiplier;
		}

		/* dilate - find most extreme color */
		if (value > value_max) {
			multiplier = distbuf_inv[index];
			value *= multiplier;
			if (value > value_max) {
				value_max = value;
				distfacinv_max = multiplier;
			}
		}
		bufferindex += offsetadd;
	}

	/* blend between the max value and gauss blue - gives nice feather */
	const float value_blur  = alpha_accum / multiplier_accum;
	const float value_final = (value_max * distfacinv_max) + (value_blur * (1.0f - distfacinv_max));
	color[0] = finv_test(value_final, do_invert);
}

void GaussianAlphaXBlurOperation::deinitExecution()
{
	BlurBaseOperation::deinitExecution();
	delete [] this->gausstab;
	this->gausstab = NULL;
	delete [] this->distbuf_inv;
	this->distbuf_inv = NULL;

	deinitMutex();
}

bool GaussianAlphaXBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
#if 0 /* until we add size input */
	rcti sizeInput;
	sizeInput.xmin = 0;
	sizeInput.ymin = 0;
	sizeInput.xmax = 5;
	sizeInput.ymax = 5;

	NodeOperation *operation = this->getInputOperation(1);
	if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
		return true;
	}
	else
#endif
	{
		if (this->sizeavailable && this->gausstab != NULL) {
			newInput.xmax = input->xmax + rad;
			newInput.xmin = input->xmin - rad;
			newInput.ymax = input->ymax;
			newInput.ymin = input->ymin;
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
