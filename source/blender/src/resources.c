

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

/* global for themes */
static bTheme *theme_active=NULL;
static int theme_spacetype= SPACE_VIEW3D;

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

#if 0

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
#endif

static void icon_draw_blended(Icon *icon, char *blendcol, int shade)
{
	/* commented out, for now only alpha (ton) */
//	float r, g, b;
	
//	r= (-shade + (float)blendcol[0])/180.0;
//	g= (-shade + (float)blendcol[1])/180.0;
//	b= (-shade + (float)blendcol[2])/180.0;
	
//	glPixelTransferf(GL_RED_SCALE, r>0.0?r:0.0);
//	glPixelTransferf(GL_GREEN_SCALE, g>0.0?g:0.0);
//	glPixelTransferf(GL_BLUE_SCALE, b>0.0?b:0.0);

	if(shade < 0) {
		float r= (128+shade)/128.0;
		glPixelTransferf(GL_ALPHA_SCALE, r);
	}

	glDrawPixels(icon->w, icon->h, GL_RGBA, GL_UNSIGNED_BYTE, icon->data);

//	glPixelTransferf(GL_RED_SCALE, 1.0);
//	glPixelTransferf(GL_GREEN_SCALE, 1.0);
//	glPixelTransferf(GL_BLUE_SCALE, 1.0);
	glPixelTransferf(GL_ALPHA_SCALE, 1.0);

}


static void icon_free(Icon *icon)
{
	MEM_freeN(icon->data);
	MEM_freeN(icon);
}


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

void BIF_draw_icon_blended(BIFIconID icon, int colorid, int shade)
{
	char *cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	icon_draw_blended(get_icon(icon), cp, shade);
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
}

void BIF_resources_free(void)
{
	free_common_icons();

	MEM_freeN(common_icons_arr);
	
}


/* ******************************************************** */
/*    THEMES */
/* ******************************************************** */

