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

#include "COM_GaussianBokehBlurOperation.h"
#include "BLI_math.h"
#include "MEM_guardedalloc.h"
extern "C" {
#  include "RE_pipeline.h"
}

GaussianBokehBlurOperation::GaussianBokehBlurOperation() : BlurBaseOperation(COM_DT_COLOR)
{
	this->m_gausstab = NULL;
}

void *GaussianBokehBlurOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (!this->m_sizeavailable) {
		updateGauss();
	}
	void *buffer = getInputOperation(0)->initializeTileData(NULL);
	unlockMutex();
	return buffer;
}

void GaussianBokehBlurOperation::initExecution()
{
	BlurBaseOperation::initExecution();

	initMutex();

	if (this->m_sizeavailable) {
		updateGauss();
	}
}

void GaussianBokehBlurOperation::updateGauss()
{
	if (this->m_gausstab == NULL) {
		float radxf;
		float radyf;
		int n;
		float *dgauss;
		float *ddgauss;
		int j, i;
		const float width = this->getWidth();
		const float height = this->getHeight();
		if (!this->m_sizeavailable) {
			updateSize();
		}
		radxf = this->m_size * (float)this->m_data.sizex;
		CLAMP(radxf, 0.0f, width / 2.0f);
	
		/* vertical */
		radyf = this->m_size * (float)this->m_data.sizey;
		CLAMP(radyf, 0.0f, height / 2.0f);
	
		this->m_radx = ceil(radxf);
		this->m_rady = ceil(radyf);
		
		int ddwidth = 2 * this->m_radx + 1;
		int ddheight = 2 * this->m_rady + 1;
		n = ddwidth * ddheight;
	
		/* create a full filter image */
		ddgauss = (float *)MEM_mallocN(sizeof(float) * n, __func__);
		dgauss = ddgauss;
		float sum = 0.0f;
		float facx = (radxf > 0.0f ? 1.0f / radxf : 0.0f);
		float facy = (radyf > 0.0f ? 1.0f / radyf : 0.0f);
		for (j = -this->m_rady; j <= this->m_rady; j++) {
			for (i = -this->m_radx; i <= this->m_radx; i++, dgauss++) {
				float fj = (float)j * facy;
				float fi = (float)i * facx;
				float dist = sqrt(fj * fj + fi * fi);
				*dgauss = RE_filter_value(this->m_data.filtertype, dist);
				
				sum += *dgauss;
			}
		}

		if (sum > 0.0f) {
			/* normalize */
			float norm = 1.0f / sum;
			for (j = n - 1; j >= 0; j--)
				ddgauss[j] *= norm;
		}
		else {
			int center = m_rady * ddwidth + m_radx;
			ddgauss[center] = 1.0f;
		}
		
		this->m_gausstab = ddgauss;
	}
}

void GaussianBokehBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	float tempColor[4];
	tempColor[0] = 0;
	tempColor[1] = 0;
	tempColor[2] = 0;
	tempColor[3] = 0;
	float multiplier_accum = 0;
	MemoryBuffer *inputBuffer = (MemoryBuffer *)data;
	float *buffer = inputBuffer->getBuffer();
	int bufferwidth = inputBuffer->getWidth();
	int bufferstartx = inputBuffer->getRect()->xmin;
	int bufferstarty = inputBuffer->getRect()->ymin;

	rcti &rect = *inputBuffer->getRect();
	int ymin = max_ii(y - this->m_rady, rect.ymin);
	int ymax = min_ii(y + this->m_rady + 1,  rect.ymax);
	int xmin = max_ii(x - this->m_radx, rect.xmin);
	int xmax = min_ii(x + this->m_radx + 1,  rect.xmax);

	int index;
	int step = QualityStepHelper::getStep();
	int offsetadd = QualityStepHelper::getOffsetAdd();
	const int addConst = (xmin - x + this->m_radx);
	const int mulConst = (this->m_radx * 2 + 1);
	for (int ny = ymin; ny < ymax; ny += step) {
		index = ((ny - y) + this->m_rady) * mulConst + addConst;
		int bufferindex = ((xmin - bufferstartx) * 4) + ((ny - bufferstarty) * 4 * bufferwidth);
		for (int nx = xmin; nx < xmax; nx += step) {
			const float multiplier = this->m_gausstab[index];
			madd_v4_v4fl(tempColor, &buffer[bufferindex], multiplier);
			multiplier_accum += multiplier;
			index += step;
			bufferindex += offsetadd;
		}
	}

	mul_v4_v4fl(output, tempColor, 1.0f / multiplier_accum);
}

void GaussianBokehBlurOperation::deinitExecution()
{
	BlurBaseOperation::deinitExecution();
	MEM_freeN(this->m_gausstab);
	this->m_gausstab = NULL;

	deinitMutex();
}

bool GaussianBokehBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	rcti sizeInput;
	sizeInput.xmin = 0;
	sizeInput.ymin = 0;
	sizeInput.xmax = 5;
	sizeInput.ymax = 5;
	NodeOperation *operation = this->getInputOperation(1);
	
	if (operation->determineDependingAreaOfInterest(&sizeInput, readOperation, output)) {
		return true;
	}
	else {
		if (this->m_sizeavailable && this->m_gausstab != NULL) {
			newInput.xmin = 0;
			newInput.ymin = 0;
			newInput.xmax = this->getWidth();
			newInput.ymax = this->getHeight();
		}
		else {
			int addx = this->m_radx;
			int addy = this->m_rady;
			newInput.xmax = input->xmax + addx;
			newInput.xmin = input->xmin - addx;
			newInput.ymax = input->ymax + addy;
			newInput.ymin = input->ymin - addy;

		}
		return BlurBaseOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}

