

/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_resources.h"

#include "datatoc.h"

typedef struct {
	unsigned char *data;
	int w, h;
} Icon;

static Icon *icon_from_data(unsigned char *rect, int w, int h, int rowstride)
{
	Icon *icon= MEM_mallocN(sizeof(*icon), "internicon");
	int y;
	icon->data= MEM_mallocN(w*h*4, "icon->data");
	icon->w= w;
	icon->h= h;
	for (y=0; y<h; y++)
		memcpy(&icon->data[y*w*4], &rect[y*rowstride], w*4);
	return icon;
}
static void icon_draw(Icon *icon)
{
	glDrawPixels(icon->w, icon->h, GL_RGBA, GL_UNSIGNED_BYTE, icon->data);
}
static unsigned char colclamp(int val)
{
	return (val<0)?(0):((val>255)?255:val);
}
static void icon_draw_blended(Icon *icon, unsigned char blendcol[3])
{
	unsigned char temprect[20*21*4];	/* XXX, not so safe */
	unsigned char *bgcol= icon->data;
	int blendfac[3];
	int x, y;

	blendfac[0]= bgcol[0]? (blendcol[0]<<8)/bgcol[0] : 0;
	blendfac[1]= bgcol[1]? (blendcol[1]<<8)/bgcol[1] : 0;
	blendfac[2]= bgcol[2]? (blendcol[2]<<8)/bgcol[2] : 0;

	for (y=0; y<icon->h; y++) {
		unsigned char *row= &icon->data[y*(icon->w*4)];
		unsigned char *orow= &temprect[y*(icon->w*4)];

		for (x=0; x<icon->w; x++) {
			unsigned char *pxl= &row[x*4];
			unsigned char *opxl= &orow[x*4];

			opxl[0]= colclamp((pxl[0]*blendfac[0])>>8);
			opxl[1]= colclamp((pxl[1]*blendfac[1])>>8);
			opxl[2]= colclamp((pxl[2]*blendfac[2])>>8);
			opxl[3]= pxl[3];
		}
	}

	glDrawPixels(icon->w, icon->h, GL_RGBA, GL_UNSIGNED_BYTE, temprect);
}
static void icon_free(Icon *icon)
{
	MEM_freeN(icon->data);
	MEM_freeN(icon);
}

/***/

typedef struct {
	unsigned char cols[BIFNCOLORSHADES][3];
} Color;

static Color *common_colors_arr= NULL;

static unsigned char *get_color(BIFColorID colorid, BIFColorShade shade)
{
	int coloridx= colorid-BIFCOLORID_FIRST;
	int shadeidx= shade-BIFCOLORSHADE_FIRST;
	if (coloridx>=0 && coloridx<BIFNCOLORIDS && shadeidx>=0&& shadeidx<BIFNCOLORSHADES) {
		return common_colors_arr[coloridx].cols[shadeidx];
	} else {
		static unsigned char errorcol[3]= {0xFF, 0x33, 0x33};

		return errorcol;
	}
}

void BIF_set_color(BIFColorID colorid, BIFColorShade shade)
{
	glColor3ubv(get_color(colorid, shade));
}

static void rgbaCCol_addNT(unsigned char *t, unsigned char *a, int N)
{
	t[0]= colclamp(a[0]+N);
	t[1]= colclamp(a[1]+N);
	t[2]= colclamp(a[2]+N);
}
static void def_col(BIFColorID colorid, unsigned char r, unsigned char g, unsigned char b)
{
	int coloridx= colorid-BIFCOLORID_FIRST;
	if (coloridx>=0 && coloridx<BIFNCOLORIDS) {
		unsigned char col[3];

		col[0]= r, col[1]= g, col[2]= b;
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_WHITE),	col, 60);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_LIGHT),	col, 35);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_HILITE),	col, 20);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_MEDIUM),	col, 0);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_GREY),		col, -45);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_DARK),		col, -60);
	} else {
		printf("def_col: Internal error, bad color ID: %d\n", colorid);
	}
}

/***/

static Icon **common_icons_arr= NULL;

static Icon *get_icon(BIFIconID icon)
{
	int iconidx= icon-BIFICONID_FIRST;
	if (iconidx>=0 && iconidx<BIFNICONIDS) {
		return common_icons_arr[iconidx];
	} else {
		return common_icons_arr[ICON_ERROR-BIFICONID_FIRST];
	}
}
static void free_common_icons(void)
{
	int i;

	for (i=0; i<BIFNICONIDS; i++) {
		icon_free(common_icons_arr[i+BIFICONID_FIRST]);
	}
}

void BIF_draw_icon(BIFIconID icon)
{
	icon_draw(get_icon(icon));
}
void BIF_draw_icon_blended(BIFIconID icon, BIFColorID color, BIFColorShade shade)
{
	icon_draw_blended(get_icon(icon), get_color(color, shade));
}
int BIF_get_icon_width(BIFIconID icon)
{
	return get_icon(icon)->w;
}
int BIF_get_icon_height(BIFIconID icon)
{
	return get_icon(icon)->h;
}

