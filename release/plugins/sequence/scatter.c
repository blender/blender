/**
 *
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
 */

#include "plugin.h"

/* ******************** GLOBAL VARIABLES ***************** */


char name[24] = "scatter";

/* structure for buttons, 
 *  butcode      name           default  min  max  0
 */

VarStruct varstr[] = {
	LABEL,      "Input: 1 strip", 0.0, 0.0, 0.0, "",
	NUM | INT,    "seed: ",       1.0,    0.0, 10.0, "Offset in random table",
	NUMSLI | FLO, "swing: ",      1.0,    0.0, 3.0, "The amplitude, width of the effect",
	TOG | INT,    "wrap",         0.0,    0.0, 1.0, "Cyclic wrap around the left/right edges",
	NUM | INT,    "type: ",       1.0,    0.0, 1.0, "Type 1 is random for each frame",
};

/* The cast struct is for input in the main doit function
   Varstr and Cast must have the same variables in the same order */ 

typedef struct Cast {
	int dummy;          /* because of the 'label' button */
	int seed;
	float swing;
	int wrap;
	int type;
} Cast;

/* cfra: the current frame */

float cfra;

void plugin_seq_doit(Cast *, float, float, int, int, ImBuf *, ImBuf *, ImBuf *, ImBuf *);


/* ******************** Fixed functions ***************** */

int plugin_seq_getversion(void) 
{
	return B_PLUGIN_VERSION;
}

void plugin_but_changed(int but) 
{
}

void plugin_init()
{
}

void plugin_getinfo(PluginInfo *info)
{
	info->name = name;
	info->nvars = sizeof(varstr) / sizeof(VarStruct);
	info->cfra = &cfra;

	info->varstr = varstr;

	info->init = plugin_init;
	info->seq_doit = (SeqDoit) plugin_seq_doit;
	info->callback = plugin_but_changed;
}


/* ************************************************************
    Scatter
	
************************************************************ */

static void rectcpy(ImBuf *dbuf, ImBuf *sbuf,	
                    int destx, int desty,
                    int srcx, int srcy, int width, int height)
{
	uint *drect, *srect;
	float *dfrect, *sfrect;
	int tmp;

	if (dbuf == 0) return;

	if (destx < 0) {
		srcx -= destx;
		width += destx;
		destx = 0;
	}
	if (srcx < 0) {
		destx -= srcx;
		width += destx;
		srcx = 0;
	}
	if (desty < 0) {
		srcy -= desty;
		height += desty;
		desty = 0;
	}
	if (srcy < 0) {
		desty -= srcy;
		height += desty;
		srcy = 0;
	}

	if (width > dbuf->x - destx) width = dbuf->x - destx;
	if (height > dbuf->y - desty) height = dbuf->y - desty;
	if (sbuf) {
		if (width > sbuf->x - srcx) width = sbuf->x - srcx;
		if (height > sbuf->y - srcy) height = sbuf->y - srcy;
		srect = sbuf->rect;
		sfrect = sbuf->rect_float;
	}

	if (width <= 0) return;
	if (height <= 0) return;

	drect = dbuf->rect;
	dfrect = dbuf->rect_float;

	tmp = (desty * dbuf->x + destx);

	if (dbuf->rect_float) dfrect += 4 * tmp;
	else drect += tmp;

	destx = dbuf->x;

	if (sbuf) {
		tmp = (srcy * sbuf->x + srcx);
		if (dbuf->rect_float) sfrect += 4 * tmp; 
		else srect += tmp;
		srcx = sbuf->x;
	}
	else {
		if (dbuf->rect_float) sfrect = dfrect;
		else srect = drect;
		srcx = destx;
	}

	for (; height > 0; height--) {
		if (dbuf->rect_float) {
			memcpy(dfrect, sfrect, 4 * width * sizeof(float));
			dfrect += destx;
			sfrect += srcx;
		}
		else {
			memcpy(drect, srect, width * sizeof(int));
			drect += destx;
			srect += srcx;
		}
	}
}

static void fill_out(ImBuf *out, float r, float g, float b, float a)
{
	int tot, x;
	float *rectf = out->rect_float;
	unsigned char *rect = (unsigned char *)out->rect;

	tot = out->x * out->y;
	if (out->rect_float) {
		for (x = 0; x < tot; x++) {
			rectf[0] = r;
			rectf[1] = g;
			rectf[2] = b;
			rectf[3] = a;
			rectf += 4;
		}
	}
	else {
		for (x = 0; x < tot; x++) {
			rect[0] = (int)(r * 255);
			rect[1] = (int)(g * 255);
			rect[2] = (int)(b * 255);
			rect[3] = (int)(a * 255);
			rect += 4;
		}
	}
}


void plugin_seq_doit(Cast *cast, float facf0, float facf1, int sx, int sy, ImBuf *ibuf1, ImBuf *ibuf2, ImBuf *out, ImBuf *use)
{
	float f1, f2, t1, t2, t3;
	int x, y, lr;
	
	/* fill imbuf 'out' with black */
	fill_out(out, 0, 0, 0, 0);


	switch (cast->type) {
		case 0:
			srand48(cast->seed);
			break;
		case 1:
			srand48(cast->seed + facf0 * 1000);
			break;
	}

	for (y = 0; y < sy; y++) {
		switch (cast->type) {
			case 0:
				if ((y & 1) == 0) {
					f1 = drand48() - 0.5;
					f2 = drand48() - 0.5;
					f1 = cast->swing * f1;
					f2 = cast->swing * f2;
					if (cast->wrap) f2 += 1.0;
					lr = drand48() > 0.5;
					t1 = facf0;
				}
				else t1 = facf1;
				
				t2 = 1.0 - t1;
				t3 = 3.0 * (f1 * t1 * t1 * t2 + f2 * t1 * t2 * t2);
				if (cast->wrap) t3 += t2 * t2 * t2;
				x = sx * t3;
				if (lr) x = -x;
				break;
			case 1:
				f1 = drand48() - 0.5;
				f1 = f1 * cast->swing;
				if ((y & 1) == 0) f1 *= facf0;
				else f1 *= facf1;
				x = f1 * sx;
				break;
		}
		
		rectcpy(out, ibuf1, 0, y, x, y, 32767, 1);
		if (cast->wrap) {
			rectcpy(out, ibuf1, 0, y, x + sx, y, 32767, 1);
			rectcpy(out, ibuf1, 0, y, x + sx + sx, y, 32767, 1);
			rectcpy(out, ibuf1, 0, y, x - sx, y, 32767, 1);
			rectcpy(out, ibuf1, 0, y, x - sx - sx, y, 32767, 1);
		}
	}
}

