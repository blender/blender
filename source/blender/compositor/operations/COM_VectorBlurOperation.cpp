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

#include <string.h>
#include "MEM_guardedalloc.h"
#include "BLI_math.h"
extern "C" {
#include "BLI_jitter_2d.h"
}
#include "COM_VectorBlurOperation.h"


/* Defined */
#define PASS_VECTOR_MAX 10000.0f

/* Forward declarations */
struct ZSpan;
struct DrawBufPixel;
void zbuf_accumulate_vecblur(
        NodeBlurData *nbd, int xsize, int ysize, float *newrect,
        const float *imgrect, float *vecbufrect, const float *zbufrect);
void zbuf_alloc_span(ZSpan *zspan, int rectx, int recty, float clipcrop);
void zbuf_free_span(ZSpan *zspan);
void antialias_tagbuf(int xsize, int ysize, char *rectmove);


/* VectorBlurOperation */
VectorBlurOperation::VectorBlurOperation() : NodeOperation()
{
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE); // ZBUF
	this->addInputSocket(COM_DT_COLOR); //SPEED
	this->addOutputSocket(COM_DT_COLOR);
	this->m_settings = NULL;
	this->m_cachedInstance = NULL;
	this->m_inputImageProgram = NULL;
	this->m_inputSpeedProgram = NULL;
	this->m_inputZProgram = NULL;
	setComplex(true);
}
void VectorBlurOperation::initExecution()
{
	initMutex();
	this->m_inputImageProgram = getInputSocketReader(0);
	this->m_inputZProgram = getInputSocketReader(1);
	this->m_inputSpeedProgram = getInputSocketReader(2);
	this->m_cachedInstance = NULL;
	QualityStepHelper::initExecution(COM_QH_INCREASE);

}

void VectorBlurOperation::executePixel(float output[4], int x, int y, void *data)
{
	float *buffer = (float *)data;
	int index = (y * this->getWidth() + x) * COM_NUM_CHANNELS_COLOR;
	copy_v4_v4(output, &buffer[index]);
}

void VectorBlurOperation::deinitExecution()
{
	deinitMutex();
	this->m_inputImageProgram = NULL;
	this->m_inputSpeedProgram = NULL;
	this->m_inputZProgram = NULL;
	if (this->m_cachedInstance) {
		MEM_freeN(this->m_cachedInstance);
		this->m_cachedInstance = NULL;
	}
}
void *VectorBlurOperation::initializeTileData(rcti *rect)
{
	if (this->m_cachedInstance) {
		return this->m_cachedInstance;
	}

	lockMutex();
	if (this->m_cachedInstance == NULL) {
		MemoryBuffer *tile = (MemoryBuffer *)this->m_inputImageProgram->initializeTileData(rect);
		MemoryBuffer *speed = (MemoryBuffer *)this->m_inputSpeedProgram->initializeTileData(rect);
		MemoryBuffer *z = (MemoryBuffer *)this->m_inputZProgram->initializeTileData(rect);
		float *data = (float *)MEM_dupallocN(tile->getBuffer());
		this->generateVectorBlur(data, tile, speed, z);
		this->m_cachedInstance = data;
	}
	unlockMutex();
	return this->m_cachedInstance;
}

bool VectorBlurOperation::determineDependingAreaOfInterest(rcti * /*input*/, ReadBufferOperation *readOperation, rcti *output)
{
	if (this->m_cachedInstance == NULL) {
		rcti newInput;
		newInput.xmax = this->getWidth();
		newInput.xmin = 0;
		newInput.ymax = this->getHeight();
		newInput.ymin = 0;
		return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
	}
	else {
		return false;
	}
}

void VectorBlurOperation::generateVectorBlur(float *data, MemoryBuffer *inputImage, MemoryBuffer *inputSpeed, MemoryBuffer *inputZ)
{
	NodeBlurData blurdata;
	blurdata.samples = this->m_settings->samples / QualityStepHelper::getStep();
	blurdata.maxspeed = this->m_settings->maxspeed;
	blurdata.minspeed = this->m_settings->minspeed;
	blurdata.curved = this->m_settings->curved;
	blurdata.fac = this->m_settings->fac;
	zbuf_accumulate_vecblur(&blurdata, this->getWidth(), this->getHeight(), data, inputImage->getBuffer(), inputSpeed->getBuffer(), inputZ->getBuffer());
	return;
}