// reference image
GaussianBlurReferenceOperation::GaussianBlurReferenceOperation() : BlurBaseOperation(COM_DT_COLOR)
{
	this->m_maintabs = NULL;
}

void *GaussianBlurReferenceOperation::initializeTileData(rcti *rect)
{
	void *buffer = getInputOperation(0)->initializeTileData(NULL);
	return buffer;
}

void GaussianBlurReferenceOperation::initExecution()
{
	BlurBaseOperation::initExecution();
	// setup gaustab
	this->m_data.image_in_width = this->getWidth();
	this->m_data.image_in_height = this->getHeight();
	if (this->m_data.relative) {
		switch (this->m_data.aspect) {
			case CMP_NODE_BLUR_ASPECT_NONE:
				this->m_data.sizex = (int)(this->m_data.percentx * 0.01f * this->m_data.image_in_width);
				this->m_data.sizey = (int)(this->m_data.percenty * 0.01f * this->m_data.image_in_height);
				break;
			case CMP_NODE_BLUR_ASPECT_Y:
				this->m_data.sizex = (int)(this->m_data.percentx * 0.01f * this->m_data.image_in_width);
				this->m_data.sizey = (int)(this->m_data.percenty * 0.01f * this->m_data.image_in_width);
				break;
			case CMP_NODE_BLUR_ASPECT_X:
				this->m_data.sizex = (int)(this->m_data.percentx * 0.01f * this->m_data.image_in_height);
				this->m_data.sizey = (int)(this->m_data.percenty * 0.01f * this->m_data.image_in_height);
				break;
		}
	}
	
	
	/* horizontal */
	m_filtersizex = (float)this->m_data.sizex;
	int imgx = getWidth() / 2;
	if (m_filtersizex > imgx)
		m_filtersizex = imgx;
	else if (m_filtersizex < 1)
		m_filtersizex = 1;
	m_radx = (float)m_filtersizex;

	/* vertical */
	m_filtersizey = (float)this->m_data.sizey;
	int imgy = getHeight() / 2;
	if (m_filtersizey > imgy)
		m_filtersizey = imgy;
	else if (m_filtersizey < 1)
		m_filtersizey = 1;
	m_rady = (float)m_filtersizey;
	updateGauss();
}

void GaussianBlurReferenceOperation::updateGauss()
{
	int i;
	int x = max(m_filtersizex, m_filtersizey);
	m_maintabs = (float **)MEM_mallocN(x * sizeof(float *), "gauss array");
	for (i = 0; i < x; i++) {
		m_maintabs[i] = make_gausstab(i + 1, i + 1);
	}
}

void GaussianBlurReferenceOperation::executePixel(float output[4], int x, int y, void *data)
{
	MemoryBuffer *memorybuffer = (MemoryBuffer *)data;
	float *buffer = memorybuffer->getBuffer();
	float *gausstabx, *gausstabcenty;
	float *gausstaby, *gausstabcentx;
	int i, j;
	float *src;
	register float sum, val;
	float rval, gval, bval, aval;
	int imgx = getWidth();
	int imgy = getHeight();
	float tempSize[4];
	this->m_inputSize->read(tempSize, x, y, data);
	float refSize = tempSize[0];
	int refradx = (int)(refSize * m_radx);
	int refrady = (int)(refSize * m_rady);
	if (refradx > m_filtersizex) refradx = m_filtersizex;
	else if (refradx < 1) refradx = 1;
	if (refrady > m_filtersizey) refrady = m_filtersizey;
	else if (refrady < 1) refrady = 1;

	if (refradx == 1 && refrady == 1) {
		memorybuffer->readNoCheck(output, x, y);
	}
	else {
		int minxr = x - refradx < 0 ? -x : -refradx;
		int maxxr = x + refradx > imgx ? imgx - x : refradx;
		int minyr = y - refrady < 0 ? -y : -refrady;
		int maxyr = y + refrady > imgy ? imgy - y : refrady;

		float *srcd = buffer + COM_NUMBER_OF_CHANNELS * ( (y + minyr) * imgx + x + minxr);

		gausstabx = m_maintabs[refradx - 1];
		gausstabcentx = gausstabx + refradx;
		gausstaby = m_maintabs[refrady - 1];
		gausstabcenty = gausstaby + refrady;

		sum = gval = rval = bval = aval = 0.0f;
		for (i = minyr; i < maxyr; i++, srcd += COM_NUMBER_OF_CHANNELS * imgx) {
			src = srcd;
			for (j = minxr; j < maxxr; j++, src += COM_NUMBER_OF_CHANNELS) {
			
				val = gausstabcenty[i] * gausstabcentx[j];
				sum += val;
				rval += val * src[0];
				gval += val * src[1];
				bval += val * src[2];
				aval += val * src[3];
			}
		}
		sum = 1.0f / sum;
		output[0] = rval * sum;
		output[1] = gval * sum;
		output[2] = bval * sum;
		output[3] = aval * sum;
	}

}

void GaussianBlurReferenceOperation::deinitExecution()
{
	int x, i;
	x = max(this->m_filtersizex, this->m_filtersizey);
	for (i = 0; i < x; i++) {
		MEM_freeN(this->m_maintabs[i]);
	}
	MEM_freeN(this->m_maintabs);
	BlurBaseOperation::deinitExecution();
}

bool GaussianBlurReferenceOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	NodeOperation *operation = this->getInputOperation(1);
	
	if (operation->determineDependingAreaOfInterest(input, readOperation, output)) {
		return true;
	}
	else {
		int addx = this->m_data.sizex + 2;
		int addy = this->m_data.sizey + 2;
		newInput.xmax = input->xmax + addx;
		newInput.xmin = input->xmin - addx;
		newInput.ymax = input->ymax + addy;
		newInput.ymin = input->ymin - addy;
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}