char *BIF_ThemeGetColorPtr(bTheme *btheme, int spacetype, int colorid)
{
	ThemeSpace *ts= NULL;
	static char error[4]={240, 0, 240, 255};
	static char alert[4]={240, 60, 60, 255};
	static char headerdesel[4]={0,0,0,255};
	static char custom[4]={0,0,0,255};
	
	char *cp= error;
	
	if(btheme) {
	
		// first check for ui buttons theme
		if(colorid < TH_THEMEUI) {
		
			switch(colorid) {
			case TH_BUT_OUTLINE:
				cp= btheme->tui.outline; break;
			case TH_BUT_NEUTRAL:
				cp= btheme->tui.neutral; break;
			case TH_BUT_ACTION:
				cp= btheme->tui.action; break;
			case TH_BUT_SETTING:
				cp= btheme->tui.setting; break;
			case TH_BUT_SETTING1:
				cp= btheme->tui.setting1; break;
			case TH_BUT_SETTING2:
				cp= btheme->tui.setting2; break;
			case TH_BUT_NUM:
				cp= btheme->tui.num; break;
			case TH_BUT_TEXTFIELD:
				cp= btheme->tui.textfield; break;
			case TH_BUT_POPUP:
				cp= btheme->tui.popup; break;
			case TH_BUT_TEXT:
				cp= btheme->tui.text; break;
			case TH_BUT_TEXT_HI:
				cp= btheme->tui.text_hi; break;
			case TH_MENU_BACK:
				cp= btheme->tui.menu_back; break;
			case TH_MENU_ITEM:
				cp= btheme->tui.menu_item; break;
			case TH_MENU_HILITE:
				cp= btheme->tui.menu_hilite; break;
			case TH_MENU_TEXT:
				cp= btheme->tui.menu_text; break;
			case TH_MENU_TEXT_HI:
				cp= btheme->tui.menu_text_hi; break;
			
			case TH_BUT_DRAWTYPE:
				cp= &btheme->tui.but_drawtype; break;
			
			case TH_REDALERT:
				cp= alert; break;
			case TH_CUSTOM:
				cp= custom; break;
			}
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
			case SPACE_NLA:
				ts= &btheme->tnla;
				break;
			case SPACE_ACTION:
				ts= &btheme->tact;
				break;
			case SPACE_SEQ:
				ts= &btheme->tseq;
				break;
			case SPACE_IMAGE:
				ts= &btheme->tima;
				break;
			case SPACE_IMASEL:
				ts= &btheme->timasel;
				break;
			case SPACE_TEXT:
				ts= &btheme->text;
				break;
			case SPACE_OOPS:
				ts= &btheme->toops;
				break;
			case SPACE_SOUND:
				ts= &btheme->tsnd;
				break;
			case SPACE_INFO:
				ts= &btheme->tinfo;
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
			case TH_HEADERDESEL:
				/* we calculate a dynamic builtin header deselect color, also for pulldowns... */
				cp= ts->header; 
				headerdesel[0]= cp[0]>10?cp[0]-10:0;
				headerdesel[1]= cp[1]>10?cp[1]-10:0;
				headerdesel[2]= cp[2]>10?cp[2]-10:0;
				cp= headerdesel;
				break;
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
	SETCOL(btheme->tui.outline, 	0xA0,0xA0,0xA0, 255);
	SETCOL(btheme->tui.neutral, 	0xA0,0xA0,0xA0, 255);
	SETCOL(btheme->tui.action, 		0xAD,0xA0,0x93, 255);
	SETCOL(btheme->tui.setting, 	0x8A,0x9E,0xA1, 255);
	SETCOL(btheme->tui.setting1, 	0xA1,0xA1,0xAE, 255);
	SETCOL(btheme->tui.setting2, 	0xA1,0x99,0xA7, 255);
	SETCOL(btheme->tui.num,		 	0x90,0x90,0x90, 255);
	SETCOL(btheme->tui.textfield,	0x90,0x90,0x90, 255);
	SETCOL(btheme->tui.popup,		0xA0,0xA0,0xA0, 255);
	
	SETCOL(btheme->tui.text,		0,0,0, 255);
	SETCOL(btheme->tui.text_hi, 	255, 255, 255, 255);
	
	SETCOL(btheme->tui.menu_back, 	0xD2,0xD2,0xD2, 255);
	SETCOL(btheme->tui.menu_item, 	0xDA,0xDA,0xDA, 255);
	SETCOL(btheme->tui.menu_hilite, 0x7F,0x7F,0x7F, 255);
	SETCOL(btheme->tui.menu_text, 	0, 0, 0, 255);
	SETCOL(btheme->tui.menu_text_hi, 255, 255, 255, 255);
	btheme->tui.but_drawtype= 1;
	
	/* space view3d */
	SETCOL(btheme->tv3d.back, 	115, 115, 115, 255);
	SETCOL(btheme->tv3d.text, 	0, 0, 0, 255);
	SETCOL(btheme->tv3d.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tv3d.header, 195, 195, 195, 255);
	SETCOL(btheme->tv3d.panel, 	165, 165, 165, 127);
	
	SETCOL(btheme->tv3d.shade1,  160, 160, 160, 100);
	SETCOL(btheme->tv3d.shade2,  0x7f, 0x70, 0x70, 100);

	SETCOL(btheme->tv3d.grid, 	92, 92, 92, 255);
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
	SETCOL(btheme->tipo.panel,  255, 255, 255, 150);
	SETCOL(btheme->tipo.shade1,  172, 172, 172, 100);
	SETCOL(btheme->tipo.shade2,  0x70, 0x70, 0x70, 100);
	SETCOL(btheme->tipo.vertex, 0xff, 0x70, 0xff, 255);
	SETCOL(btheme->tipo.vertex_select, 0xff, 0xff, 0x70, 255);
	SETCOL(btheme->tipo.hilite, 0x60, 0xc0, 0x40, 255); 

	/* space file */
	/* to have something initialized */
	btheme->tfile= btheme->tv3d;
	SETCOL(btheme->tfile.back, 	128, 128, 128, 255);
	SETCOL(btheme->tfile.text, 	0, 0, 0, 255);
	SETCOL(btheme->tfile.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tfile.header, 182, 182, 182, 255);
	SETCOL(btheme->tfile.hilite, 0xA0, 0xA0, 0xD0, 255); // selected files

	
	/* space action */
	btheme->tact= btheme->tv3d;
	SETCOL(btheme->tact.back, 	116, 116, 116, 255);
	SETCOL(btheme->tact.text, 	0, 0, 0, 255);
	SETCOL(btheme->tact.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tact.header, 182, 182, 182, 255);
	SETCOL(btheme->tact.grid,  94, 94, 94, 255);
	SETCOL(btheme->tact.face,  166, 166, 166, 255);	// RVK
	SETCOL(btheme->tact.shade1,  172, 172, 172, 255);		// sliders
	SETCOL(btheme->tact.shade2,  84, 44, 31, 100);	// bar
	SETCOL(btheme->tact.hilite,  17, 27, 60, 100);	// bar

	/* space nla */
	btheme->tnla= btheme->tv3d;
	SETCOL(btheme->tnla.back, 	116, 116, 116, 255);
	SETCOL(btheme->tnla.text, 	0, 0, 0, 255);
	SETCOL(btheme->tnla.text_hi, 255, 255, 255, 255);
	SETCOL(btheme->tnla.header, 182, 182, 182, 255);
	SETCOL(btheme->tnla.grid,  94, 94, 94, 255);	
	SETCOL(btheme->tnla.shade1,  172, 172, 172, 255);		// sliders
	SETCOL(btheme->tnla.shade2,  84, 44, 31, 100);	// bar
	SETCOL(btheme->tnla.hilite,  17, 27, 60, 100);	// bar
	
	/* space seq */
	btheme->tseq= btheme->tv3d;
	SETCOL(btheme->tnla.back, 	116, 116, 116, 255);

	/* space image */
	btheme->tima= btheme->tv3d;
	SETCOL(btheme->tima.back, 	53, 53, 53, 255);
	SETCOL(btheme->tima.vertex, 0xff, 0x70, 0xff, 255);
	SETCOL(btheme->tima.vertex_select, 0xff, 0xff, 0x70, 255);

	/* space imageselect */
	btheme->timasel= btheme->tv3d;
	SETCOL(btheme->timasel.back, 	110, 110, 110, 255);
	SETCOL(btheme->timasel.shade1, 	0xaa, 0xaa, 0xba, 255);

	/* space text */
	btheme->text= btheme->tv3d;
	SETCOL(btheme->text.back, 	153, 153, 153, 255);
	SETCOL(btheme->text.shade1, 	143, 143, 143, 255);
	SETCOL(btheme->text.shade2, 	0xc6, 0x77, 0x77, 255);
	SETCOL(btheme->text.hilite, 	255, 0, 0, 255);

	/* space oops */
	btheme->toops= btheme->tv3d;
	SETCOL(btheme->toops.back, 	153, 153, 153, 255);

	/* space info */
	btheme->tinfo= btheme->tv3d;
	SETCOL(btheme->tinfo.back, 	153, 153, 153, 255);

	/* space sound */
	btheme->tsnd= btheme->tv3d;
	SETCOL(btheme->tsnd.back, 	153, 153, 153, 255);
	SETCOL(btheme->tsnd.shade1,  173, 173, 173, 255);		// sliders
	SETCOL(btheme->tsnd.grid, 140, 140, 140, 255);
	

}

char *BIF_ThemeColorsPup(int spacetype)
{
	char *cp= MEM_callocN(20*32, "theme pup");
	char str[32];
	
	if(spacetype==0) {
		sprintf(str, "Outline %%x%d|", TH_BUT_OUTLINE); strcat(cp, str);
		sprintf(str, "Neutral %%x%d|", TH_BUT_NEUTRAL); strcat(cp, str);
		sprintf(str, "Action %%x%d|", TH_BUT_ACTION); strcat(cp, str);
		sprintf(str, "Setting %%x%d|", TH_BUT_SETTING); strcat(cp, str);
		sprintf(str, "Special Setting 1%%x%d|", TH_BUT_SETTING1); strcat(cp, str);
		sprintf(str, "Special Setting 2 %%x%d|", TH_BUT_SETTING2); strcat(cp, str);
		sprintf(str, "Number Input %%x%d|", TH_BUT_NUM); strcat(cp, str);
		sprintf(str, "Text Input %%x%d|", TH_BUT_TEXTFIELD); strcat(cp, str);
		sprintf(str, "Popup %%x%d|", TH_BUT_POPUP); strcat(cp, str);
		sprintf(str, "Text %%x%d|", TH_BUT_TEXT); strcat(cp, str);
		sprintf(str, "Text Highlight %%x%d|", TH_BUT_TEXT_HI); strcat(cp, str);
			strcat(cp,"%l|");
		sprintf(str, "Menu Background %%x%d|", TH_MENU_BACK); strcat(cp, str);
		sprintf(str, "Menu Item %%x%d|", TH_MENU_ITEM); strcat(cp, str);
		sprintf(str, "Menu Item Highlight %%x%d|", TH_MENU_HILITE); strcat(cp, str);
		sprintf(str, "Menu Text %%x%d|", TH_MENU_TEXT); strcat(cp, str);
		sprintf(str, "Menu Text Highlight %%x%d|", TH_MENU_TEXT_HI); strcat(cp, str);
		strcat(cp,"%l|");
		sprintf(str, "Drawtype %%x%d|", TH_BUT_DRAWTYPE); strcat(cp, str);
	}
	else {
		// first defaults for each space
		sprintf(str, "Background %%x%d|", TH_BACK); strcat(cp, str);
		sprintf(str, "Text %%x%d|", TH_TEXT); strcat(cp, str);
		sprintf(str, "Text Highlight %%x%d|", TH_TEXT_HI); strcat(cp, str);
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
			sprintf(str, "Edge Selected %%x%d|", TH_EDGE_SELECT); strcat(cp, str);
			sprintf(str, "Face %%x%d|", TH_FACE); strcat(cp, str);
			sprintf(str, "Face Selected %%x%d", TH_FACE_SELECT); strcat(cp, str);
		}
		else if(spacetype==SPACE_IPO) {
			sprintf(str, "Panel %%x%d|", TH_PANEL); strcat(cp, str);
			strcat(cp,"%l|");
			sprintf(str, "Window Sliders %%x%d|", TH_SHADE1); strcat(cp, str);
			sprintf(str, "Ipo Channels %%x%d|", TH_SHADE2); strcat(cp, str);
			sprintf(str, "Vertex %%x%d|", TH_VERTEX); strcat(cp, str);
			sprintf(str, "Vertex Selected %%x%d|", TH_VERTEX_SELECT); strcat(cp, str);
		}
		else if(spacetype==SPACE_FILE) {
			sprintf(str, "Selected file %%x%d", TH_HILITE); strcat(cp, str);
		}
		else if(spacetype==SPACE_NLA) {
			//sprintf(str, "Panel %%x%d|", TH_PANEL); strcat(cp, str);
			strcat(cp,"%l|");
			sprintf(str, "View Sliders %%x%d|", TH_SHADE1); strcat(cp, str);
			sprintf(str, "Bars %%x%d|", TH_SHADE2); strcat(cp, str);
			sprintf(str, "Bars selected %%x%d|", TH_HILITE); strcat(cp, str);
		}
		else if(spacetype==SPACE_ACTION) {
			//sprintf(str, "Panel %%x%d|", TH_PANEL); strcat(cp, str);
			strcat(cp,"%l|");
			sprintf(str, "RVK Sliders %%x%d|", TH_FACE); strcat(cp, str);
			sprintf(str, "View Sliders %%x%d|", TH_SHADE1); strcat(cp, str);
			sprintf(str, "Channels %%x%d|", TH_SHADE2); strcat(cp, str);
			sprintf(str, "Channels Selected %%x%d|", TH_HILITE); strcat(cp, str);
		}
		else if(spacetype==SPACE_SEQ) {
			sprintf(str, "Window Sliders %%x%d|", TH_SHADE1); strcat(cp, str);
		}
		else if(spacetype==SPACE_SOUND) {
			sprintf(str, "Window Slider %%x%d|", TH_SHADE1); strcat(cp, str);
		}
		else if(spacetype==SPACE_BUTS) {
			sprintf(str, "Panel %%x%d|", TH_PANEL); strcat(cp, str);
		}
		else if(spacetype==SPACE_IMASEL) {
			sprintf(str, "Main Shade %%x%d|", TH_SHADE1); strcat(cp, str);
		}
		else if(spacetype==SPACE_TEXT) {
			sprintf(str, "Scroll Bar %%x%d|", TH_SHADE1); strcat(cp, str);
			sprintf(str, "Selected Text %%x%d|", TH_SHADE2); strcat(cp, str);
			sprintf(str, "Cursor %%x%d|", TH_HILITE); strcat(cp, str);
		}
	}
	return cp;
}

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
	//glColor3ub(r, g, b);
	glColor4ub(r, g, b, cp[3]);
}
void BIF_ThemeColorShadeAlpha(int colorid, int coloffset, int alphaoffset)
{
	int r, g, b, a;
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	r= coloffset + (int) cp[0];
	CLAMP(r, 0, 255);
	g= coloffset + (int) cp[1];
	CLAMP(g, 0, 255);
	b= coloffset + (int) cp[2];
	CLAMP(b, 0, 255);
	a= alphaoffset + (int) cp[3];
	CLAMP(a, 0, 255);
	glColor4ub(r, g, b, a);
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

// blend between to theme colors, shade it, and set it
void BIF_ThemeColorBlendShade(int colorid1, int colorid2, float fac, int offset)
{
	int r, g, b;
	char *cp1, *cp2;
	
	cp1= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid1);
	cp2= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid2);

	if(fac<0.0) fac=0.0; else if(fac>1.0) fac= 1.0;
	r= offset+floor((1.0-fac)*cp1[0] + fac*cp2[0]);
	g= offset+floor((1.0-fac)*cp1[1] + fac*cp2[1]);
	b= offset+floor((1.0-fac)*cp1[2] + fac*cp2[2]);
	
	r= r<0?0:(r>255?255:r);
	g= g<0?0:(g>255?255:g);
	b= b<0?0:(b>255?255:b);
	
	glColor3ub(r, g, b);
}


// get individual values, not scaled
float BIF_GetThemeValuef(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((float)cp[0]);

}

// get individual values, not scaled
int BIF_GetThemeValue(int colorid)
{
	char *cp;
	
	cp= BIF_ThemeGetColorPtr(theme_active, theme_spacetype, colorid);
	return ((int) cp[0]);

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