/* ****************** Spans ******************************* */
/* span fill in method, is also used to localize data for zbuffering */
typedef struct ZSpan {
	/* range for clipping */
	int rectx, recty;

	/* actual filled in range */
	int miny1, maxy1, miny2, maxy2;
	/* vertex pointers detect min/max range in */
	const float *minp1, *maxp1, *minp2, *maxp2;
	float *span1, *span2;

	/* transform from hoco to zbuf co */
	float zmulx, zmuly, zofsx, zofsy;

	int *rectz;
	DrawBufPixel *rectdraw;
	float clipcrop;

} ZSpan;

/* each zbuffer has coordinates transformed to local rect coordinates, so we can simply clip */
void zbuf_alloc_span(ZSpan *zspan, int rectx, int recty, float clipcrop)
{
	memset(zspan, 0, sizeof(ZSpan));

	zspan->rectx = rectx;
	zspan->recty = recty;

	zspan->span1 = (float *)MEM_mallocN(recty * sizeof(float), "zspan");
	zspan->span2 = (float *)MEM_mallocN(recty * sizeof(float), "zspan");

	zspan->clipcrop = clipcrop;
}

void zbuf_free_span(ZSpan *zspan)
{
	if (zspan) {
		if (zspan->span1) MEM_freeN(zspan->span1);
		if (zspan->span2) MEM_freeN(zspan->span2);
		zspan->span1 = zspan->span2 = NULL;
	}
}

/* reset range for clipping */
static void zbuf_init_span(ZSpan *zspan)
{
	zspan->miny1 = zspan->miny2 = zspan->recty + 1;
	zspan->maxy1 = zspan->maxy2 = -1;
	zspan->minp1 = zspan->maxp1 = zspan->minp2 = zspan->maxp2 = NULL;
}

static void zbuf_add_to_span(ZSpan *zspan, const float v1[2], const float v2[2])
{
	const float *minv, *maxv;
	float *span;
	float xx1, dx0, xs0;
	int y, my0, my2;

	if (v1[1] < v2[1]) {
		minv = v1; maxv = v2;
	}
	else {
		minv = v2; maxv = v1;
	}

	my0 = ceil(minv[1]);
	my2 = floor(maxv[1]);

	if (my2 < 0 || my0 >= zspan->recty) return;

	/* clip top */
	if (my2 >= zspan->recty) my2 = zspan->recty - 1;
	/* clip bottom */
	if (my0 < 0) my0 = 0;

	if (my0 > my2) return;
	/* if (my0>my2) should still fill in, that way we get spans that skip nicely */

	xx1 = maxv[1] - minv[1];
	if (xx1 > FLT_EPSILON) {
		dx0 = (minv[0] - maxv[0]) / xx1;
		xs0 = dx0 * (minv[1] - my2) + minv[0];
	}
	else {
		dx0 = 0.0f;
		xs0 = min_ff(minv[0], maxv[0]);
	}

	/* empty span */
	if (zspan->maxp1 == NULL) {
		span = zspan->span1;
	}
	else {  /* does it complete left span? */
		if (maxv == zspan->minp1 || minv == zspan->maxp1) {
			span = zspan->span1;
		}
		else {
			span = zspan->span2;
		}
	}

	if (span == zspan->span1) {
//		printf("left span my0 %d my2 %d\n", my0, my2);
		if (zspan->minp1 == NULL || zspan->minp1[1] > minv[1]) {
			zspan->minp1 = minv;
		}
		if (zspan->maxp1 == NULL || zspan->maxp1[1] < maxv[1]) {
			zspan->maxp1 = maxv;
		}
		if (my0 < zspan->miny1) zspan->miny1 = my0;
		if (my2 > zspan->maxy1) zspan->maxy1 = my2;
	}
	else {
//		printf("right span my0 %d my2 %d\n", my0, my2);
		if (zspan->minp2 == NULL || zspan->minp2[1] > minv[1]) {
			zspan->minp2 = minv;
		}
		if (zspan->maxp2 == NULL || zspan->maxp2[1] < maxv[1]) {
			zspan->maxp2 = maxv;
		}
		if (my0 < zspan->miny2) zspan->miny2 = my0;
		if (my2 > zspan->maxy2) zspan->maxy2 = my2;
	}

	for (y = my2; y >= my0; y--, xs0 += dx0) {
		/* xs0 is the xcoord! */
		span[y] = xs0;
	}
}

