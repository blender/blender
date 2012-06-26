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

#include <limits.h>

#include "COM_FastGaussianBlurOperation.h"
#include "MEM_guardedalloc.h"
#include "BLI_utildefines.h"

FastGaussianBlurOperation::FastGaussianBlurOperation() : BlurBaseOperation(COM_DT_COLOR)
{
	this->m_iirgaus = NULL;
}

void FastGaussianBlurOperation::executePixel(float *color, int x, int y, MemoryBuffer *inputBuffers[], void *data)
{
	MemoryBuffer *newData = (MemoryBuffer *)data;
	newData->read(color, x, y);	
}

bool FastGaussianBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
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
		if (this->m_iirgaus) {
			return false;
		}
		else {
			newInput.xmin = 0;
			newInput.ymin = 0;
			newInput.xmax = this->getWidth();
			newInput.ymax = this->getHeight();
		}
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
}

void FastGaussianBlurOperation::initExecution()
{
	BlurBaseOperation::initExecution();
	BlurBaseOperation::initMutex();
}

void FastGaussianBlurOperation::deinitExecution() 
{
	if (this->m_iirgaus) {
		delete this->m_iirgaus;
		this->m_iirgaus = NULL;
	}
	BlurBaseOperation::deinitMutex();
}

void *FastGaussianBlurOperation::initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers)
{
	lockMutex();
	if (!this->m_iirgaus) {
		MemoryBuffer *newBuf = (MemoryBuffer *)this->m_inputProgram->initializeTileData(rect, memoryBuffers);
		MemoryBuffer *copy = newBuf->duplicate();
		updateSize(memoryBuffers);

		int c;
		this->m_sx = this->m_data->sizex * this->m_size / 2.0f;
		this->m_sy = this->m_data->sizey * this->m_size / 2.0f;
		
		if ((this->m_sx == this->m_sy) && (this->m_sx > 0.f)) {
			for (c = 0; c < COM_NUMBER_OF_CHANNELS; ++c)
				IIR_gauss(copy, this->m_sx, c, 3);
		}
		else {
			if (this->m_sx > 0.0f) {
				for (c = 0; c < COM_NUMBER_OF_CHANNELS; ++c)
					IIR_gauss(copy, this->m_sx, c, 1);
			}
			if (this->m_sy > 0.0f) {
				for (c = 0; c < COM_NUMBER_OF_CHANNELS; ++c)
					IIR_gauss(copy, this->m_sy, c, 2);
			}
		}
		this->m_iirgaus = copy;
	}
	unlockMutex();
	return this->m_iirgaus;
}

