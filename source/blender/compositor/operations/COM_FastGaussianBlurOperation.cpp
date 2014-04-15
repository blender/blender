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
	this->m_chunksize = 256;
}

void FastGaussianBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	MemoryBuffer *newData = (MemoryBuffer *)data;
	newData->read(output, x, y);
}

// Calculate the depending area of interest. This depends on the
// size of the blur operation; if the blur is large it is faster
// to just calculate the whole image at once.
// Returns true if the area is just a tile and false if it is
// the whole image.
bool FastGaussianBlurOperation::getDAI(rcti *rect, rcti *output)
{
	// m_data.sizex * m_size should be enough? For some reason there
	// seem to be errors in the boundary between tiles.
	float size = this->m_size * COM_FAST_GAUSSIAN_MULTIPLIER;
	int sx = this->m_data.sizex * size;
	if (sx < 1)
		sx = 1;
	int sy = this->m_data.sizey * size;
	if (sy < 1)
		sy = 1;

	if (sx >= this->m_chunksize || sy >= this->m_chunksize) {
		output->xmin = 0;
		output->xmax = this->getWidth();
		output->ymin = 0;
		output->ymax = this->getHeight();
		return false;
	}
	else {
		output->xmin = rect->xmin - sx - 1;
		output->xmax = rect->xmax + sx + 1;
		output->ymin = rect->ymin - sy - 1;
		output->ymax = rect->ymax + sy + 1;
		return true;
	}
}

