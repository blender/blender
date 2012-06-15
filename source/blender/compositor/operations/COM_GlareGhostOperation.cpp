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

#include "COM_GlareGhostOperation.h"
#include "BLI_math.h"
#include "COM_FastGaussianBlurOperation.h"

static float smoothMask(float x, float y)
{
	float t;
	x = 2.0f * x - 1.0f;
	y = 2.0f * y - 1.0f;
	if ((t = 1.0f - sqrtf(x * x + y * y)) > 0.0f) {
		return t;
	}
	else {
		return 0.0f;
	}
}

void GlareGhostOperation::generateGlare(float *data, MemoryBuffer *inputTile, NodeGlare *settings)
{
	const int qt = 1 << settings->quality;
	const float s1 = 4.f / (float)qt, s2 = 2.f * s1;
	int x, y, n, p, np;
	fRGB c, tc, cm[64];
	float sc, isc, u, v, sm, s, t, ofs, scalef[64];
	const float cmo = 1.f - settings->colmod;

	MemoryBuffer *gbuf = inputTile->duplicate();
	MemoryBuffer *tbuf1 = inputTile->duplicate();

	bool breaked = false;

	FastGaussianBlurOperation::IIR_gauss(tbuf1, s1, 0, 3);
	if (!breaked) FastGaussianBlurOperation::IIR_gauss(tbuf1, s1, 1, 3);
	if (isBreaked()) breaked = true;
	if (!breaked) FastGaussianBlurOperation::IIR_gauss(tbuf1, s1, 2, 3);

	MemoryBuffer *tbuf2 = tbuf1->duplicate();

	if (isBreaked()) breaked = true;
	if (!breaked) FastGaussianBlurOperation::IIR_gauss(tbuf2, s2, 0, 3);
	if (isBreaked()) breaked = true;
	if (!breaked) FastGaussianBlurOperation::IIR_gauss(tbuf2, s2, 1, 3);
	if (isBreaked()) breaked = true;
	if (!breaked) FastGaussianBlurOperation::IIR_gauss(tbuf2, s2, 2, 3);

	if (settings->iter & 1) ofs = 0.5f; else ofs = 0.f;
	for (x = 0; x < (settings->iter * 4); x++) {
		y = x & 3;
		cm[x][0] = cm[x][1] = cm[x][2] = 1;
		if (y == 1) fRGB_rgbmult(cm[x], 1.f, cmo, cmo);
		if (y == 2) fRGB_rgbmult(cm[x], cmo, cmo, 1.f);
		if (y == 3) fRGB_rgbmult(cm[x], cmo, 1.f, cmo);
		scalef[x] = 2.1f * (1.f - (x + ofs) / (float)(settings->iter * 4));
		if (x & 1) scalef[x] = -0.99f / scalef[x];
	}

	sc = 2.13;
	isc = -0.97;
	for (y = 0; y < gbuf->getHeight() && (!breaked); y++) {
		v = (float)(y + 0.5f) / (float)gbuf->getHeight();
		for (x = 0; x < gbuf->getWidth(); x++) {
			u = (float)(x + 0.5f) / (float)gbuf->getWidth();
			s = (u - 0.5f) * sc + 0.5f, t = (v - 0.5f) * sc + 0.5f;
			tbuf1->read(c, s * gbuf->getWidth(), t * gbuf->getHeight());
			sm = smoothMask(s, t);
			mul_v3_fl(c, sm);
			s = (u - 0.5f) * isc + 0.5f, t = (v - 0.5f) * isc + 0.5f;
			tbuf2->read(tc, s * gbuf->getWidth() - 0.5f, t * gbuf->getHeight() - 0.5f);
			sm = smoothMask(s, t);
			madd_v3_v3fl(c, tc, sm);

			gbuf->writePixel(x, y, c);
		}
		if (isBreaked()) breaked = true;

	}

	memset(tbuf1->getBuffer(), 0, tbuf1->getWidth() * tbuf1->getHeight() * COM_NUMBER_OF_CHANNELS * sizeof(float));
	for (n = 1; n < settings->iter && (!breaked); n++) {
		for (y = 0; y < gbuf->getHeight() && (!breaked); y++) {
			v = (float)(y + 0.5f) / (float)gbuf->getHeight();
			for (x = 0; x < gbuf->getWidth(); x++) {
				u = (float)(x + 0.5f) / (float)gbuf->getWidth();
				tc[0] = tc[1] = tc[2] = 0.f;
				for (p = 0; p < 4; p++) {
					np = (n << 2) + p;
					s = (u - 0.5f) * scalef[np] + 0.5f;
					t = (v - 0.5f) * scalef[np] + 0.5f;
					gbuf->read(c, s * gbuf->getWidth() - 0.5f, t * gbuf->getHeight() - 0.5f);
					mul_v3_v3(c, cm[np]);
					sm = smoothMask(s, t) * 0.25f;
					madd_v3_v3fl(tc, c, sm);
				}
				tbuf1->addPixel(x, y, tc);
			}
			if (isBreaked()) breaked = true;
		}
		memcpy(gbuf->getBuffer(), tbuf1->getBuffer(), tbuf1->getWidth() * tbuf1->getHeight() * COM_NUMBER_OF_CHANNELS * sizeof(float));
	}
	memcpy(data, gbuf->getBuffer(), gbuf->getWidth() * gbuf->getHeight() * COM_NUMBER_OF_CHANNELS * sizeof(float));

	delete gbuf;
	delete tbuf1;
	delete tbuf2;
}