/* ******************** VECBLUR ACCUM BUF ************************* */

typedef struct DrawBufPixel {
	const float *colpoin;
	float alpha;
} DrawBufPixel;


static void zbuf_fill_in_rgba(ZSpan *zspan, DrawBufPixel *col, float *v1, float *v2, float *v3, float *v4)
{
	DrawBufPixel *rectpofs, *rp;
	double zxd, zyd, zy0, zverg;
	float x0, y0, z0;
	float x1, y1, z1, x2, y2, z2, xx1;
	const float *span1, *span2;
	float *rectzofs, *rz;
	int x, y;
	int sn1, sn2, rectx, my0, my2;

	/* init */
	zbuf_init_span(zspan);

	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	zbuf_add_to_span(zspan, v3, v4);
	zbuf_add_to_span(zspan, v4, v1);

	/* clipped */
	if (zspan->minp2 == NULL || zspan->maxp2 == NULL) return;

	my0 = max_ii(zspan->miny1, zspan->miny2);
	my2 = min_ii(zspan->maxy1, zspan->maxy2);

	//	printf("my %d %d\n", my0, my2);
	if (my2 < my0) return;

	/* ZBUF DX DY, in floats still */
	x1 = v1[0] - v2[0];
	x2 = v2[0] - v3[0];
	y1 = v1[1] - v2[1];
	y2 = v2[1] - v3[1];
	z1 = v1[2] - v2[2];
	z2 = v2[2] - v3[2];
	x0 = y1 * z2 - z1 * y2;
	y0 = z1 * x2 - x1 * z2;
	z0 = x1 * y2 - y1 * x2;

	if (z0 == 0.0f) return;

	xx1 = (x0 * v1[0] + y0 * v1[1]) / z0 + v1[2];

	zxd = -(double)x0 / (double)z0;
	zyd = -(double)y0 / (double)z0;
	zy0 = ((double)my2) * zyd + (double)xx1;

	/* start-offset in rect */
	rectx = zspan->rectx;
	rectzofs = (float *)(zspan->rectz + rectx * my2);
	rectpofs = ((DrawBufPixel *)zspan->rectdraw) + rectx * my2;

	/* correct span */
	sn1 = (my0 + my2) / 2;
	if (zspan->span1[sn1] < zspan->span2[sn1]) {
		span1 = zspan->span1 + my2;
		span2 = zspan->span2 + my2;
	}
	else {
		span1 = zspan->span2 + my2;
		span2 = zspan->span1 + my2;
	}

	for (y = my2; y >= my0; y--, span1--, span2--) {

		sn1 = floor(*span1);
		sn2 = floor(*span2);
		sn1++;

		if (sn2 >= rectx) sn2 = rectx - 1;
		if (sn1 < 0) sn1 = 0;

		if (sn2 >= sn1) {
			zverg = (double)sn1 * zxd + zy0;
			rz = rectzofs + sn1;
			rp = rectpofs + sn1;
			x = sn2 - sn1;

			while (x >= 0) {
				if (zverg < (double)*rz) {
					*rz = zverg;
					*rp = *col;
				}
				zverg += zxd;
				rz++;
				rp++;
				x--;
			}
		}

		zy0 -= zyd;
		rectzofs -= rectx;
		rectpofs -= rectx;
	}
}

