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

#include "COM_TonemapOperation.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"



TonemapOperation::TonemapOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR, COM_SC_NO_RESIZE);
	this->addOutputSocket(COM_DT_COLOR);
	this->m_imageReader = NULL;
	this->m_data = NULL;
	this->m_cachedInstance = NULL;
	this->setComplex(true);
}
void TonemapOperation::initExecution()
{
	this->m_imageReader = this->getInputSocketReader(0);
	NodeOperation::initMutex();
}

void TonemapOperation::executePixel(float output[4], int x, int y, void *data)
{
	AvgLogLum *avg = (AvgLogLum *)data;

	this->m_imageReader->read(output, x, y, NULL);
	mul_v3_fl(output, avg->al);
	float dr = output[0] + this->m_data->offset;
	float dg = output[1] + this->m_data->offset;
	float db = output[2] + this->m_data->offset;
	output[0] /= ((dr == 0.f) ? 1.0f : dr);
	output[1] /= ((dg == 0.f) ? 1.0f : dg);
	output[2] /= ((db == 0.f) ? 1.0f : db);
	const float igm = avg->igm;
	if (igm != 0.0f) {
		output[0] = powf(max(output[0], 0.0f), igm);
		output[1] = powf(max(output[1], 0.0f), igm);
		output[2] = powf(max(output[2], 0.0f), igm);
	}
}
void PhotoreceptorTonemapOperation::executePixel(float output[4], int x, int y, void *data)
{
	AvgLogLum *avg = (AvgLogLum *)data;
	NodeTonemap *ntm = this->m_data;

	const float f = expf(-this->m_data->f);
	const float m = (ntm->m > 0.0f) ? ntm->m : (0.3f + 0.7f * powf(avg->auto_key, 1.4f));
	const float ic = 1.0f - ntm->c, ia = 1.0f - ntm->a;

	this->m_imageReader->read(output, x, y, NULL);

	const float L = rgb_to_luma_y(output);
	float I_l = output[0] + ic * (L - output[0]);
	float I_g = avg->cav[0] + ic * (avg->lav - avg->cav[0]);
	float I_a = I_l + ia * (I_g - I_l);
	output[0] /= (output[0] + powf(f * I_a, m));
	I_l = output[1] + ic * (L - output[1]);
	I_g = avg->cav[1] + ic * (avg->lav - avg->cav[1]);
	I_a = I_l + ia * (I_g - I_l);
	output[1] /= (output[1] + powf(f * I_a, m));
	I_l = output[2] + ic * (L - output[2]);
	I_g = avg->cav[2] + ic * (avg->lav - avg->cav[2]);
	I_a = I_l + ia * (I_g - I_l);
	output[2] /= (output[2] + powf(f * I_a, m));
}

void TonemapOperation::deinitExecution()
{
	this->m_imageReader = NULL;
	if (this->m_cachedInstance) {
		delete this->m_cachedInstance;
	}
	NodeOperation::deinitMutex();
}

bool TonemapOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti imageInput;

	NodeOperation *operation = getInputOperation(0);
	imageInput.xmax = operation->getWidth();
	imageInput.xmin = 0;
	imageInput.ymax = operation->getHeight();
	imageInput.ymin = 0;
	if (operation->determineDependingAreaOfInterest(&imageInput, readOperation, output) ) {
		return true;
	}
	return false;
}

void *TonemapOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (this->m_cachedInstance == NULL) {
		MemoryBuffer *tile = (MemoryBuffer *)this->m_imageReader->initializeTileData(rect);
		AvgLogLum *data = new AvgLogLum();

		float *buffer = tile->getBuffer();

		float lsum = 0.0f;
		int p = tile->getWidth() * tile->getHeight();
		float *bc = buffer;
		float avl, maxl = -1e10f, minl = 1e10f;
		const float sc = 1.0f / p;
		float Lav = 0.f;
		float cav[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		while (p--) {
			float L = rgb_to_luma_y(bc);
			Lav += L;
			add_v3_v3(cav, bc);
			lsum += logf(MAX2(L, 0.0f) + 1e-5f);
			maxl = (L > maxl) ? L : maxl;
			minl = (L < minl) ? L : minl;
			bc += 4;
		}
		data->lav = Lav * sc;
		mul_v3_v3fl(data->cav, cav, sc);
		maxl = log((double)maxl + 1e-5); minl = log((double)minl + 1e-5); avl = lsum * sc;
		data->auto_key = (maxl > minl) ? ((maxl - avl) / (maxl - minl)) : 1.f;
		float al = exp((double)avl);
		data->al = (al == 0.0f) ? 0.0f : (this->m_data->key / al);
		data->igm = (this->m_data->gamma == 0.f) ? 1 : (1.f / this->m_data->gamma);
		this->m_cachedInstance = data;
	}
	unlockMutex();
	return this->m_cachedInstance;
}

void TonemapOperation::deinitializeTileData(rcti *rect, void *data)
{
	/* pass */
}
