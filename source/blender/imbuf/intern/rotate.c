/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * rotate.c
 *
 */

/** \file blender/imbuf/intern/rotate.c
 *  \ingroup imbuf
 */

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "imbuf.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

void IMB_flipy(struct ImBuf *ibuf)
{
	int x, y;

	if (ibuf == NULL) return;

	if (ibuf->rect) {
		unsigned int *top, *bottom, *line;

		x = ibuf->x;
		y = ibuf->y;

		top = ibuf->rect;
		bottom = top + ((y - 1) * x);
		line = MEM_mallocN(x * sizeof(int), "linebuf");
	
		y >>= 1;

		for (; y > 0; y--) {
			memcpy(line, top, x * sizeof(int));
			memcpy(top, bottom, x * sizeof(int));
			memcpy(bottom, line, x * sizeof(int));
			bottom -= x;
			top += x;
		}

		MEM_freeN(line);
	}

	if (ibuf->rect_float) {
		float *topf = NULL, *bottomf = NULL, *linef = NULL;

		x = ibuf->x;
		y = ibuf->y;

		topf = ibuf->rect_float;
		bottomf = topf + 4 * ((y - 1) * x);
		linef = MEM_mallocN(4 * x * sizeof(float), "linebuff");

		y >>= 1;

		for (; y > 0; y--) {
			memcpy(linef, topf, 4 * x * sizeof(float));
			memcpy(topf, bottomf, 4 * x * sizeof(float));
			memcpy(bottomf, linef, 4 * x * sizeof(float));
			bottomf -= 4 * x;
			topf += 4 * x;
		}

		MEM_freeN(linef);
	}
}

void IMB_flipx(struct ImBuf *ibuf)
{
	int x, y, xr, xl, yi;
	float px_f[4];
	
	if (ibuf == NULL) return;

	x = ibuf->x;
	y = ibuf->y;

	if (ibuf->rect) {
		for (yi = y - 1; yi >= 0; yi--) {
			for (xr = x - 1, xl = 0; xr >= xl; xr--, xl++) {
				SWAP(unsigned int, ibuf->rect[(x * yi) + xr], ibuf->rect[(x * yi) + xl]);
			}
		}
	}
	
	if (ibuf->rect_float) {
		for (yi = y - 1; yi >= 0; yi--) {
			for (xr = x - 1, xl = 0; xr >= xl; xr--, xl++) {
				memcpy(&px_f, &ibuf->rect_float[((x * yi) + xr) * 4], 4 * sizeof(float));
				memcpy(&ibuf->rect_float[((x * yi) + xr) * 4], &ibuf->rect_float[((x * yi) + xl) * 4], 4 * sizeof(float));
				memcpy(&ibuf->rect_float[((x * yi) + xl) * 4], &px_f, 4 * sizeof(float));
			}
		}
	}
}
