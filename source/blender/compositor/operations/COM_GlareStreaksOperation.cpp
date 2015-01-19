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

#include "COM_GlareStreaksOperation.h"
#include "BLI_math.h"

void GlareStreaksOperation::generateGlare(float *data, MemoryBuffer *inputTile, NodeGlare *settings)
{
	int x, y, n;
	unsigned int nump = 0;
	float c1[4], c2[4], c3[4], c4[4];
	float a, ang = DEG2RADF(360.0f) / (float)settings->angle;

	int size = inputTile->getWidth() * inputTile->getHeight();
	int size4 = size * 4;

	bool breaked = false;

	MemoryBuffer *tsrc = inputTile->duplicate();
	MemoryBuffer *tdst = new MemoryBuffer(COM_DT_COLOR, inputTile->getRect());
	tdst->clear();
	memset(data, 0, size4 * sizeof(float));

	for (a = 0.f; a < DEG2RADF(360.0f) && (!breaked); a += ang) {
		const float an = a + settings->angle_ofs;
		const float vx = cos((double)an), vy = sin((double)an);
		for (n = 0; n < settings->iter && (!breaked); ++n) {
			const float p4 = pow(4.0, (double)n);
			const float vxp = vx * p4, vyp = vy * p4;
			const float wt = pow((double)settings->fade, (double)p4);
			const float cmo = 1.f - (float)pow((double)settings->colmod, (double)n + 1);  // colormodulation amount relative to current pass
			float *tdstcol = tdst->getBuffer();
			for (y = 0; y < tsrc->getHeight() && (!breaked); ++y) {
				for (x = 0; x < tsrc->getWidth(); ++x, tdstcol += 4) {
					// first pass no offset, always same for every pass, exact copy,
					// otherwise results in uneven brightness, only need once
					if (n == 0) tsrc->read(c1, x, y); else c1[0] = c1[1] = c1[2] = 0;
					tsrc->readBilinear(c2, x + vxp, y + vyp);
					tsrc->readBilinear(c3, x + vxp * 2.f, y + vyp * 2.f);
					tsrc->readBilinear(c4, x + vxp * 3.f, y + vyp * 3.f);
					// modulate color to look vaguely similar to a color spectrum
					c2[1] *= cmo;
					c2[2] *= cmo;

					c3[0] *= cmo;
					c3[1] *= cmo;

					c4[0] *= cmo;
					c4[2] *= cmo;

					tdstcol[0] = 0.5f * (tdstcol[0] + c1[0] + wt * (c2[0] + wt * (c3[0] + wt * c4[0])));
					tdstcol[1] = 0.5f * (tdstcol[1] + c1[1] + wt * (c2[1] + wt * (c3[1] + wt * c4[1])));
					tdstcol[2] = 0.5f * (tdstcol[2] + c1[2] + wt * (c2[2] + wt * (c3[2] + wt * c4[2])));
					tdstcol[3] = 1.0f;
				}
				if (isBreaked()) {
					breaked = true;
				}
			}
			memcpy(tsrc->getBuffer(), tdst->getBuffer(), sizeof(float) * size4);
		}

		float *sourcebuffer = tsrc->getBuffer();
		float factor = 1.f / (float)(6 - settings->iter);
		for (int i = 0; i < size4; i += 4) {
			madd_v3_v3fl(&data[i], &sourcebuffer[i], factor);
			data[i + 3] =  1.0f;
		}

		tdst->clear();
		memcpy(tsrc->getBuffer(), inputTile->getBuffer(), sizeof(float) * size4);
		nump++;
	}

	delete tsrc;
	delete tdst;
}
