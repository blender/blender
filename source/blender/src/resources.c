

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_resources.h"

#include "BLI_blenlib.h"
#include "blendef.h"	// CLAMP
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
	
	if (coloridx>=0 && coloridx<BIFNCOLORIDS && shadeidx>=0 && shadeidx<BIFNCOLORSHADES) {
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
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_WHITE),	col, 80);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_LIGHT),	col, 45);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_HILITE),	col, 25);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_LMEDIUM),	col, 10);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_MEDIUM),	col, 0);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_LGREY),	col, -20);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_GREY),	col, -45);
		rgbaCCol_addNT(get_color(colorid, COLORSHADE_DARK),	col, -80);
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

	def_col(BUTGREY,		0x90,0x90,0x90);
	def_col(BUTGREEN,		0x88,0xA0,0xA4);
	def_col(BUTBLUE,		0xA0,0xA0,0xB0);
	def_col(BUTSALMON,		0xB0,0xA0,0x90);
	def_col(MIDGREY,		0xB0,0xB0,0xB0);	
	def_col(BUTPURPLE,		0xA2,0x98,0xA9);
	def_col(BUTYELLOW,		0xB2,0xB2,0x99);
	def_col(BUTRUST,		0x80,0x70,0x70);
	def_col(REDALERT,		0xB0,0x40,0x40);
	def_col(BUTWHITE,		0xD0,0xD0,0xD0);
	def_col(BUTDBLUE,		0x80,0x80,0xA0);
	def_col(BUTDPINK,		0xAA,0x88,0x55);
	def_col(BUTPINK,		0xE8,0xBD,0xA7);
	def_col(BUTMACTIVE,		0x30,0x30,0x30);

	def_col(ACTIONBUTCOL,	0x88,0x88,0x88);
	def_col(NUMBUTCOL,		0x88,0x88,0x88);
	def_col(TEXBUTCOL,		0x88,0x88,0x88);
	def_col(TOGBUTCOL,		0x88,0x88,0x88);
	def_col(SLIDERCOL,		0x88,0x88,0x88);
	def_col(TABCOL,			0x88,0x88,0x88);
	def_col(MENUCOL,		0xCF,0xCF,0xCF);
	def_col(MENUACTIVECOL,	0x80,0x80,0x80);

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
	def_col(BUTVISIBILITY,	0xB2,0xA9,0x99);
	def_col(BUTCD,			0xB0,0x95,0x90);
	def_col(BUTGAME,		0x99,0xB2,0x9C);
	def_col(BUTYUCK,		0xB0,0x99,0xB0);
	def_col(BUTSEASICK,		0x99,0xB0,0xB0);
	def_col(BUTCHOKE,		0x88,0x94,0xA0);
	def_col(BUTIMPERIAL,	0x94,0x88,0xA0);
	
	def_col(HEADERCOL,		165, 165, 165);
	def_col(HEADERCOLSEL,	185, 185, 185);
	
}

void BIF_resources_free(void)
{
	free_common_icons();

	MEM_freeN(common_colors_arr);
	MEM_freeN(common_icons_arr);
	
}


/* ******************************************************** */
/*    THEMES */
/* ******************************************************** */


char *BIF_ThemeGetColorPtr(bTheme *btheme, int spacetype, int colorid)
{
	ThemeSpace *ts= NULL;
	char error[3]={240, 0, 240};
	char *cp= error;
	
	if(btheme) {
		// first check for ui buttons theme
		if(colorid < TH_THEMEUI) {
			
		}
		else {
		
			switch(spacetype) {
			case SPACE_BUTS:
				ts= &btheme->tbuts;
				break;
			case SPACE_VIEW3D:
				ts= &btheme->tv3d;
				break;
			case SPACE_IPO:
				ts= &btheme->tipo;
				break;
			case SPACE_FILE:
				ts= &btheme->tfile;
				break;
			default:
				ts= &btheme->tv3d;
				break;
			}
			
			switch(colorid) {
			case TH_BACK:
				cp= ts->back; break;
			case TH_TEXT:
				cp= ts->text; break;
			case TH_TEXT_HI:
				cp= ts->text_hi; break;
			case TH_HEADER:
				cp= ts->header; break;
			case TH_PANEL:
				cp= ts->panel; break;
			case TH_SHADE1:
				cp= ts->shade1; break;
			case TH_SHADE2:
				cp= ts->shade2; break;
			case TH_HILITE:
				cp= ts->hilite; break;
				
			case TH_GRID:
				cp= ts->grid; break;
			case TH_WIRE:
				cp= ts->wire; break;
			case TH_SELECT:
				cp= ts->select; break;
			case TH_ACTIVE:
				cp= ts->active; break;
			case TH_TRANSFORM:
				cp= ts->transform; break;
			case TH_VERTEX:
				cp= ts->vertex; break;
			case TH_VERTEX_SELECT:
				cp= ts->vertex_select; break;
			case TH_VERTEX_SIZE:
				cp= &ts->vertex_size; break;
			case TH_EDGE:
				cp= ts->edge; break;
			case TH_EDGE_SELECT:
				cp= ts->edge_select; break;
			case TH_FACE:
				cp= ts->face; break;
			case TH_FACE_SELECT:
				cp= ts->face_select; break;

			}

		}
	}
	
	return cp;
}