bool FastGaussianBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	
	if (!this->m_sizeavailable) {
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
		if (this->m_sizeavailable) {
			getDAI(input, &newInput);
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

void *FastGaussianBlurOperation::initializeTileData(rcti *rect)
{
#if 0
	lockMutex();
	if (!this->m_iirgaus) {
		MemoryBuffer *newBuf = (MemoryBuffer *)this->m_inputProgram->initializeTileData(rect);
		MemoryBuffer *copy = newBuf->duplicate();
		updateSize();

		int c;
		this->m_sx = this->m_data.sizex * this->m_size / 2.0f;
		this->m_sy = this->m_data.sizey * this->m_size / 2.0f;
		
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
#else

	lockMutex();
	if (this->m_iirgaus) {
		// if this->m_iirgaus is set, we don't do tile rendering, so
		// we can return the already calculated cache
		unlockMutex();
		return this->m_iirgaus;
	}
	updateSize();
	rcti dai;
	bool use_tiles = getDAI(rect, &dai);
	if (use_tiles) {
		unlockMutex();
	}

	MemoryBuffer *buffer = (MemoryBuffer *)this->m_inputProgram->initializeTileData(NULL);
	rcti *buf_rect = buffer->getRect();

	dai.xmin = max(dai.xmin, buf_rect->xmin);
	dai.xmax = min(dai.xmax, buf_rect->xmax);
	dai.ymin = max(dai.ymin, buf_rect->ymin);
	dai.ymax = min(dai.ymax, buf_rect->ymax);

	MemoryBuffer *tile = new MemoryBuffer(NULL, &dai);
	tile->copyContentFrom(buffer);

	int c;
	float sx = this->m_data.sizex * this->m_size / 2.0f;
	float sy = this->m_data.sizey * this->m_size / 2.0f;

	if ((sx == sy) && (sx > 0.f)) {
		for (c = 0; c < COM_NUMBER_OF_CHANNELS; ++c)
			IIR_gauss(tile, sx, c, 3);
	}
	else {
		if (sx > 0.0f) {
			for (c = 0; c < COM_NUMBER_OF_CHANNELS; ++c)
				IIR_gauss(tile, sx, c, 1);
		}
		if (sy > 0.0f) {
			for (c = 0; c < COM_NUMBER_OF_CHANNELS; ++c)
				IIR_gauss(tile, sy, c, 2);
		}
	}
	if (!use_tiles) {
		this->m_iirgaus = tile;
		unlockMutex();
	}
	return tile;
#endif
}

void FastGaussianBlurOperation::deinitializeTileData(rcti *rect, void *data)
{
	if (!this->m_iirgaus && data) {
		MemoryBuffer *tile = (MemoryBuffer *)data;
		delete tile;
	}
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
	if (src_width < 3) xy &= ~1;
	if (src_height < 3) xy &= ~2;
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
	sz = max(src_width, src_height);
	X = (double *)MEM_callocN(sz * sizeof(double), "IIR_gauss X buf");
	Y = (double *)MEM_callocN(sz * sizeof(double), "IIR_gauss Y buf");
	W = (double *)MEM_callocN(sz * sizeof(double), "IIR_gauss W buf");
	if (xy & 1) {   // H
		int offset;
		for (y = 0; y < src_height; ++y) {
			const int yx = y * src_width;
			offset = yx * COM_NUMBER_OF_CHANNELS + chan;
			for (x = 0; x < src_width; ++x) {
				X[x] = buffer[offset];
				offset += COM_NUMBER_OF_CHANNELS;
			}
			YVV(src_width);
			offset = yx * COM_NUMBER_OF_CHANNELS + chan;
			for (x = 0; x < src_width; ++x) {
				buffer[offset] = Y[x];
				offset += COM_NUMBER_OF_CHANNELS;
			}
		}
	}
	if (xy & 2) {   // V
		int offset;
		const int add = src_width * COM_NUMBER_OF_CHANNELS;

		for (x = 0; x < src_width; ++x) {
			offset = x * COM_NUMBER_OF_CHANNELS + chan;
			for (y = 0; y < src_height; ++y) {
				X[y] = buffer[offset];
				offset += add;
			}
			YVV(src_height);
			offset = x * COM_NUMBER_OF_CHANNELS + chan;
			for (y = 0; y < src_height; ++y) {
				buffer[offset] = Y[y];
				offset += add;
			}
		}
	}
	
	MEM_freeN(X);
	MEM_freeN(W);
	MEM_freeN(Y);
#undef YVV
	
}


///
FastGaussianBlurValueOperation::FastGaussianBlurValueOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
	this->m_iirgaus = NULL;
	this->m_inputprogram = NULL;
	this->m_sigma = 1.0f;
	this->m_overlay = 0;
	setComplex(true);
}

void FastGaussianBlurValueOperation::executePixel(float output[4], int x, int y, void *data)
{
	MemoryBuffer *newData = (MemoryBuffer *)data;
	newData->read(output, x, y);
}

bool FastGaussianBlurValueOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output)
{
	rcti newInput;
	
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

void FastGaussianBlurValueOperation::initExecution()
{
	this->m_inputprogram = getInputSocketReader(0);
	initMutex();
}

void FastGaussianBlurValueOperation::deinitExecution() 
{
	if (this->m_iirgaus) {
		delete this->m_iirgaus;
		this->m_iirgaus = NULL;
	}
	deinitMutex();
}

void *FastGaussianBlurValueOperation::initializeTileData(rcti *rect)
{
	lockMutex();
	if (!this->m_iirgaus) {
		MemoryBuffer *newBuf = (MemoryBuffer *)this->m_inputprogram->initializeTileData(rect);
		MemoryBuffer *copy = newBuf->duplicate();
		FastGaussianBlurOperation::IIR_gauss(copy, this->m_sigma, 0, 3);

		if (this->m_overlay == FAST_GAUSS_OVERLAY_MIN) {
			float *src = newBuf->getBuffer();
			float *dst = copy->getBuffer();
			for (int i = copy->getWidth() * copy->getHeight(); i != 0; i--, src += COM_NUMBER_OF_CHANNELS, dst += COM_NUMBER_OF_CHANNELS) {
				if (*src < *dst) {
					*dst = *src;
				}
			}
		}
		else if (this->m_overlay == FAST_GAUSS_OVERLAY_MAX) {
			float *src = newBuf->getBuffer();
			float *dst = copy->getBuffer();
			for (int i = copy->getWidth() * copy->getHeight(); i != 0; i--, src += COM_NUMBER_OF_CHANNELS, dst += COM_NUMBER_OF_CHANNELS) {
				if (*src > *dst) {
					*dst = *src;
				}
			}
		}

//		newBuf->

		this->m_iirgaus = copy;
	}
	unlockMutex();
	return this->m_iirgaus;
}