void FastGaussianBlurOperation::IIR_gauss(MemoryBuffer *src, float sigma, unsigned int chan, unsigned int xy)
{
	double q, q2, sc, cf[4], tsM[9], tsu[3], tsv[3];
	double *X, *Y, *W;
	const unsigned int src_width = src->getWidth();
	const unsigned int src_height = src->getHeight();
	unsigned int x, y, sz;
	unsigned int i;
	float *buffer = src->getBuffer();
	
	// <0.5 not valid, though can have a possibly useful sort of sharpening effect
	if (sigma < 0.5f) return;
	
	if ((xy < 1) || (xy > 3)) xy = 3;
	
	// XXX The YVV macro defined below explicitly expects sources of at least 3x3 pixels,
	//     so just skiping blur along faulty direction if src's def is below that limit!
	if (src_width < 3) xy &= ~(int) 1;
	if (src_height < 3) xy &= ~(int) 2;
	if (xy < 1) return;
	
	// see "Recursive Gabor Filtering" by Young/VanVliet
	// all factors here in double.prec. Required, because for single.prec it seems to blow up if sigma > ~200
	if (sigma >= 3.556f)
		q = 0.9804f * (sigma - 3.556f) + 2.5091f;
	else // sigma >= 0.5
		q = (0.0561f * sigma + 0.5784f) * sigma - 0.2568f;
	q2 = q * q;
	sc = (1.1668 + q) * (3.203729649  + (2.21566 + q) * q);
	// no gabor filtering here, so no complex multiplies, just the regular coefs.
	// all negated here, so as not to have to recalc Triggs/Sdika matrix
	cf[1] = q * (5.788961737 + (6.76492 + 3.0 * q) * q) / sc;
	cf[2] = -q2 * (3.38246 + 3.0 * q) / sc;
	// 0 & 3 unchanged
	cf[3] = q2 * q / sc;
	cf[0] = 1.0 - cf[1] - cf[2] - cf[3];
	
	// Triggs/Sdika border corrections,
	// it seems to work, not entirely sure if it is actually totally correct,
	// Besides J.M.Geusebroek's anigauss.c (see http://www.science.uva.nl/~mark),
	// found one other implementation by Cristoph Lampert,
	// but neither seem to be quite the same, result seems to be ok so far anyway.
	// Extra scale factor here to not have to do it in filter,
	// though maybe this had something to with the precision errors
	sc = cf[0] / ((1.0 + cf[1] - cf[2] + cf[3]) * (1.0 - cf[1] - cf[2] - cf[3]) * (1.0 + cf[2] + (cf[1] - cf[3]) * cf[3]));
	tsM[0] = sc * (-cf[3] * cf[1] + 1.0 - cf[3] * cf[3] - cf[2]);
	tsM[1] = sc * ((cf[3] + cf[1]) * (cf[2] + cf[3] * cf[1]));
	tsM[2] = sc * (cf[3] * (cf[1] + cf[3] * cf[2]));
	tsM[3] = sc * (cf[1] + cf[3] * cf[2]);
	tsM[4] = sc * (-(cf[2] - 1.0) * (cf[2] + cf[3] * cf[1]));
	tsM[5] = sc * (-(cf[3] * cf[1] + cf[3] * cf[3] + cf[2] - 1.0) * cf[3]);
	tsM[6] = sc * (cf[3] * cf[1] + cf[2] + cf[1] * cf[1] - cf[2] * cf[2]);
	tsM[7] = sc * (cf[1] * cf[2] + cf[3] * cf[2] * cf[2] - cf[1] * cf[3] * cf[3] - cf[3] * cf[3] * cf[3] - cf[3] * cf[2] + cf[3]);
	tsM[8] = sc * (cf[3] * (cf[1] + cf[3] * cf[2]));
	
#define YVV(L)                                                                          \
{                                                                                       \
	W[0] = cf[0] * X[0] + cf[1] * X[0] + cf[2] * X[0] + cf[3] * X[0];                   \
	W[1] = cf[0] * X[1] + cf[1] * W[0] + cf[2] * X[0] + cf[3] * X[0];                   \
	W[2] = cf[0] * X[2] + cf[1] * W[1] + cf[2] * W[0] + cf[3] * X[0];                   \
	for (i = 3; i < L; i++) {                                                           \
		W[i] = cf[0] * X[i] + cf[1] * W[i - 1] + cf[2] * W[i - 2] + cf[3] * W[i - 3];   \
	}                                                                                   \
	tsu[0] = W[L - 1] - X[L - 1];                                                       \
	tsu[1] = W[L - 2] - X[L - 1];                                                       \
	tsu[2] = W[L - 3] - X[L - 1];                                                       \
	tsv[0] = tsM[0] * tsu[0] + tsM[1] * tsu[1] + tsM[2] * tsu[2] + X[L - 1];            \
	tsv[1] = tsM[3] * tsu[0] + tsM[4] * tsu[1] + tsM[5] * tsu[2] + X[L - 1];            \
	tsv[2] = tsM[6] * tsu[0] + tsM[7] * tsu[1] + tsM[8] * tsu[2] + X[L - 1];            \
	Y[L - 1] = cf[0] * W[L - 1] + cf[1] * tsv[0] + cf[2] * tsv[1] + cf[3] * tsv[2];     \
	Y[L - 2] = cf[0] * W[L - 2] + cf[1] * Y[L - 1] + cf[2] * tsv[0] + cf[3] * tsv[1];   \
	Y[L - 3] = cf[0] * W[L - 3] + cf[1] * Y[L - 2] + cf[2] * Y[L - 1] + cf[3] * tsv[0]; \
	/* 'i != UINT_MAX' is really 'i >= 0', but necessary for unsigned int wrapping */   \
	for (i = L - 4; i != UINT_MAX; i--) {                                               \
		Y[i] = cf[0] * W[i] + cf[1] * Y[i + 1] + cf[2] * Y[i + 2] + cf[3] * Y[i + 3];   \
	}                                                                                   \
} (void)0
	
	// intermediate buffers
	sz = MAX2(src_width, src_height);
	X = (double *)MEM_callocN(sz * sizeof(double), "IIR_gauss X buf");
	Y = (double *)MEM_callocN(sz * sizeof(double), "IIR_gauss Y buf");
	W = (double *)MEM_callocN(sz * sizeof(double), "IIR_gauss W buf");
	if (xy & 1) {   // H
		for (y = 0; y < src_height; ++y) {
			const int yx = y * src_width;
			for (x = 0; x < src_width; ++x)
				X[x] = buffer[(x + yx) * COM_NUMBER_OF_CHANNELS + chan];
			YVV(src_width);
			for (x = 0; x < src_width; ++x)
				buffer[(x + yx) * COM_NUMBER_OF_CHANNELS + chan] = Y[x];
		}
	}
	if (xy & 2) {   // V
		for (x = 0; x < src_width; ++x) {
			for (y = 0; y < src_height; ++y)
				X[y] = buffer[(x + y * src_width) * COM_NUMBER_OF_CHANNELS + chan];
			YVV(src_height);
			for (y = 0; y < src_height; ++y)
				buffer[(x + y * src_width) * COM_NUMBER_OF_CHANNELS + chan] = Y[y];
		}
	}
	
	MEM_freeN(X);
	MEM_freeN(W);
	MEM_freeN(Y);
#undef YVV
	
}