#define SETCOL(col, r, g, b, a)  col[0]=r; col[1]=g; col[2]= b; col[3]= a;

// initialize
void BIF_InitTheme(void)
{
	bTheme *btheme= U.themes.first;
	
	/* we search for the theme with name Default */
	for(btheme= U.themes.first; btheme; btheme= btheme->next) {
		if(strcmp("Default", btheme->name)==0) break;
	}
	
	if(btheme==NULL) {
		btheme= MEM_callocN(sizeof(bTheme), "theme");
		BLI_addtail(&U.themes, btheme);
		strcpy(btheme->name, "Default");
	}
	
	BIF_SetTheme(NULL);	// make sure the global used in this file is set
	
	/* UI buttons (todo) */
	
	/* space view3d */
	SETCOL(btheme->tv3d.back, 	115, 115, 115, 255);
	SETCOL(btheme->tv3d.text, 	0, 0, 0, 255);
	SETCOL(btheme->tv3d.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tv3d.header, 195, 195, 195, 255);
	SETCOL(btheme->tv3d.panel, 	165, 165, 165, 100);
	
	SETCOL(btheme->tv3d.shade1,  160, 160, 160, 100);
	SETCOL(btheme->tv3d.shade2,  0x7f, 0x70, 0x70, 100);

	SETCOL(btheme->tv3d.grid, 	0x58, 0x58, 0x58, 255);
	SETCOL(btheme->tv3d.wire, 	0x0, 0x0, 0x0, 255);
	SETCOL(btheme->tv3d.select, 0xff, 0x88, 0xff, 255);
	SETCOL(btheme->tv3d.active, 0xff, 0xbb, 0xff, 255);
	SETCOL(btheme->tv3d.transform, 0xff, 0xff, 0xff, 255);
	SETCOL(btheme->tv3d.vertex, 0xff, 0x70, 0xff, 255);
	SETCOL(btheme->tv3d.vertex_select, 0xff, 0xff, 0x70, 255);
	btheme->tv3d.vertex_size= 2;
	SETCOL(btheme->tv3d.edge, 	0x0, 0x0, 0x0, 255);
	SETCOL(btheme->tv3d.edge_select, 0x90, 0x90, 0x30, 255);
	SETCOL(btheme->tv3d.face, 	0, 50, 150, 30);
	SETCOL(btheme->tv3d.face_select, 200, 100, 200, 60);

	/* space buttons */
	/* to have something initialized */
	btheme->tbuts= btheme->tv3d;

	SETCOL(btheme->tbuts.back, 	180, 180, 180, 255);
	SETCOL(btheme->tbuts.header, 195, 195, 195, 255);
	SETCOL(btheme->tbuts.panel,  255, 255, 255, 40);

	/* space ipo */
	/* to have something initialized */
	btheme->tipo= btheme->tv3d;

	SETCOL(btheme->tipo.grid, 	94, 94, 94, 255);
	SETCOL(btheme->tipo.back, 	120, 120, 120, 255);
	SETCOL(btheme->tipo.header, 195, 195, 195, 255);
	SETCOL(btheme->tipo.panel,  255, 255, 255, 100);
	SETCOL(btheme->tipo.shade1,  140, 140, 140, 100);
	SETCOL(btheme->tipo.shade2,  0x7f, 0x70, 0x70, 100);
	SETCOL(btheme->tipo.vertex, 0xff, 0x70, 0xff, 255);
	SETCOL(btheme->tipo.vertex_select, 0xff, 0xff, 0x70, 255);
	SETCOL(btheme->tipo.hilite, 0x60, 0xc0, 0x40, 255); // green cfra line

	/* space file */
	/* to have something initialized */
	btheme->tfile= btheme->tv3d;
	SETCOL(btheme->tfile.back, 	128, 128, 128, 255);
	SETCOL(btheme->tfile.text, 	0, 0, 0, 255);
	SETCOL(btheme->tfile.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tfile.header, 182, 182, 182, 255);
	SETCOL(btheme->tfile.hilite, 0xA0, 0xA0, 0xD0, 255); // selected files

	btheme->tinfo= btheme->tv3d;
	
	btheme->tsnd= btheme->tv3d;
	SETCOL(btheme->tsnd.grid,0x70, 0x70, 0x60, 255);
	
	btheme->tact= btheme->tv3d;
	btheme->tnla= btheme->tv3d;
	btheme->tseq= btheme->tv3d;
	btheme->tima= btheme->tv3d;
	btheme->timasel= btheme->tv3d;
	btheme->text= btheme->tv3d;
	btheme->toops= btheme->tv3d;


}