/* char value==255 is filled in, rest should be zero */
/* returns alpha values, but sets alpha to 1 for zero alpha pixels that have an alpha value as neighbor */
void antialias_tagbuf(int xsize, int ysize, char *rectmove)
{
	char *row1, *row2, *row3;
	char prev, next;
	int a, x, y, step;

	/* 1: tag pixels to be candidate for AA */
	for (y = 2; y < ysize; y++) {
		/* setup rows */
		row1 = rectmove + (y - 2) * xsize;
		row2 = row1 + xsize;
		row3 = row2 + xsize;
		for (x = 2; x < xsize; x++, row1++, row2++, row3++) {
			if (row2[1]) {
				if (row2[0] == 0 || row2[2] == 0 || row1[1] == 0 || row3[1] == 0)
					row2[1] = 128;
			}
		}
	}

	/* 2: evaluate horizontal scanlines and calculate alphas */
	row1 = rectmove;
	for (y = 0; y < ysize; y++) {
		row1++;
		for (x = 1; x < xsize; x++, row1++) {
			if (row1[0] == 128 && row1[1] == 128) {
				/* find previous color and next color and amount of steps to blend */
				prev = row1[-1];
				step = 1;
				while (x + step < xsize && row1[step] == 128)
					step++;

				if (x + step != xsize) {
					/* now we can blend values */
					next = row1[step];

					/* note, prev value can be next value, but we do this loop to clear 128 then */
					for (a = 0; a < step; a++) {
						int fac, mfac;

						fac = ((a + 1) << 8) / (step + 1);
						mfac = 255 - fac;

						row1[a] = (prev * mfac + next * fac) >> 8;
					}
				}
			}
		}
	}

	/* 3: evaluate vertical scanlines and calculate alphas */
	/*    use for reading a copy of the original tagged buffer */
	for (x = 0; x < xsize; x++) {
		row1 = rectmove + x + xsize;

		for (y = 1; y < ysize; y++, row1 += xsize) {
			if (row1[0] == 128 && row1[xsize] == 128) {
				/* find previous color and next color and amount of steps to blend */
				prev = row1[-xsize];
				step = 1;
				while (y + step < ysize && row1[step * xsize] == 128)
					step++;

				if (y + step != ysize) {
					/* now we can blend values */
					next = row1[step * xsize];
					/* note, prev value can be next value, but we do this loop to clear 128 then */
					for (a = 0; a < step; a++) {
						int fac, mfac;

						fac = ((a + 1) << 8) / (step + 1);
						mfac = 255 - fac;

						row1[a * xsize] = (prev * mfac + next * fac) >> 8;
					}
				}
			}
		}
	}

	/* last: pixels with 0 we fill in zbuffer, with 1 we skip for mask */
	for (y = 2; y < ysize; y++) {
		/* setup rows */
		row1 = rectmove + (y - 2) * xsize;
		row2 = row1 + xsize;
		row3 = row2 + xsize;
		for (x = 2; x < xsize; x++, row1++, row2++, row3++) {
			if (row2[1] == 0) {
				if (row2[0] > 1 || row2[2] > 1 || row1[1] > 1 || row3[1] > 1)
					row2[1] = 1;
			}
		}
	}
}

/* in: two vectors, first vector points from origin back in time, 2nd vector points to future */
/* we make this into 3 points, center point is (0, 0) */
/* and offset the center point just enough to make curve go through midpoint */

static void quad_bezier_2d(float *result, float *v1, float *v2, float *ipodata)
{
	float p1[2], p2[2], p3[2];

	p3[0] = -v2[0];
	p3[1] = -v2[1];

	p1[0] = v1[0];
	p1[1] = v1[1];

	/* official formula 2*p2 - 0.5*p1 - 0.5*p3 */
	p2[0] = -0.5f * p1[0] - 0.5f * p3[0];
	p2[1] = -0.5f * p1[1] - 0.5f * p3[1];

	result[0] = ipodata[0] * p1[0] + ipodata[1] * p2[0] + ipodata[2] * p3[0];
	result[1] = ipodata[0] * p1[1] + ipodata[1] * p2[1] + ipodata[2] * p3[1];
}

static void set_quad_bezier_ipo(float fac, float *data)
{
	float mfac = (1.0f - fac);

	data[0] = mfac * mfac;
	data[1] = 2.0f * mfac * fac;
	data[2] = fac * fac;
}

