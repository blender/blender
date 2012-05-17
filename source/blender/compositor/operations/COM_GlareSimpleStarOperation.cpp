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

#include "COM_GlareSimpleStarOperation.h"

void GlareSimpleStarOperation::generateGlare(float *data, MemoryBuffer *inputTile, NodeGlare *settings) {
	int i, x, y, ym, yp, xm, xp;
	float c[4] = {0,0,0,0}, tc[4] = {0,0,0,0};
	const float f1 = 1.f - settings->fade, f2 = (1.f - f1)*0.5f;


	MemoryBuffer* tbuf1 = inputTile->duplicate();
	MemoryBuffer* tbuf2 = inputTile->duplicate();

	for (i=0; i<settings->iter; i++) {
//		// (x || x-1, y-1) to (x || x+1, y+1)
//		// F
		for (y=0; y<this->getHeight(); y++) {
			ym = y - i;
			yp = y + i;
			for (x=0; x<this->getWidth(); x++) {
				xm = x - i;
				xp = x + i;
				tbuf1->read(c, x, y);
				c[0]*=f1; c[1]*=f1 ; c[2] *=f1;
				tbuf1->read(tc, (settings->angle ? xm : x), ym);
				c[0]+=tc[0]*f2;
				c[1]+=tc[1]*f2;
				c[2]+=tc[2]*f2;
				tbuf1->read(tc, (settings->angle ? xp : x), yp);
				c[0]+=tc[0]*f2;
				c[1]+=tc[1]*f2;
				c[2]+=tc[2]*f2;
				c[3] = 1.0f;
				tbuf1->writePixel(x, y, c);

				tbuf2->read(c, x, y);
				c[0]*=f1; c[1]*=f1 ; c[2] *=f1;
				tbuf2->read(tc, xm, (settings->angle ? yp : y));
				c[0]+=tc[0]*f2;
				c[1]+=tc[1]*f2;
				c[2]+=tc[2]*f2;
				tbuf2->read(tc, xp, (settings->angle ? ym : y));
				c[0]+=tc[0]*f2;
				c[1]+=tc[1]*f2;
				c[2]+=tc[2]*f2;
				c[3] = 1.0f;
				tbuf2->writePixel(x, y, c);

			}
		}
//		// B
		for (y=tbuf1->getHeight()-1; y>=0; y--) {
			ym = y - i;
			yp = y + i;
			for (x=tbuf1->getWidth()-1; x>=0; x--) {
				xm = x - i;
				xp = x + i;
				tbuf1->read(c, x, y);
				c[0]*=f1; c[1]*=f1 ; c[2] *=f1;
				tbuf1->read(tc, (settings->angle ? xm : x), ym);
				c[0]+=tc[0]*f2;
				c[1]+=tc[1]*f2;
				c[2]+=tc[2]*f2;
				tbuf1->read(tc, (settings->angle ? xp : x), yp);
				c[0]+=tc[0]*f2;
				c[1]+=tc[1]*f2;
				c[2]+=tc[2]*f2;
				c[3] = 1.0f;
				tbuf1->writePixel(x, y, c);

				tbuf2->read(c, x, y);
				c[0]*=f1; c[1]*=f1 ; c[2] *=f1;
				tbuf2->read(tc, xm, (settings->angle ? yp : y));
				c[0]+=tc[0]*f2;
				c[1]+=tc[1]*f2;
				c[2]+=tc[2]*f2;
				tbuf2->read(tc, xp, (settings->angle ? ym : y));
				c[0]+=tc[0]*f2;
				c[1]+=tc[1]*f2;
				c[2]+=tc[2]*f2;
				c[3] = 1.0f;
				tbuf2->writePixel(x, y, c);
			}
		}
	}

	for (i = 0 ; i < this->getWidth()*this->getHeight()*4 ; i++) {
		data[i] = tbuf1->getBuffer()[i] + tbuf2->getBuffer()[i];
	}

	delete tbuf1;
	delete tbuf2;
}