char *BIF_ThemeColorsPup(int spacetype)
{
	char *cp= MEM_callocN(20*32, "theme pup");
	char str[32];
	
	if(spacetype==0) {
		strcpy(cp, "Not Yet");
	}
	else {
		// first defaults for each space
		sprintf(str, "Background %%x%d|", TH_BACK); strcat(cp, str);
		sprintf(str, "Text %%x%d|", TH_TEXT); strcat(cp, str);
		sprintf(str, "Text Hilite %%x%d|", TH_TEXT_HI); strcat(cp, str);
		sprintf(str, "Header %%x%d|", TH_HEADER); strcat(cp, str);
		
		if(spacetype==SPACE_VIEW3D) {
			sprintf(str, "Panel %%x%d|", TH_PANEL); strcat(cp, str);
			strcat(cp,"%l|");
			sprintf(str, "Grid %%x%d|", TH_GRID); strcat(cp, str);
			sprintf(str, "Wire %%x%d|", TH_WIRE); strcat(cp, str);
			sprintf(str, "Object Selected %%x%d|", TH_SELECT); strcat(cp, str);
			sprintf(str, "Object Active %%x%d|", TH_ACTIVE); strcat(cp, str);
			sprintf(str, "Transform %%x%d|", TH_TRANSFORM); strcat(cp, str);
			strcat(cp,"%l|");
			sprintf(str, "Vertex %%x%d|", TH_VERTEX); strcat(cp, str);
			sprintf(str, "Vertex Selected %%x%d|", TH_VERTEX_SELECT); strcat(cp, str);
			sprintf(str, "Vertex Size %%x%d|", TH_VERTEX_SIZE); strcat(cp, str);
			//sprintf(str, "Edge %%x%d|", TH_EDGE); strcat(cp, str);
			sprintf(str, "Edge Selected %%x%d|", TH_EDGE_SELECT); strcat(cp, str);
			sprintf(str, "Face %%x%d|", TH_FACE); strcat(cp, str);
			// last item without '|'
			sprintf(str, "Face Selected %%x%d", TH_FACE_SELECT); strcat(cp, str);
		}
		else if(spacetype==SPACE_IPO) {
			sprintf(str, "Panel %%x%d|", TH_PANEL); strcat(cp, str);
			strcat(cp,"%l|");
			sprintf(str, "Main Shade %%x%d|", TH_SHADE1); strcat(cp, str);
			sprintf(str, "Alt Shade %%x%d|", TH_SHADE2); strcat(cp, str);
			sprintf(str, "Vertex %%x%d|", TH_VERTEX); strcat(cp, str);
			sprintf(str, "Vertex Selected %%x%d|", TH_VERTEX_SELECT); strcat(cp, str);
			sprintf(str, "Current frame %%x%d", TH_HILITE); strcat(cp, str);
			// last item without '|'
		}
		else if(spacetype==SPACE_FILE) {
			sprintf(str, "Selected file %%x%d", TH_HILITE); strcat(cp, str);
		}
	}
	return cp;
}

static bTheme *theme_active=NULL;
static int theme_spacetype= SPACE_VIEW3D;

void BIF_SetTheme(ScrArea *sa)
{
	if(sa==NULL) {	// called for safety, when delete themes
		theme_active= U.themes.first;
		theme_spacetype= SPACE_VIEW3D;
	}
	else {
		// later on, a local theme can be found too
		theme_active= U.themes.first;
		theme_spacetype= sa->spacetype;
	
	}
}

// for space windows only
void BIF_ThemeColor(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	glColor3ub(cp[0], cp[1], cp[2]);

}

// plus alpha
void BIF_ThemeColor4(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	glColor4ub(cp[0], cp[1], cp[2], cp[3]);

}

// set the color with offset for shades
void BIF_ThemeColorShade(int colorid, int offset)
{
	int r, g, b;
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	r= offset + (int) cp[0];
	CLAMP(r, 0, 255);
	g= offset + (int) cp[1];
	CLAMP(g, 0, 255);
	b= offset + (int) cp[2];
	CLAMP(b, 0, 255);
	glColor3ub(r, g, b);
}

// blend between to theme colors, and set it
void BIF_ThemeColorBlend(int colorid1, int colorid2, float fac)
{
	int r, g, b;
	char *cp1, *cp2;
	
	cp1= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	if(fac<0.0) fac=0.0; else if(fac>1.0) fac= 1.0;
	r= floor((1.0-fac)*cp1[0] + fac*cp2[0]);
	g= floor((1.0-fac)*cp1[1] + fac*cp2[1]);
	b= floor((1.0-fac)*cp1[2] + fac*cp2[2]);
	
	glColor3ub(r, g, b);
}


// get individual values, not scaled
float BIF_GetThemeColorf(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((float)cp[0]);

}

// get the color, range 0.0-1.0
void BIF_GetThemeColor3fv(int colorid, float *col)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= ((float)cp[0])/255.0;
	col[1]= ((float)cp[1])/255.0;
	col[2]= ((float)cp[2])/255.0;
}

// get the color, in char pointer
void BIF_GetThemeColor3ubv(int colorid, char *col)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= cp[0];
	col[1]= cp[1];
	col[2]= cp[2];
}

// get the color, in char pointer
void BIF_GetThemeColor4ubv(int colorid, char *col)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	col[0]= cp[0];
	col[1]= cp[1];
	col[2]= cp[2];
	col[3]= cp[3];
}