void zbuf_accumulate_vecblur(
	NodeBlurData *nbd, int xsize, int ysize, float *newrect,
	const float *imgrect, float *vecbufrect, const float *zbufrect)
{
	ZSpan zspan;
	DrawBufPixel *rectdraw, *dr;
	static float jit[256][2];
	float v1[3], v2[3], v3[3], v4[3], fx, fy;
	const float *dimg, *dz, *ro;
	float *rectvz, *dvz, *dvec1, *dvec2, *dz1, *dz2, *rectz;
	float *minvecbufrect = NULL, *rectweight, *rw, *rectmax, *rm;
	float maxspeedsq = (float)nbd->maxspeed * nbd->maxspeed;
	int y, x, step, maxspeed = nbd->maxspeed, samples = nbd->samples;
	int tsktsk = 0;
	static int firsttime = 1;
	char *rectmove, *dm;

	zbuf_alloc_span(&zspan, xsize, ysize, 1.0f);
	zspan.zmulx =  ((float)xsize) / 2.0f;
	zspan.zmuly =  ((float)ysize) / 2.0f;
	zspan.zofsx = 0.0f;
	zspan.zofsy = 0.0f;

	/* the buffers */
	rectz = (float *)MEM_mapallocN(sizeof(float) * xsize * ysize, "zbuf accum");
	zspan.rectz = (int *)rectz;

	rectmove = (char *)MEM_mapallocN(xsize * ysize, "rectmove");
	rectdraw = (DrawBufPixel *)MEM_mapallocN(sizeof(DrawBufPixel) * xsize * ysize, "rect draw");
	zspan.rectdraw = rectdraw;

	rectweight = (float *)MEM_mapallocN(sizeof(float) * xsize * ysize, "rect weight");
	rectmax = (float *)MEM_mapallocN(sizeof(float) * xsize * ysize, "rect max");

	/* debug... check if PASS_VECTOR_MAX still is in buffers */
	dvec1 = vecbufrect;
	for (x = 4 * xsize * ysize; x > 0; x--, dvec1++) {
		if (dvec1[0] == PASS_VECTOR_MAX) {
			dvec1[0] = 0.0f;
			tsktsk = 1;
		}
	}
	if (tsktsk) printf("Found uninitialized speed in vector buffer... fixed.\n");

	/* min speed? then copy speedbuffer to recalculate speed vectors */
	if (nbd->minspeed) {
		float minspeed = (float)nbd->minspeed;
		float minspeedsq = minspeed * minspeed;

		minvecbufrect = (float *)MEM_mapallocN(4 * sizeof(float) * xsize * ysize, "minspeed buf");

		dvec1 = vecbufrect;
		dvec2 = minvecbufrect;
		for (x = 2 * xsize * ysize; x > 0; x--, dvec1 += 2, dvec2 += 2) {
			if (dvec1[0] == 0.0f && dvec1[1] == 0.0f) {
				dvec2[0] = dvec1[0];
				dvec2[1] = dvec1[1];
			}
			else {
				float speedsq = dvec1[0] * dvec1[0] + dvec1[1] * dvec1[1];
				if (speedsq <= minspeedsq) {
					dvec2[0] = 0.0f;
					dvec2[1] = 0.0f;
				}
				else {
					speedsq = 1.0f - minspeed / sqrtf(speedsq);
					dvec2[0] = speedsq * dvec1[0];
					dvec2[1] = speedsq * dvec1[1];
				}
			}
		}
		SWAP(float *, minvecbufrect, vecbufrect);
	}

	/* make vertex buffer with averaged speed and zvalues */
	rectvz = (float *)MEM_mapallocN(4 * sizeof(float) * (xsize + 1) * (ysize + 1), "vertices");
	dvz = rectvz;
	for (y = 0; y <= ysize; y++) {

		if (y == 0)
			dvec1 = vecbufrect + 4 * y * xsize;
		else
			dvec1 = vecbufrect + 4 * (y - 1) * xsize;

		if (y == ysize)
			dvec2 = vecbufrect + 4 * (y - 1) * xsize;
		else
			dvec2 = vecbufrect + 4 * y * xsize;

		for (x = 0; x <= xsize; x++) {

			/* two vectors, so a step loop */
			for (step = 0; step < 2; step++, dvec1 += 2, dvec2 += 2, dvz += 2) {
				/* average on minimal speed */
				int div = 0;

				if (x != 0) {
					if (dvec1[-4] != 0.0f || dvec1[-3] != 0.0f) {
						dvz[0] = dvec1[-4];
						dvz[1] = dvec1[-3];
						div++;
					}
					if (dvec2[-4] != 0.0f || dvec2[-3] != 0.0f) {
						if (div == 0) {
							dvz[0] = dvec2[-4];
							dvz[1] = dvec2[-3];
							div++;
						}
						else if ( (ABS(dvec2[-4]) + ABS(dvec2[-3])) < (ABS(dvz[0]) + ABS(dvz[1])) ) {
							dvz[0] = dvec2[-4];
							dvz[1] = dvec2[-3];
						}
					}
				}

				if (x != xsize) {
					if (dvec1[0] != 0.0f || dvec1[1] != 0.0f) {
						if (div == 0) {
							dvz[0] = dvec1[0];
							dvz[1] = dvec1[1];
							div++;
						}
						else if ( (ABS(dvec1[0]) + ABS(dvec1[1])) < (ABS(dvz[0]) + ABS(dvz[1])) ) {
							dvz[0] = dvec1[0];
							dvz[1] = dvec1[1];
						}
					}
					if (dvec2[0] != 0.0f || dvec2[1] != 0.0f) {
						if (div == 0) {
							dvz[0] = dvec2[0];
							dvz[1] = dvec2[1];
						}
						else if ( (ABS(dvec2[0]) + ABS(dvec2[1])) < (ABS(dvz[0]) + ABS(dvz[1])) ) {
							dvz[0] = dvec2[0];
							dvz[1] = dvec2[1];
						}
					}
				}
				if (maxspeed) {
					float speedsq = dvz[0] * dvz[0] + dvz[1] * dvz[1];
					if (speedsq > maxspeedsq) {
						speedsq = (float)maxspeed / sqrtf(speedsq);
						dvz[0] *= speedsq;
						dvz[1] *= speedsq;
					}
				}
			}
		}
	}

	/* set border speeds to keep border speeds on border */
	dz1 = rectvz;
	dz2 = rectvz + 4 * (ysize) * (xsize + 1);
	for (x = 0; x <= xsize; x++, dz1 += 4, dz2 += 4) {
		dz1[1] = 0.0f;
		dz2[1] = 0.0f;
		dz1[3] = 0.0f;
		dz2[3] = 0.0f;
	}
	dz1 = rectvz;
	dz2 = rectvz + 4 * (xsize);
	for (y = 0; y <= ysize; y++, dz1 += 4 * (xsize + 1), dz2 += 4 * (xsize + 1)) {
		dz1[0] = 0.0f;
		dz2[0] = 0.0f;
		dz1[2] = 0.0f;
		dz2[2] = 0.0f;
	}

	/* tag moving pixels, only these faces we draw */
	dm = rectmove;
	dvec1 = vecbufrect;
	for (x = xsize * ysize; x > 0; x--, dm++, dvec1 += 4) {
		if ((dvec1[0] != 0.0f || dvec1[1] != 0.0f || dvec1[2] != 0.0f || dvec1[3] != 0.0f))
			*dm = 255;
	}

	antialias_tagbuf(xsize, ysize, rectmove);

	/* has to become static, the init-jit calls a random-seed, screwing up texture noise node */
	if (firsttime) {
		firsttime = 0;
		BLI_jitter_init(jit, 256);
	}

	memset(newrect, 0, sizeof(float) * xsize * ysize * 4);

	/* accumulate */
	samples /= 2;
	for (step = 1; step <= samples; step++) {
		float speedfac = 0.5f * nbd->fac * (float)step / (float)(samples + 1);
		int side;

		for (side = 0; side < 2; side++) {
			float blendfac, ipodata[4];

			/* clear zbuf, if we draw future we fill in not moving pixels */
			if (0)
				for (x = xsize * ysize - 1; x >= 0; x--) rectz[x] = 10e16;
			else
				for (x = xsize * ysize - 1; x >= 0; x--) {
					if (rectmove[x] == 0)
						rectz[x] = zbufrect[x];
					else
						rectz[x] = 10e16;
				}

			/* clear drawing buffer */
			for (x = xsize * ysize - 1; x >= 0; x--) rectdraw[x].colpoin = NULL;

			dimg = imgrect;
			dm = rectmove;
			dz = zbufrect;
			dz1 = rectvz;
			dz2 = rectvz + 4 * (xsize + 1);

			if (side) {
				if (nbd->curved == 0) {
					dz1 += 2;
					dz2 += 2;
				}
				speedfac = -speedfac;
			}

			set_quad_bezier_ipo(0.5f + 0.5f * speedfac, ipodata);

			for (fy = -0.5f + jit[step & 255][0], y = 0; y < ysize; y++, fy += 1.0f) {
				for (fx = -0.5f + jit[step & 255][1], x = 0; x < xsize; x++, fx += 1.0f, dimg += 4, dz1 += 4, dz2 += 4, dm++, dz++) {
					if (*dm > 1) {
						float jfx = fx + 0.5f;
						float jfy = fy + 0.5f;
						DrawBufPixel col;

						/* make vertices */
						if (nbd->curved) {  /* curved */
							quad_bezier_2d(v1, dz1, dz1 + 2, ipodata);
							v1[0] += jfx; v1[1] += jfy; v1[2] = *dz;

							quad_bezier_2d(v2, dz1 + 4, dz1 + 4 + 2, ipodata);
							v2[0] += jfx + 1.0f; v2[1] += jfy; v2[2] = *dz;

							quad_bezier_2d(v3, dz2 + 4, dz2 + 4 + 2, ipodata);
							v3[0] += jfx + 1.0f; v3[1] += jfy + 1.0f; v3[2] = *dz;

							quad_bezier_2d(v4, dz2, dz2 + 2, ipodata);
							v4[0] += jfx; v4[1] += jfy + 1.0f; v4[2] = *dz;
						}
						else {
							ARRAY_SET_ITEMS(v1, speedfac * dz1[0] + jfx,        speedfac * dz1[1] + jfy,        *dz);
							ARRAY_SET_ITEMS(v2, speedfac * dz1[4] + jfx + 1.0f, speedfac * dz1[5] + jfy,        *dz);
							ARRAY_SET_ITEMS(v3, speedfac * dz2[4] + jfx + 1.0f, speedfac * dz2[5] + jfy + 1.0f, *dz);
							ARRAY_SET_ITEMS(v4, speedfac * dz2[0] + jfx,        speedfac * dz2[1] + jfy + 1.0f, *dz);
						}
						if (*dm == 255) col.alpha = 1.0f;
						else if (*dm < 2) col.alpha = 0.0f;
						else col.alpha = ((float)*dm) / 255.0f;
						col.colpoin = dimg;

						zbuf_fill_in_rgba(&zspan, &col, v1, v2, v3, v4);
					}
				}
				dz1 += 4;
				dz2 += 4;
			}

			/* blend with a falloff. this fixes the ugly effect you get with
			 * a fast moving object. then it looks like a solid object overlaid
			 * over a very transparent moving version of itself. in reality, the
			 * whole object should become transparent if it is moving fast, be
			 * we don't know what is behind it so we don't do that. this hack
			 * overestimates the contribution of foreground pixels but looks a
			 * bit better without a sudden cutoff. */
			blendfac = ((samples - step) / (float)samples);
			/* smoothstep to make it look a bit nicer as well */
			blendfac = 3.0f * pow(blendfac, 2.0f) - 2.0f * pow(blendfac, 3.0f);

			/* accum */
			rw = rectweight;
			rm = rectmax;
			for (dr = rectdraw, dz2 = newrect, x = xsize * ysize - 1; x >= 0; x--, dr++, dz2 += 4, rw++, rm++) {
				if (dr->colpoin) {
					float bfac = dr->alpha * blendfac;

					dz2[0] += bfac * dr->colpoin[0];
					dz2[1] += bfac * dr->colpoin[1];
					dz2[2] += bfac * dr->colpoin[2];
					dz2[3] += bfac * dr->colpoin[3];

					*rw += bfac;
					*rm = MAX2(*rm, bfac);
				}
			}
		}
	}

	/* blend between original images and accumulated image */
	rw = rectweight;
	rm = rectmax;
	ro = imgrect;
	dm = rectmove;
	for (dz2 = newrect, x = xsize * ysize - 1; x >= 0; x--, dz2 += 4, ro += 4, rw++, rm++, dm++) {
		float mfac = *rm;
		float fac = (*rw == 0.0f) ? 0.0f : mfac / (*rw);
		float nfac = 1.0f - mfac;

		dz2[0] = fac * dz2[0] + nfac * ro[0];
		dz2[1] = fac * dz2[1] + nfac * ro[1];
		dz2[2] = fac * dz2[2] + nfac * ro[2];
		dz2[3] = fac * dz2[3] + nfac * ro[3];
	}

	MEM_freeN(rectz);
	MEM_freeN(rectmove);
	MEM_freeN(rectdraw);
	MEM_freeN(rectvz);
	MEM_freeN(rectweight);
	MEM_freeN(rectmax);
	if (minvecbufrect) MEM_freeN(vecbufrect);  /* rects were swapped! */
	zbuf_free_span(&zspan);
}