static void def_icon(ImBuf *bbuf, BIFIconID icon, int xidx, int yidx, int w, int h, int offsx, int offsy)
{
	int iconidx= icon-BIFICONID_FIRST;
	if (iconidx>=0 && iconidx<BIFNICONIDS) {
		int rowstride= bbuf->x*4;
		unsigned char *start= ((char*) bbuf->rect) + (yidx*21 + 3 + offsy)*rowstride + (xidx*20 + 3 + offsx)*4;
		common_icons_arr[iconidx]= icon_from_data(start, w, h, rowstride);
	} else {
		printf("def_icon: Internal error, bad icon ID: %d\n", icon);
	}
}

/***/

static void clear_transp_rect(unsigned char *transp, unsigned char *rect, int w, int h, int rowstride)
{
	int x,y;
	for (y=0; y<h; y++) {
		unsigned char *row= &rect[y*rowstride];
		for (x=0; x<w; x++) {
			unsigned char *pxl= &row[x*4];
			if (*((unsigned int*) pxl)==*((unsigned int*) transp)) {
				pxl[3]= 0;
			}
		}
	}
}

void BIF_resources_init(void)
{
	ImBuf *bbuf= IMB_ibImageFromMemory((int *)datatoc_blenderbuttons, datatoc_blenderbuttons_size, IB_rect);
	int x, y;

	common_icons_arr= MEM_mallocN(sizeof(*common_icons_arr)*BIFNICONIDS, "common_icons");
	common_colors_arr= MEM_mallocN(sizeof(*common_colors_arr)*BIFNCOLORIDS, "common_colors");

	for (y=0; y<10; y++) {
		for (x=0; x<21; x++) {
			int rowstride= bbuf->x*4;
			unsigned char *start= ((char*) bbuf->rect) + (y*21 + 3)*rowstride + (x*20 + 3)*4;
			unsigned char transp[4];
			transp[0]= start[0];
			transp[1]= start[1];
			transp[2]= start[2];
			transp[3]= start[3];
			clear_transp_rect(transp, start, 20, 21, rowstride);
		}
	} 

		/* hack! */
	for (y=0; y<10; y++) {
		for (x=0; x<21; x++) {
			if (x==11 && y==6) {
				def_icon(bbuf, ICON_BEVELBUT_HLT,			x, y, 7, 13, 4, 2);
			} else if (x==12 && y==6) {
				def_icon(bbuf, ICON_BEVELBUT_DEHLT,			x, y, 7, 13, 4, 2);
			} else {
				def_icon(bbuf, BIFICONID_FIRST + y*21 + x,	x, y, 15, 16, 0, 0);
			}
		}
	}

	IMB_freeImBuf(bbuf);

	def_col(BUTGREY,		0xB0,0xB0,0xB0);
	def_col(BUTGREEN,		0x88,0xA0,0xA4);
	def_col(BUTBLUE,		0xA0,0xA0,0xB0);
	def_col(BUTSALMON,		0xB0,0xA0,0x90);
	def_col(MIDGREY,		0x90,0x90,0x90);	
	def_col(BUTPURPLE,		0xA2,0x98,0xA9);
	def_col(BUTYELLOW,		0xB2,0xB2,0x99);
	def_col(BUTRUST,		0x80,0x70,0x70);
	def_col(REDALERT,		0xB0,0x40,0x40);
	def_col(BUTWHITE,		0xD0,0xD0,0xD0);
	def_col(BUTDBLUE,		0x80,0x80,0xA0);
	def_col(BUTDPINK,		0xAA,0x88,0x55);
	def_col(BUTPINK,		0xE8,0xBD,0xA7);
	def_col(BUTMACTIVE,		0x70,0x70,0xC0);

	def_col(BUTIPO,			0xB0,0xB0,0x99);
	def_col(BUTAUDIO,		0xB0,0xA0,0x90);
	def_col(BUTCAMERA,		0x99,0xB2,0xA5);
	def_col(BUTRANDOM,		0xA9,0x9A,0x98);
	def_col(BUTEDITOBJECT,	0xA2,0x98,0xA9);
	def_col(BUTPROPERTY,	0xA0,0xA0,0xB0);
	def_col(BUTSCENE,		0x99,0x99,0xB2);
	def_col(BUTMOTION,		0x98,0xA7,0xA9);
	def_col(BUTMESSAGE,		0x88,0xA0,0x94);
	def_col(BUTACTION,		0xB2,0xA9,0x99);
	def_col(BUTVISIBILITY,		0xB2,0xA9,0x99);
	def_col(BUTCD,			0xB0,0x95,0x90);
	def_col(BUTGAME,		0x99,0xB2,0x9C);
	def_col(BUTYUCK,		0xB0,0x99,0xB0);
	def_col(BUTSEASICK,		0x99,0xB0,0xB0);
	def_col(BUTCHOKE,		0x88,0x94,0xA0);
	def_col(BUTIMPERIAL,	0x94,0x88,0xA0);
}

void BIF_resources_free(void)
{
	free_common_icons();

	MEM_freeN(common_colors_arr);
	MEM_freeN(common_icons_arr);
}
