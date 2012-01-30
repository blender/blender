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
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_draw.c
 *  \ingroup edinterface
 */


#include <math.h>
#include <string.h>

#include "DNA_color_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_movieclip_types.h"

#include "BLI_math.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_colortools.h"
#include "BKE_texture.h"


#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "BLF_api.h"

#include "UI_interface.h"

/* own include */
#include "interface_intern.h"

#define UI_DISABLED_ALPHA_OFFS	-160

static int roundboxtype= UI_CNR_ALL;

void uiSetRoundBox(int type)
{
	/* Not sure the roundbox function is the best place to change this
	 * if this is undone, its not that big a deal, only makes curves edges
	 * square for the  */
	roundboxtype= type;
	
}

int uiGetRoundBox(void)
{
	return roundboxtype;
}

void uiDrawBox(int mode, float minx, float miny, float maxx, float maxy, float rad)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
					  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	int a;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}

	glBegin(mode);

	/* start with corner right-bottom */
	if(roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		glVertex2f(maxx-rad, miny);
		for(a=0; a<7; a++) {
			glVertex2f(maxx-rad+vec[a][0], miny+vec[a][1]);
		}
		glVertex2f(maxx, miny+rad);
	}
	else glVertex2f(maxx, miny);
	
	/* corner right-top */
	if(roundboxtype & UI_CNR_TOP_RIGHT) {
		glVertex2f(maxx, maxy-rad);
		for(a=0; a<7; a++) {
			glVertex2f(maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		glVertex2f(maxx-rad, maxy);
	}
	else glVertex2f(maxx, maxy);
	
	/* corner left-top */
	if(roundboxtype & UI_CNR_TOP_LEFT) {
		glVertex2f(minx+rad, maxy);
		for(a=0; a<7; a++) {
			glVertex2f(minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		glVertex2f(minx, maxy-rad);
	}
	else glVertex2f(minx, maxy);
	
	/* corner left-bottom */
	if(roundboxtype & UI_CNR_BOTTOM_LEFT) {
		glVertex2f(minx, miny+rad);
		for(a=0; a<7; a++) {
			glVertex2f(minx+vec[a][1], miny+rad-vec[a][0]);
		}
		glVertex2f(minx+rad, miny);
	}
	else glVertex2f(minx, miny);
	
	glEnd();
}

static void round_box_shade_col(const float col1[3], float const col2[3], const float fac)
{
	float col[3];

	col[0]= (fac*col1[0] + (1.0f-fac)*col2[0]);
	col[1]= (fac*col1[1] + (1.0f-fac)*col2[1]);
	col[2]= (fac*col1[2] + (1.0f-fac)*col2[2]);
	glColor3fv(col);
}

/* linear horizontal shade within button or in outline */
/* view2d scrollers use it */
void uiDrawBoxShade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
					  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	const float div= maxy - miny;
	const float idiv= 1.0f / div;
	float coltop[3], coldown[3], color[4];
	int a;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}
	/* get current color, needs to be outside of glBegin/End */
	glGetFloatv(GL_CURRENT_COLOR, color);

	/* 'shade' defines strength of shading */	
	coltop[0]= color[0]+shadetop; if(coltop[0]>1.0f) coltop[0]= 1.0f;
	coltop[1]= color[1]+shadetop; if(coltop[1]>1.0f) coltop[1]= 1.0f;
	coltop[2]= color[2]+shadetop; if(coltop[2]>1.0f) coltop[2]= 1.0f;
	coldown[0]= color[0]+shadedown; if(coldown[0]<0.0f) coldown[0]= 0.0f;
	coldown[1]= color[1]+shadedown; if(coldown[1]<0.0f) coldown[1]= 0.0f;
	coldown[2]= color[2]+shadedown; if(coldown[2]<0.0f) coldown[2]= 0.0f;

	glShadeModel(GL_SMOOTH);
	glBegin(mode);

	/* start with corner right-bottom */
	if(roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(maxx-rad, miny);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(coltop, coldown, vec[a][1]*idiv);
			glVertex2f(maxx-rad+vec[a][0], miny+vec[a][1]);
		}
		
		round_box_shade_col(coltop, coldown, rad*idiv);
		glVertex2f(maxx, miny+rad);
	}
	else {
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if(roundboxtype & UI_CNR_TOP_RIGHT) {
		
		round_box_shade_col(coltop, coldown, (div-rad)*idiv);
		glVertex2f(maxx, maxy-rad);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(coltop, coldown, (div-rad+vec[a][1])*idiv);
			glVertex2f(maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(maxx-rad, maxy);
	}
	else {
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(maxx, maxy);
	}
	
	/* corner left-top */
	if(roundboxtype & UI_CNR_TOP_LEFT) {
		
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(minx+rad, maxy);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(coltop, coldown, (div-vec[a][1])*idiv);
			glVertex2f(minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		
		round_box_shade_col(coltop, coldown, (div-rad)*idiv);
		glVertex2f(minx, maxy-rad);
	}
	else {
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(minx, maxy);
	}
	
	/* corner left-bottom */
	if(roundboxtype & UI_CNR_BOTTOM_LEFT) {
		
		round_box_shade_col(coltop, coldown, rad*idiv);
		glVertex2f(minx, miny+rad);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(coltop, coldown, (rad-vec[a][1])*idiv);
			glVertex2f(minx+vec[a][1], miny+rad-vec[a][0]);
		}
		
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(minx+rad, miny);
	}
	else {
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(minx, miny);
	}
	
	glEnd();
	glShadeModel(GL_FLAT);
}

/* linear vertical shade within button or in outline */
/* view2d scrollers use it */
void uiDrawBoxVerticalShade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadeLeft, float shadeRight)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
					  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	const float div= maxx - minx;
	const float idiv= 1.0f / div;
	float colLeft[3], colRight[3], color[4];
	int a;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}
	/* get current color, needs to be outside of glBegin/End */
	glGetFloatv(GL_CURRENT_COLOR, color);

	/* 'shade' defines strength of shading */	
	colLeft[0]= color[0]+shadeLeft; if(colLeft[0]>1.0f) colLeft[0]= 1.0f;
	colLeft[1]= color[1]+shadeLeft; if(colLeft[1]>1.0f) colLeft[1]= 1.0f;
	colLeft[2]= color[2]+shadeLeft; if(colLeft[2]>1.0f) colLeft[2]= 1.0f;
	colRight[0]= color[0]+shadeRight; if(colRight[0]<0.0f) colRight[0]= 0.0f;
	colRight[1]= color[1]+shadeRight; if(colRight[1]<0.0f) colRight[1]= 0.0f;
	colRight[2]= color[2]+shadeRight; if(colRight[2]<0.0f) colRight[2]= 0.0f;

	glShadeModel(GL_SMOOTH);
	glBegin(mode);

	/* start with corner right-bottom */
	if(roundboxtype & UI_CNR_BOTTOM_RIGHT) {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx-rad, miny);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(colLeft, colRight, vec[a][0]*idiv);
			glVertex2f(maxx-rad+vec[a][0], miny+vec[a][1]);
		}
		
		round_box_shade_col(colLeft, colRight, rad*idiv);
		glVertex2f(maxx, miny+rad);
	}
	else {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if(roundboxtype & UI_CNR_TOP_RIGHT) {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, maxy-rad);
		
		for(a=0; a<7; a++) {
			
			round_box_shade_col(colLeft, colRight, (div-rad-vec[a][0])*idiv);
			glVertex2f(maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		round_box_shade_col(colLeft, colRight, (div-rad)*idiv);
		glVertex2f(maxx-rad, maxy);
	}
	else {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, maxy);
	}
	
	/* corner left-top */
	if(roundboxtype & UI_CNR_TOP_LEFT) {
		round_box_shade_col(colLeft, colRight, (div-rad)*idiv);
		glVertex2f(minx+rad, maxy);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(colLeft, colRight, (div-rad+vec[a][0])*idiv);
			glVertex2f(minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, maxy-rad);
	}
	else {
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, maxy);
	}
	
	/* corner left-bottom */
	if(roundboxtype & UI_CNR_BOTTOM_LEFT) {
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, miny+rad);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(colLeft, colRight, (vec[a][0])*idiv);
			glVertex2f(minx+vec[a][1], miny+rad-vec[a][0]);
		}
		
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx+rad, miny);
	}
	else {
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, miny);
	}
	
	glEnd();
	glShadeModel(GL_FLAT);
}

/* plain antialiased unfilled rectangle */
void uiRoundRect(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if(roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3]= 0.5;
		glColor4fv(color);
		glEnable( GL_BLEND );
	}
	
	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );

	uiDrawBox(GL_LINE_LOOP, minx, miny, maxx, maxy, rad);
   
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );
}

/* (old, used in outliner) plain antialiased filled box */
void uiRoundBox(float minx, float miny, float maxx, float maxy, float rad)
{
	float color[4];
	
	if(roundboxtype & UI_RB_ALPHA) {
		glGetFloatv(GL_CURRENT_COLOR, color);
		color[3]= 0.5;
		glColor4fv(color);
		glEnable( GL_BLEND );
	}
	
	ui_draw_anti_roundbox(GL_POLYGON, minx, miny, maxx, maxy, rad);
}


/* ************** generic embossed rect, for window sliders etc ************* */


/* text_draw.c uses this */
void uiEmboss(float x1, float y1, float x2, float y2, int sel)
{
	
	/* below */
	if(sel) glColor3ub(200,200,200);
	else glColor3ub(50,50,50);
	fdrawline(x1, y1, x2, y1);

	/* right */
	fdrawline(x2, y1, x2, y2);
	
	/* top */
	if(sel) glColor3ub(50,50,50);
	else glColor3ub(200,200,200);
	fdrawline(x1, y2, x2, y2);

	/* left */
	fdrawline(x1, y1, x1, y2);
	
}

/* ************** SPECIAL BUTTON DRAWING FUNCTIONS ************* */

void ui_draw_but_IMAGE(ARegion *UNUSED(ar), uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *rect)
{
#ifdef WITH_HEADLESS
	(void)rect;
	(void)but;
#else
	ImBuf *ibuf= (ImBuf *)but->poin;
	//GLint scissor[4];
	//int w, h;

	if (!ibuf) return;
	
	/* scissor doesn't seem to be doing the right thing...?
	//glColor4f(1.0, 0.f, 0.f, 1.f);
	//fdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax)

	w = (rect->xmax - rect->xmin);
	h = (rect->ymax - rect->ymin);
	// prevent drawing outside widget area
	glGetIntegerv(GL_SCISSOR_BOX, scissor);
	glScissor(ar->winrct.xmin + rect->xmin, ar->winrct.ymin + rect->ymin, w, h);
	*/
	
	glEnable(GL_BLEND);
	glColor4f(0.0, 0.0, 0.0, 0.0);
	
	glaDrawPixelsSafe((float)rect->xmin, (float)rect->ymin, ibuf->x, ibuf->y, ibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect);
	//glaDrawPixelsTex((float)rect->xmin, (float)rect->ymin, ibuf->x, ibuf->y, GL_UNSIGNED_BYTE, ibuf->rect);
	
	glDisable(GL_BLEND);
	
	/* 
	// restore scissortest
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
	*/
	
#endif
}

#if 0
#ifdef WITH_INTERNATIONAL
static void ui_draw_but_CHARTAB(uiBut *but)
{
	/* XXX 2.50 bad global access */
	/* Some local variables */
	float sx, sy, ex, ey;
	float width, height;
	float butw, buth;
	int x, y, cs;
	wchar_t wstr[2];
	unsigned char ustr[16];
	PackedFile *pf;
	int result = 0;
	int charmax = G.charmax;
	
	/* FO_BUILTIN_NAME font in use. There are TTF FO_BUILTIN_NAME and non-TTF FO_BUILTIN_NAME fonts */
	if(!strcmp(G.selfont->name, FO_BUILTIN_NAME)) {
		if(G.ui_international == TRUE) {
			charmax = 0xff;
		}
		else {
			charmax = 0xff;
		}
	}

	/* Category list exited without selecting the area */
	if(G.charmax == 0)
		charmax = G.charmax = 0xffff;

	/* Calculate the size of the button */
	width = abs(rect->xmax - rect->xmin);
	height = abs(rect->ymax - rect->ymin);
	
	butw = floor(width / 12);
	buth = floor(height / 6);
	
	/* Initialize variables */
	sx = rect->xmin;
	ex = rect->xmin + butw;
	sy = rect->ymin + height - buth;
	ey = rect->ymin + height;

	cs = G.charstart;

	/* Set the font, in case it is not FO_BUILTIN_NAME font */
	if(G.selfont && strcmp(G.selfont->name, FO_BUILTIN_NAME)) {
		// Is the font file packed, if so then use the packed file
		if(G.selfont->packedfile) {
			pf = G.selfont->packedfile;		
			FTF_SetFont(pf->data, pf->size, 14.0);
		}
		else {
			char tmpStr[256];
			int err;

			BLI_strncpy(tmpStr, G.selfont->name, sizeof(tmpStr));
			BLI_path_abs(tmpStr, G.main->name);
			err = FTF_SetFont((unsigned char *)tmpStr, 0, 14.0);
		}
	}
	else {
		if(G.ui_international == TRUE) {
			FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, 14.0);
		}
	}

	/* Start drawing the button itself */
	glShadeModel(GL_SMOOTH);

	glColor3ub(200,  200,  200);
	glRectf((rect->xmin), (rect->ymin), (rect->xmax), (rect->ymax));

	glColor3ub(0,  0,  0);
	for(y = 0; y < 6; y++) {
		// Do not draw more than the category allows
		if(cs > charmax) break;

		for(x = 0; x < 12; x++)
		{
			// Do not draw more than the category allows
			if(cs > charmax) break;

			// Draw one grid cell
			glBegin(GL_LINE_LOOP);
				glVertex2f(sx, sy);
				glVertex2f(ex, sy);
				glVertex2f(ex, ey);
				glVertex2f(sx, ey);				
			glEnd();	

			// Draw character inside the cell
			memset(wstr, 0, sizeof(wchar_t)*2);
			memset(ustr, 0, 16);

			// Set the font to be either unicode or FO_BUILTIN_NAME	
			wstr[0] = cs;
			if(strcmp(G.selfont->name, FO_BUILTIN_NAME))
			{
				BLI_strncpy_wchar_as_utf8((char *)ustr, (wchar_t *)wstr, sizeof(ustr));
			}
			else
			{
				if(G.ui_international == TRUE)
				{
					BLI_strncpy_wchar_as_utf8((char *)ustr, (wchar_t *)wstr, sizeof(ustr));
				}
				else
				{
					ustr[0] = cs;
					ustr[1] = 0;
				}
			}

			if((G.selfont && strcmp(G.selfont->name, FO_BUILTIN_NAME)) || (G.selfont && !strcmp(G.selfont->name, FO_BUILTIN_NAME) && G.ui_international == TRUE))
			{
				float wid;
				float llx, lly, llz, urx, ury, urz;
				float dx, dy;
				float px, py;
	
				// Calculate the position
				wid = FTF_GetStringWidth((char *) ustr, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
				FTF_GetBoundingBox((char *) ustr, &llx,&lly,&llz,&urx,&ury,&urz, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
				dx = urx-llx;
				dy = ury-lly;

				// This isn't fully functional since the but->aspect isn't working like I suspected
				px = sx + ((butw/but->aspect)-dx)/2;
				py = sy + ((buth/but->aspect)-dy)/2;

				// Set the position and draw the character
				ui_rasterpos_safe(px, py, but->aspect);
				FTF_DrawString((char *) ustr, FTF_USE_GETTEXT | FTF_INPUT_UTF8);
			}
			else
			{
				ui_rasterpos_safe(sx + butw/2, sy + buth/2, but->aspect);
				UI_DrawString(but->font, (char *) ustr, 0);
			}
	
			// Calculate the next position and character
			sx += butw; ex +=butw;
			cs++;
		}
		/* Add the y position and reset x position */
		sy -= buth; 
		ey -= buth;
		sx = rect->xmin;
		ex = rect->xmin + butw;
	}	
	glShadeModel(GL_FLAT);

	/* Return Font Settings to original */
	if(U.fontsize && U.fontname[0]) {
		result = FTF_SetFont((unsigned char *)U.fontname, 0, U.fontsize);
	}
	else if (U.fontsize) {
		result = FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, U.fontsize);
	}

	if (result == 0) {
		result = FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, 11);
	}
	
	/* resets the font size */
	if(G.ui_international == TRUE) {
		// uiSetCurFont(but->block, UI_HELV);
	}
}

#endif // WITH_INTERNATIONAL
#endif

static void draw_scope_end(rctf *rect, GLint *scissor)
{
	float scaler_x1, scaler_x2;
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	
	/* scale widget */
	scaler_x1 = rect->xmin + (rect->xmax - rect->xmin)/2 - SCOPE_RESIZE_PAD;
	scaler_x2 = rect->xmin + (rect->xmax - rect->xmin)/2 + SCOPE_RESIZE_PAD;
	
	glColor4f(0.f, 0.f, 0.f, 0.25f);
	fdrawline(scaler_x1, rect->ymin-4, scaler_x2, rect->ymin-4);
	fdrawline(scaler_x1, rect->ymin-7, scaler_x2, rect->ymin-7);
	glColor4f(1.f, 1.f, 1.f, 0.25f);
	fdrawline(scaler_x1, rect->ymin-5, scaler_x2, rect->ymin-5);
	fdrawline(scaler_x1, rect->ymin-8, scaler_x2, rect->ymin-8);
	
	/* outline */
	glColor4f(0.f, 0.f, 0.f, 0.5f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_LINE_LOOP, rect->xmin-1, rect->ymin, rect->xmax+1, rect->ymax+1, 3.0f);
}

static void histogram_draw_one(float r, float g, float b, float alpha, float x, float y, float w, float h, float *data, int res)
{
	int i;
	
	/* under the curve */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	glColor4f(r, g, b, alpha);
	
	glShadeModel(GL_FLAT);
	glBegin(GL_QUAD_STRIP);
	glVertex2f(x, y);
	glVertex2f(x, y + (data[0]*h));
	for (i=1; i < res; i++) {
		float x2 = x + i * (w/(float)res);
		glVertex2f(x2, y + (data[i]*h));
		glVertex2f(x2, y);
	}
	glEnd();
	
	/* curve outline */
	glColor4f(0.f, 0.f, 0.f, 0.25f);
	
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);
	glBegin(GL_LINE_STRIP);
	for (i=0; i < res; i++) {
		float x2 = x + i * (w/(float)res);
		glVertex2f(x2, y + (data[i]*h));
	}
	glEnd();
	glDisable(GL_LINE_SMOOTH);
}

void ui_draw_but_HISTOGRAM(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	Histogram *hist = (Histogram *)but->poin;
	int res = hist->x_resolution;
	rctf rect;
	int i;
	float w, h;
	//float alpha;
	GLint scissor[4];
	
	rect.xmin = (float)recti->xmin+1;
	rect.xmax = (float)recti->xmax-1;
	rect.ymin = (float)recti->ymin+SCOPE_RESIZE_PAD+2;
	rect.ymax = (float)recti->ymax-1;
	
	w = rect.xmax - rect.xmin;
	h = (rect.ymax - rect.ymin) * hist->ymax;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	
	glColor4f(0.f, 0.f, 0.f, 0.3f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_POLYGON, rect.xmin-1, rect.ymin-1, rect.xmax+1, rect.ymax+1, 3.0f);

	/* need scissor test, histogram can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin-1), ar->winrct.ymin+(rect.ymin-1), (rect.xmax+1)-(rect.xmin-1), (rect.ymax+1)-(rect.ymin-1));

	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid lines here */
	for (i=1; i<4; i++) {
		fdrawline(rect.xmin, rect.ymin+(i/4.f)*h, rect.xmax, rect.ymin+(i/4.f)*h);
		fdrawline(rect.xmin+(i/4.f)*w, rect.ymin, rect.xmin+(i/4.f)*w, rect.ymax);
	}
	
	if (hist->mode == HISTO_MODE_LUMA)
		histogram_draw_one(1.0, 1.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_luma, res);
	else {
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_R)
			histogram_draw_one(1.0, 0.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_r, res);
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_G)
			histogram_draw_one(0.0, 1.0, 0.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_g, res);
		if (hist->mode == HISTO_MODE_RGB || hist->mode == HISTO_MODE_B)
			histogram_draw_one(0.0, 0.0, 1.0, 0.75, rect.xmin, rect.ymin, w, h, hist->data_b, res);
	}
	
	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);
}

void ui_draw_but_WAVEFORM(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	Scopes *scopes = (Scopes *)but->poin;
	rctf rect;
	int i, c;
	float w, w3, h, alpha, yofs;
	GLint scissor[4];
	float colors[3][3]= MAT3_UNITY;
	float colorsycc[3][3] = {{1,0,1},{1,1,0},{0,1,1}};
	float colors_alpha[3][3], colorsycc_alpha[3][3]; /* colors  pre multiplied by alpha for speed up */
	float min, max;
	
	if (scopes==NULL) return;
	
	rect.xmin = (float)recti->xmin+1;
	rect.xmax = (float)recti->xmax-1;
	rect.ymin = (float)recti->ymin+SCOPE_RESIZE_PAD+2;
	rect.ymax = (float)recti->ymax-1;
	
	if (scopes->wavefrm_yfac < 0.5f )
		scopes->wavefrm_yfac =0.98f;
	w = rect.xmax - rect.xmin-7;
	h = (rect.ymax - rect.ymin)*scopes->wavefrm_yfac;
	yofs= rect.ymin + (rect.ymax - rect.ymin -h)/2.0f;
	w3=w/3.0f;
	
	/* log scale for alpha */
	alpha = scopes->wavefrm_alpha*scopes->wavefrm_alpha;
	
	for(c=0; c<3; c++) {
		for(i=0; i<3; i++) {
			colors_alpha[c][i] = colors[c][i] * alpha;
			colorsycc_alpha[c][i] = colorsycc[c][i] * alpha;
		}
	}
			
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	
	glColor4f(0.f, 0.f, 0.f, 0.3f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_POLYGON, rect.xmin-1, rect.ymin-1, rect.xmax+1, rect.ymax+1, 3.0f);
	

	/* need scissor test, waveform can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin-1), ar->winrct.ymin+(rect.ymin-1), (rect.xmax+1)-(rect.xmin-1), (rect.ymax+1)-(rect.ymin-1));

	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid lines here */
	for (i=0; i<6; i++) {
		char str[4];
		BLI_snprintf(str, sizeof(str), "%-3d",i*20);
		str[3]='\0';
		fdrawline(rect.xmin+22, yofs+(i/5.f)*h, rect.xmax+1, yofs+(i/5.f)*h);
		BLF_draw_default(rect.xmin+1, yofs-5+(i/5.f)*h, 0, str, sizeof(str)-1);
		/* in the loop because blf_draw reset it */
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	}
	/* 3 vertical separation */
	if (scopes->wavefrm_mode!= SCOPES_WAVEFRM_LUMA) {
		for (i=1; i<3; i++) {
			fdrawline(rect.xmin+i*w3, rect.ymin, rect.xmin+i*w3, rect.ymax);
		}
	}
	
	/* separate min max zone on the right */
	fdrawline(rect.xmin+w, rect.ymin, rect.xmin+w, rect.ymax);
	/* 16-235-240 level in case of ITU-R BT601/709 */
	glColor4f(1.f, 0.4f, 0.f, 0.2f);
	if (ELEM(scopes->wavefrm_mode, SCOPES_WAVEFRM_YCC_601, SCOPES_WAVEFRM_YCC_709)){
		fdrawline(rect.xmin+22, yofs+h*16.0f/255.0f, rect.xmax+1, yofs+h*16.0f/255.0f);
		fdrawline(rect.xmin+22, yofs+h*235.0f/255.0f, rect.xmin+w3, yofs+h*235.0f/255.0f);
		fdrawline(rect.xmin+3*w3, yofs+h*235.0f/255.0f, rect.xmax+1, yofs+h*235.0f/255.0f);
		fdrawline(rect.xmin+w3, yofs+h*240.0f/255.0f, rect.xmax+1, yofs+h*240.0f/255.0f);
	}
	/* 7.5 IRE black point level for NTSC */
	if (scopes->wavefrm_mode== SCOPES_WAVEFRM_LUMA)
		fdrawline(rect.xmin, yofs+h*0.075f, rect.xmax+1, yofs+h*0.075f);

	if (scopes->ok && scopes->waveform_1 != NULL) {
		
		/* LUMA (1 channel) */
		glBlendFunc(GL_ONE,GL_ONE);
		glColor3f(alpha, alpha, alpha);
		if (scopes->wavefrm_mode == SCOPES_WAVEFRM_LUMA){

			glBlendFunc(GL_ONE,GL_ONE);
			
			glPushMatrix();
			glEnableClientState(GL_VERTEX_ARRAY);
			
			glTranslatef(rect.xmin, yofs, 0.f);
			glScalef(w, h, 0.f);
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_1);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);
					
			glDisableClientState(GL_VERTEX_ARRAY);
			glPopMatrix();

			/* min max */
			glColor3f(.5f, .5f, .5f);
			min= yofs+scopes->minmax[0][0]*h;
			max= yofs+scopes->minmax[0][1]*h;
			CLAMP(min, rect.ymin, rect.ymax);
			CLAMP(max, rect.ymin, rect.ymax);
			fdrawline(rect.xmax-3,min,rect.xmax-3,max);
		}

		/* RGB / YCC (3 channels) */
		else if (ELEM4(scopes->wavefrm_mode, SCOPES_WAVEFRM_RGB, SCOPES_WAVEFRM_YCC_601, SCOPES_WAVEFRM_YCC_709, SCOPES_WAVEFRM_YCC_JPEG)) {
			int rgb = (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB);
			
			glBlendFunc(GL_ONE,GL_ONE);
			
			glPushMatrix();
			glEnableClientState(GL_VERTEX_ARRAY);
			
			glTranslatef(rect.xmin, yofs, 0.f);
			glScalef(w3, h, 0.f);
			
			glColor3fv((rgb)?colors_alpha[0]:colorsycc_alpha[0]);
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_1);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);

			glTranslatef(1.f, 0.f, 0.f);
			glColor3fv((rgb)?colors_alpha[1]:colorsycc_alpha[1]);
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_2);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);
			
			glTranslatef(1.f, 0.f, 0.f);
			glColor3fv((rgb)?colors_alpha[2]:colorsycc_alpha[2]);
			glVertexPointer(2, GL_FLOAT, 0, scopes->waveform_3);
			glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);
			
			glDisableClientState(GL_VERTEX_ARRAY);
			glPopMatrix();

			
			/* min max */
			for (c=0; c<3; c++) {
				if (scopes->wavefrm_mode == SCOPES_WAVEFRM_RGB)
					glColor3f(colors[c][0]*0.75f, colors[c][1]*0.75f, colors[c][2]*0.75f);
				else
					glColor3f(colorsycc[c][0]*0.75f, colorsycc[c][1]*0.75f, colorsycc[c][2]*0.75f);
				min= yofs+scopes->minmax[c][0]*h;
				max= yofs+scopes->minmax[c][1]*h;
				CLAMP(min, rect.ymin, rect.ymax);
				CLAMP(max, rect.ymin, rect.ymax);
				fdrawline(rect.xmin+w+2+c*2,min,rect.xmin+w+2+c*2,max);
			}
		}
		
	}
	
	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);
}

static float polar_to_x(float center, float diam, float ampli, float angle)
{
	return center + diam * ampli * cosf(angle);
}

static float polar_to_y(float center, float diam, float ampli, float angle)
{
	return center + diam * ampli * sinf(angle);
}

static void vectorscope_draw_target(float centerx, float centery, float diam, const float colf[3])
{
	float y,u,v;
	float tangle=0.f, tampli;
	float dangle, dampli, dangle2, dampli2;

	rgb_to_yuv(colf[0], colf[1], colf[2], &y, &u, &v);
	if (u>0 && v>=0) tangle=atanf(v/u);
	else if (u>0 && v<0) tangle= atanf(v/u) + 2.0f * (float)M_PI;
	else if (u<0) tangle=atanf(v/u) + (float)M_PI;
	else if (u==0 && v > 0.0f) tangle= (float)M_PI/2.0f;
	else if (u==0 && v < 0.0f) tangle=-(float)M_PI/2.0f;
	tampli= sqrtf(u*u+v*v);

	/* small target vary by 2.5 degree and 2.5 IRE unit */
	glColor4f(1.0f, 1.0f, 1.0, 0.12f);
	dangle= DEG2RADF(2.5f);
	dampli= 2.5f/200.0f;
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli,tangle+dangle), polar_to_y(centery,diam,tampli+dampli,tangle+dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli-dampli,tangle+dangle), polar_to_y(centery,diam,tampli-dampli,tangle+dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli-dampli,tangle-dangle), polar_to_y(centery,diam,tampli-dampli,tangle-dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli,tangle-dangle), polar_to_y(centery,diam,tampli+dampli,tangle-dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli,tangle+dangle), polar_to_y(centery,diam,tampli+dampli,tangle+dangle));
	glEnd();
	/* big target vary by 10 degree and 20% amplitude */
	glColor4f(1.0f, 1.0f, 1.0, 0.12f);
	dangle= DEG2RADF(10.0f);
	dampli= 0.2f*tampli;
	dangle2= DEG2RADF(5.0f);
	dampli2= 0.5f*dampli;
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli-dampli2,tangle+dangle), polar_to_y(centery,diam,tampli+dampli-dampli2,tangle+dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli,tangle+dangle), polar_to_y(centery,diam,tampli+dampli,tangle+dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli,tangle+dangle-dangle2), polar_to_y(centery,diam,tampli+dampli,tangle+dangle-dangle2));
	glEnd();
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx,diam,tampli-dampli+dampli2,tangle+dangle), polar_to_y(centery ,diam,tampli-dampli+dampli2,tangle+dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli-dampli,tangle+dangle), polar_to_y(centery,diam,tampli-dampli,tangle+dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli-dampli,tangle+dangle-dangle2), polar_to_y(centery,diam,tampli-dampli,tangle+dangle-dangle2));
	glEnd();
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx,diam,tampli-dampli+dampli2,tangle-dangle), polar_to_y(centery,diam,tampli-dampli+dampli2,tangle-dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli-dampli,tangle-dangle), polar_to_y(centery,diam,tampli-dampli,tangle-dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli-dampli,tangle-dangle+dangle2), polar_to_y(centery,diam,tampli-dampli,tangle-dangle+dangle2));
	glEnd();
	glBegin(GL_LINE_STRIP);
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli-dampli2,tangle-dangle), polar_to_y(centery,diam,tampli+dampli-dampli2,tangle-dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli,tangle-dangle), polar_to_y(centery,diam,tampli+dampli,tangle-dangle));
	glVertex2f(polar_to_x(centerx,diam,tampli+dampli,tangle-dangle+dangle2), polar_to_y(centery,diam,tampli+dampli,tangle-dangle+dangle2));
	glEnd();
}

void ui_draw_but_VECTORSCOPE(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	const float skin_rad= DEG2RADF(123.0f); /* angle in radians of the skin tone line */
	Scopes *scopes = (Scopes *)but->poin;
	rctf rect;
	int i, j;
	float w, h, centerx, centery, diam;
	float alpha;
	const float colors[6][3]={{.75,0,0},{.75,.75,0},{0,.75,0},{0,.75,.75},{0,0,.75},{.75,0,.75}};
	GLint scissor[4];
	
	rect.xmin = (float)recti->xmin+1;
	rect.xmax = (float)recti->xmax-1;
	rect.ymin = (float)recti->ymin+SCOPE_RESIZE_PAD+2;
	rect.ymax = (float)recti->ymax-1;
	
	w = rect.xmax - rect.xmin;
	h = rect.ymax - rect.ymin;
	centerx = rect.xmin + w/2;
	centery = rect.ymin + h/2;
	diam= (w<h)?w:h;
	
	alpha = scopes->vecscope_alpha*scopes->vecscope_alpha*scopes->vecscope_alpha;
			
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	
	glColor4f(0.f, 0.f, 0.f, 0.3f);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_POLYGON, rect.xmin-1, rect.ymin-1, rect.xmax+1, rect.ymax+1, 3.0f);

	/* need scissor test, hvectorscope can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin-1), ar->winrct.ymin+(rect.ymin-1), (rect.xmax+1)-(rect.xmin-1), (rect.ymax+1)-(rect.ymin-1));
	
	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid elements */
	/* cross */
	fdrawline(centerx - (diam/2)-5, centery, centerx + (diam/2)+5, centery);
	fdrawline(centerx, centery - (diam/2)-5, centerx, centery + (diam/2)+5);
	/* circles */
	for(j=0; j<5; j++) {
		glBegin(GL_LINE_STRIP);
		for(i=0; i<=360; i=i+15) {
			const float a= DEG2RADF((float)i);
			const float r= (j+1)/10.0f;
			glVertex2f(polar_to_x(centerx,diam,r,a), polar_to_y(centery,diam,r,a));
		}
		glEnd();
	}
	/* skin tone line */
	glColor4f(1.f, 0.4f, 0.f, 0.2f);
	fdrawline(polar_to_x(centerx, diam, 0.5f, skin_rad), polar_to_y(centery,diam,0.5,skin_rad),
	          polar_to_x(centerx, diam, 0.1f, skin_rad), polar_to_y(centery,diam,0.1,skin_rad));
	/* saturation points */
	for(i=0; i<6; i++)
		vectorscope_draw_target(centerx, centery, diam, colors[i]);
	
	if (scopes->ok && scopes->vecscope != NULL) {
		/* pixel point cloud */
		glBlendFunc(GL_ONE,GL_ONE);
		glColor3f(alpha, alpha, alpha);

		glPushMatrix();
		glEnableClientState(GL_VERTEX_ARRAY);

		glTranslatef(centerx, centery, 0.f);
		glScalef(diam, diam, 0.f);

		glVertexPointer(2, GL_FLOAT, 0, scopes->vecscope);
		glDrawArrays(GL_POINTS, 0, scopes->waveform_tot);
		
		glDisableClientState(GL_VERTEX_ARRAY);
		glPopMatrix();
	}

	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);
		
	glDisable(GL_BLEND);
}

void ui_draw_but_COLORBAND(uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *rect)
{
	ColorBand *coba;
	CBData *cbd;
	float x1, y1, sizex, sizey;
	float v3[2], v1[2], v2[2], v1a[2], v2a[2];
	int a;
	float pos, colf[4]= {0,0,0,0}; /* initialize incase the colorband isnt valid */
		
	coba= (ColorBand *)(but->editcoba? but->editcoba: but->poin);
	if(coba==NULL) return;
	
	x1= rect->xmin;
	y1= rect->ymin;
	sizex= rect->xmax-x1;
	sizey= rect->ymax-y1;

	/* first background, to show tranparency */

	glColor4ub(UI_TRANSP_DARK, UI_TRANSP_DARK, UI_TRANSP_DARK, 255);
	glRectf(x1, y1, x1+sizex, y1+sizey);
	glEnable(GL_POLYGON_STIPPLE);
	glColor4ub(UI_TRANSP_LIGHT, UI_TRANSP_LIGHT, UI_TRANSP_LIGHT, 255);
	glPolygonStipple(checker_stipple_sml);
	glRectf(x1, y1, x1+sizex, y1+sizey);
	glDisable(GL_POLYGON_STIPPLE);

	glShadeModel(GL_FLAT);
	glEnable(GL_BLEND);
	
	cbd= coba->data;
	
	v1[0]= v2[0]= x1;
	v1[1]= y1;
	v2[1]= y1+sizey;
	
	glBegin(GL_QUAD_STRIP);
	
	glColor4fv( &cbd->r );
	glVertex2fv(v1); glVertex2fv(v2);
	
	for( a = 1; a <= sizex; a++ ) {
		pos = ((float)a) / (sizex-1);
		do_colorband( coba, pos, colf );
		if (but->block->color_profile != BLI_PR_NONE)
			linearrgb_to_srgb_v3_v3(colf, colf);
		
		v1[0]=v2[0]= x1 + a;
		
		glColor4fv( colf );
		glVertex2fv(v1); glVertex2fv(v2);
	}
	
	glEnd();
	glShadeModel(GL_FLAT);
	glDisable(GL_BLEND);
	
	/* outline */
	glColor4f(0.0, 0.0, 0.0, 1.0);
	fdrawbox(x1, y1, x1+sizex, y1+sizey);
	
	/* help lines */
	v1[0]= v2[0]=v3[0]= x1;
	v1[1]= y1;
	v1a[1]= y1+0.25f*sizey;
	v2[1]= y1+0.5f*sizey;
	v2a[1]= y1+0.75f*sizey;
	v3[1]= y1+sizey;
	
	
	cbd= coba->data;
	glBegin(GL_LINES);
	for(a=0; a<coba->tot; a++, cbd++) {
		v1[0]=v2[0]=v3[0]=v1a[0]=v2a[0]= x1+ cbd->pos*sizex;
		
		if(a==coba->cur) {
			glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v3);
			glEnd();
			
			setlinestyle(2);
			glBegin(GL_LINES);
			glColor3ub(255, 255, 255);
			glVertex2fv(v1);
			glVertex2fv(v3);
			glEnd();
			setlinestyle(0);
			glBegin(GL_LINES);
			
			/* glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v1a);
			glColor3ub(255, 255, 255);
			glVertex2fv(v1a);
			glVertex2fv(v2);
			glColor3ub(0, 0, 0);
			glVertex2fv(v2);
			glVertex2fv(v2a);
			glColor3ub(255, 255, 255);
			glVertex2fv(v2a);
			glVertex2fv(v3);
			*/
		}
		else {
			glColor3ub(0, 0, 0);
			glVertex2fv(v1);
			glVertex2fv(v2);
			
			glColor3ub(255, 255, 255);
			glVertex2fv(v2);
			glVertex2fv(v3);
		}	
	}
	glEnd();

}

void ui_draw_but_NORMAL(uiBut *but, uiWidgetColors *wcol, rcti *rect)
{
	static GLuint displist=0;
	int a, old[8];
	GLfloat diff[4], diffn[4]={1.0f, 1.0f, 1.0f, 1.0f};
	float vec0[4]={0.0f, 0.0f, 0.0f, 0.0f};
	float dir[4], size;
	
	/* store stuff */
	glGetMaterialfv(GL_FRONT, GL_DIFFUSE, diff);
		
	/* backdrop */
	glColor3ubv((unsigned char*)wcol->inner);
	uiSetRoundBox(UI_CNR_ALL);
	uiDrawBox(GL_POLYGON, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 5.0f);
	
	/* sphere color */
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diffn);
	glCullFace(GL_BACK); glEnable(GL_CULL_FACE);
	
	/* disable blender light */
	for(a=0; a<8; a++) {
		old[a]= glIsEnabled(GL_LIGHT0+a);
		glDisable(GL_LIGHT0+a);
	}
	
	/* own light */
	glEnable(GL_LIGHT7);
	glEnable(GL_LIGHTING);
	
	ui_get_but_vectorf(but, dir);

	dir[3]= 0.0f;	/* glLight needs 4 args, 0.0 is sun */
	glLightfv(GL_LIGHT7, GL_POSITION, dir); 
	glLightfv(GL_LIGHT7, GL_DIFFUSE, diffn); 
	glLightfv(GL_LIGHT7, GL_SPECULAR, vec0); 
	glLightf(GL_LIGHT7, GL_CONSTANT_ATTENUATION, 1.0f);
	glLightf(GL_LIGHT7, GL_LINEAR_ATTENUATION, 0.0f);
	
	/* transform to button */
	glPushMatrix();
	glTranslatef(rect->xmin + 0.5f*(rect->xmax-rect->xmin), rect->ymin+ 0.5f*(rect->ymax-rect->ymin), 0.0f);
	
	if( rect->xmax-rect->xmin < rect->ymax-rect->ymin)
		size= (rect->xmax-rect->xmin)/200.f;
	else
		size= (rect->ymax-rect->ymin)/200.f;
	
	glScalef(size, size, size);
	
	if(displist==0) {
		GLUquadricObj	*qobj;
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE_AND_EXECUTE);
		
		qobj= gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_FILL); 
		glShadeModel(GL_SMOOTH);
		gluSphere( qobj, 100.0, 32, 24);
		glShadeModel(GL_FLAT);
		gluDeleteQuadric(qobj);  
		
		glEndList();
	}
	else glCallList(displist);
	
	/* restore */
	glDisable(GL_LIGHTING);
	glDisable(GL_CULL_FACE);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, diff); 
	glDisable(GL_LIGHT7);
	
	/* AA circle */
	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH );
	glColor3ubv((unsigned char*)wcol->inner);
	glutil_draw_lined_arc(0.0f, M_PI*2.0, 100.0f, 32);
	glDisable(GL_BLEND);
	glDisable(GL_LINE_SMOOTH );

	/* matrix after circle */
	glPopMatrix();

	/* enable blender light */
	for(a=0; a<8; a++) {
		if(old[a])
			glEnable(GL_LIGHT0+a);
	}
}

static void ui_draw_but_curve_grid(rcti *rect, float zoomx, float zoomy, float offsx, float offsy, float step)
{
	float dx, dy, fx, fy;
	
	glBegin(GL_LINES);
	dx= step*zoomx;
	fx= rect->xmin + zoomx*(-offsx);
	if(fx > rect->xmin) fx -= dx*(floorf(fx-rect->xmin));
	while(fx < rect->xmax) {
		glVertex2f(fx, rect->ymin); 
		glVertex2f(fx, rect->ymax);
		fx+= dx;
	}
	
	dy= step*zoomy;
	fy= rect->ymin + zoomy*(-offsy);
	if(fy > rect->ymin) fy -= dy*(floorf(fy-rect->ymin));
	while(fy < rect->ymax) {
		glVertex2f(rect->xmin, fy); 
		glVertex2f(rect->xmax, fy);
		fy+= dy;
	}
	glEnd();
	
}

static void glColor3ubvShade(unsigned char *col, int shade)
{
	glColor3ub(col[0]-shade>0?col[0]-shade:0,
	           col[1]-shade>0?col[1]-shade:0,
	           col[2]-shade>0?col[2]-shade:0);
}

void ui_draw_but_CURVE(ARegion *ar, uiBut *but, uiWidgetColors *wcol, rcti *rect)
{
	CurveMapping *cumap;
	CurveMap *cuma;
	CurveMapPoint *cmp;
	float fx, fy, fac[2], zoomx, zoomy, offsx, offsy;
	GLint scissor[4];
	rcti scissor_new;
	int a;

	cumap= (CurveMapping *)(but->editcumap? but->editcumap: but->poin);
	cuma= cumap->cm+cumap->cur;
	
	/* need scissor test, curve can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	scissor_new.xmin= ar->winrct.xmin + rect->xmin;
	scissor_new.ymin= ar->winrct.ymin + rect->ymin;
	scissor_new.xmax= ar->winrct.xmin + rect->xmax;
	scissor_new.ymax= ar->winrct.ymin + rect->ymax;
	BLI_isect_rcti(&scissor_new, &ar->winrct, &scissor_new);
	glScissor(scissor_new.xmin, scissor_new.ymin, scissor_new.xmax-scissor_new.xmin, scissor_new.ymax-scissor_new.ymin);
	
	/* calculate offset and zoom */
	zoomx= (rect->xmax-rect->xmin-2.0f*but->aspect)/(cumap->curr.xmax - cumap->curr.xmin);
	zoomy= (rect->ymax-rect->ymin-2.0f*but->aspect)/(cumap->curr.ymax - cumap->curr.ymin);
	offsx= cumap->curr.xmin-but->aspect/zoomx;
	offsy= cumap->curr.ymin-but->aspect/zoomy;
	
	/* backdrop */
	if(cumap->flag & CUMA_DO_CLIP) {
		glColor3ubvShade((unsigned char *)wcol->inner, -20);
		glRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
		glColor3ubv((unsigned char*)wcol->inner);
		glRectf(rect->xmin + zoomx*(cumap->clipr.xmin-offsx),
				rect->ymin + zoomy*(cumap->clipr.ymin-offsy),
				rect->xmin + zoomx*(cumap->clipr.xmax-offsx),
				rect->ymin + zoomy*(cumap->clipr.ymax-offsy));
	}
	else {
		glColor3ubv((unsigned char*)wcol->inner);
		glRectf(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
	}
		
	/* grid, every .25 step */
	glColor3ubvShade((unsigned char *)wcol->inner, -16);
	ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 0.25f);
	/* grid, every 1.0 step */
	glColor3ubvShade((unsigned char *)wcol->inner, -24);
	ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 1.0f);
	/* axes */
	glColor3ubvShade((unsigned char *)wcol->inner, -50);
	glBegin(GL_LINES);
	glVertex2f(rect->xmin, rect->ymin + zoomy*(-offsy));
	glVertex2f(rect->xmax, rect->ymin + zoomy*(-offsy));
	glVertex2f(rect->xmin + zoomx*(-offsx), rect->ymin);
	glVertex2f(rect->xmin + zoomx*(-offsx), rect->ymax);
	glEnd();
	
	/* magic trigger for curve backgrounds */
	if (but->a1 != -1) {
		if (but->a1 == UI_GRAD_H) {
			rcti grid;
			float col[3]= {0.0f, 0.0f, 0.0f}; /* dummy arg */
			
			grid.xmin = rect->xmin + zoomx*(-offsx);
			grid.xmax = rect->xmax + zoomx*(-offsx);
			grid.ymin = rect->ymin + zoomy*(-offsy);
			grid.ymax = rect->ymax + zoomy*(-offsy);
			
			glEnable(GL_BLEND);
			ui_draw_gradient(&grid, col, UI_GRAD_H, 0.5f);
			glDisable(GL_BLEND);
		}
	}
	
	
	/* cfra option */
	/* XXX 2.48
	if(cumap->flag & CUMA_DRAW_CFRA) {
		glColor3ub(0x60, 0xc0, 0x40);
		glBegin(GL_LINES);
		glVertex2f(rect->xmin + zoomx*(cumap->sample[0]-offsx), rect->ymin);
		glVertex2f(rect->xmin + zoomx*(cumap->sample[0]-offsx), rect->ymax);
		glEnd();
	}*/
	/* sample option */
	/* XXX 2.48
	 * if(cumap->flag & CUMA_DRAW_SAMPLE) {
		if(cumap->cur==3) {
			float lum= cumap->sample[0]*0.35f + cumap->sample[1]*0.45f + cumap->sample[2]*0.2f;
			glColor3ub(240, 240, 240);
			
			glBegin(GL_LINES);
			glVertex2f(rect->xmin + zoomx*(lum-offsx), rect->ymin);
			glVertex2f(rect->xmin + zoomx*(lum-offsx), rect->ymax);
			glEnd();
		}
		else {
			if(cumap->cur==0)
				glColor3ub(240, 100, 100);
			else if(cumap->cur==1)
				glColor3ub(100, 240, 100);
			else
				glColor3ub(100, 100, 240);
			
			glBegin(GL_LINES);
			glVertex2f(rect->xmin + zoomx*(cumap->sample[cumap->cur]-offsx), rect->ymin);
			glVertex2f(rect->xmin + zoomx*(cumap->sample[cumap->cur]-offsx), rect->ymax);
			glEnd();
		}
	}*/
	
	/* the curve */
	glColor3ubv((unsigned char*)wcol->item);
	glEnable(GL_LINE_SMOOTH);
	glEnable(GL_BLEND);
	glBegin(GL_LINE_STRIP);
	
	if(cuma->table==NULL)
		curvemapping_changed(cumap, 0);	/* 0 = no remove doubles */
	cmp= cuma->table;
	
	/* first point */
	if((cuma->flag & CUMA_EXTEND_EXTRAPOLATE)==0)
		glVertex2f(rect->xmin, rect->ymin + zoomy*(cmp[0].y-offsy));
	else {
		fx= rect->xmin + zoomx*(cmp[0].x-offsx + cuma->ext_in[0]);
		fy= rect->ymin + zoomy*(cmp[0].y-offsy + cuma->ext_in[1]);
		glVertex2f(fx, fy);
	}
	for(a=0; a<=CM_TABLE; a++) {
		fx= rect->xmin + zoomx*(cmp[a].x-offsx);
		fy= rect->ymin + zoomy*(cmp[a].y-offsy);
		glVertex2f(fx, fy);
	}
	/* last point */
	if((cuma->flag & CUMA_EXTEND_EXTRAPOLATE)==0)
		glVertex2f(rect->xmax, rect->ymin + zoomy*(cmp[CM_TABLE].y-offsy));	
	else {
		fx= rect->xmin + zoomx*(cmp[CM_TABLE].x-offsx - cuma->ext_out[0]);
		fy= rect->ymin + zoomy*(cmp[CM_TABLE].y-offsy - cuma->ext_out[1]);
		glVertex2f(fx, fy);
	}
	glEnd();
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_BLEND);

	/* the points, use aspect to make them visible on edges */
	cmp= cuma->curve;
	glPointSize(3.0f);
	bglBegin(GL_POINTS);
	for(a=0; a<cuma->totpoint; a++) {
		if(cmp[a].flag & SELECT)
			UI_ThemeColor(TH_TEXT_HI);
		else
			UI_ThemeColor(TH_TEXT);
		fac[0]= rect->xmin + zoomx*(cmp[a].x-offsx);
		fac[1]= rect->ymin + zoomy*(cmp[a].y-offsy);
		bglVertex2fv(fac);
	}
	bglEnd();
	glPointSize(1.0f);
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);

	/* outline */
	glColor3ubv((unsigned char*)wcol->outline);
	fdrawbox(rect->xmin, rect->ymin, rect->xmax, rect->ymax);
}

static ImBuf *scale_trackpreview_ibuf(ImBuf *ibuf, float track_pos[2], int width, float height, int margin)
{
	ImBuf *scaleibuf;
	const float scalex= ((float)ibuf->x-2*margin) / width;
	const float scaley= ((float)ibuf->y-2*margin) / height;
	float off_x= (int)track_pos[0]-track_pos[0]+0.5f;
	float off_y= (int)track_pos[1]-track_pos[1]+0.5f;
	int x, y;

	scaleibuf= IMB_allocImBuf(width, height, 32, IB_rect);

	for(y= 0; y<height; y++) {
		for (x= 0; x<width; x++) {
			float src_x= scalex*(x)+margin-off_x;
			float src_y= scaley*(y)+margin-off_y;

			bicubic_interpolation(ibuf, scaleibuf, src_x, src_y, x, y);
		}
	}

	return scaleibuf;
}

void ui_draw_but_TRACKPREVIEW(ARegion *ar, uiBut *but, uiWidgetColors *UNUSED(wcol), rcti *recti)
{
	rctf rect;
	int ok= 0;
	GLint scissor[4];
	MovieClipScopes *scopes = (MovieClipScopes *)but->poin;

	rect.xmin = (float)recti->xmin+1;
	rect.xmax = (float)recti->xmax-1;
	rect.ymin = (float)recti->ymin+SCOPE_RESIZE_PAD+2;
	rect.ymax = (float)recti->ymax-1;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

	/* need scissor test, preview image can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin-1), ar->winrct.ymin+(rect.ymin-1), (rect.xmax+1)-(rect.xmin-1), (rect.ymax+1)-(rect.ymin-1));

	if(scopes->track_disabled) {
		glColor4f(0.7f, 0.3f, 0.3f, 0.3f);
		uiSetRoundBox(15);
		uiDrawBox(GL_POLYGON, rect.xmin-1, rect.ymin, rect.xmax+1, rect.ymax+1, 3.0f);

		ok= 1;
	}
	else if(scopes->track_preview) {
		/* additional margin around image */
		/* NOTE: should be kept in sync with value from BKE_movieclip_update_scopes */
		const int margin= 3;
		float zoomx, zoomy, track_pos[2], off_x, off_y;
		int a, width, height;
		ImBuf *drawibuf;

		glPushMatrix();

		track_pos[0]= scopes->track_pos[0]-margin;
		track_pos[1]= scopes->track_pos[1]-margin;

		/* draw content of pattern area */
		glScissor(ar->winrct.xmin+rect.xmin, ar->winrct.ymin+rect.ymin, scissor[2], scissor[3]);

		width= rect.xmax-rect.xmin+1;
		height = rect.ymax-rect.ymin;

		if(width > 0 && height > 0) {
			zoomx= (float)width / (scopes->track_preview->x-2*margin);
			zoomy= (float)height / (scopes->track_preview->y-2*margin);

			off_x= ((int)track_pos[0]-track_pos[0]+0.5)*zoomx;
			off_y= ((int)track_pos[1]-track_pos[1]+0.5)*zoomy;

			drawibuf= scale_trackpreview_ibuf(scopes->track_preview, track_pos, width, height, margin);

			glaDrawPixelsSafe(rect.xmin, rect.ymin+1, drawibuf->x, drawibuf->y,
			                  drawibuf->x, GL_RGBA, GL_UNSIGNED_BYTE, drawibuf->rect);
			IMB_freeImBuf(drawibuf);

			/* draw cross for pizel position */
			glTranslatef(off_x+rect.xmin+track_pos[0]*zoomx, off_y+rect.ymin+track_pos[1]*zoomy, 0.f);
			glScissor(ar->winrct.xmin + rect.xmin, ar->winrct.ymin+rect.ymin, rect.xmax-rect.xmin, rect.ymax-rect.ymin);

			for(a= 0; a< 2; a++) {
				if(a==1) {
					glLineStipple(3, 0xaaaa);
					glEnable(GL_LINE_STIPPLE);
					UI_ThemeColor(TH_SEL_MARKER);
				}
				else {
					UI_ThemeColor(TH_MARKER_OUTLINE);
				}

				glBegin(GL_LINES);
					glVertex2f(-10.0f, 0.0f);
					glVertex2f(10.0f, 0.0f);
					glVertex2f(0.0f, -10.0f);
					glVertex2f(0.0f, 10.0f);
					glEnd();
			}
		}

		glDisable(GL_LINE_STIPPLE);
		glPopMatrix();

		ok= 1;
	}

	if(!ok) {
		glColor4f(0.f, 0.f, 0.f, 0.3f);
		uiSetRoundBox(15);
		uiDrawBox(GL_POLYGON, rect.xmin-1, rect.ymin, rect.xmax+1, rect.ymax+1, 3.0f);
	}

	/* outline, scale gripper */
	draw_scope_end(&rect, scissor);

	glDisable(GL_BLEND);
}

/* ****************************************************** */


static void ui_shadowbox(float minx, float miny, float maxx, float maxy, float shadsize, unsigned char alpha)
{
	glEnable(GL_BLEND);
	glShadeModel(GL_SMOOTH);
	
	/* right quad */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glVertex2f(maxx, maxy-0.3f*shadsize);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx+shadsize, maxy-0.75f*shadsize);
	glVertex2f(maxx+shadsize, miny);
	glEnd();
	
	/* corner shape */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx+shadsize, miny);
	glVertex2f(maxx+0.7f*shadsize, miny-0.7f*shadsize);
	glVertex2f(maxx, miny-shadsize);
	glEnd();
	
	/* bottom quad */		
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(minx+0.3f*shadsize, miny);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx, miny-shadsize);
	glVertex2f(minx+0.5f*shadsize, miny-shadsize);
	glEnd();
	
	glDisable(GL_BLEND);
	glShadeModel(GL_FLAT);
}

void uiDrawBoxShadow(unsigned char alpha, float minx, float miny, float maxx, float maxy)
{
	/* accumulated outline boxes to make shade not linear, is more pleasant */
	ui_shadowbox(minx, miny, maxx, maxy, 11.0, (20*alpha)>>8);
	ui_shadowbox(minx, miny, maxx, maxy, 7.0, (40*alpha)>>8);
	ui_shadowbox(minx, miny, maxx, maxy, 5.0, (80*alpha)>>8);
	
}


void ui_dropshadow(rctf *rct, float radius, float aspect, int UNUSED(select))
{
	int i;
	float rad;
	float a;
	char alpha= 2;
	
	glEnable(GL_BLEND);
	
	if(radius > (rct->ymax-rct->ymin-10.0f)/2.0f)
		rad= (rct->ymax-rct->ymin-10.0f)/2.0f;
	else
		rad= radius;

	i= 12;
#if 0
	if(select) {
		a= i*aspect; /* same as below */
	}
	else
#endif
	{
		a= i*aspect;
	}

	for(; i--; a-=aspect) {
		/* alpha ranges from 2 to 20 or so */
		glColor4ub(0, 0, 0, alpha);
		alpha+= 2;
		
		uiDrawBox(GL_POLYGON, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax-10.0f + a, rad+a);
	}
	
	/* outline emphasis */
	glEnable( GL_LINE_SMOOTH );
	glColor4ub(0, 0, 0, 100);
	uiDrawBox(GL_LINE_LOOP, rct->xmin-0.5f, rct->ymin-0.5f, rct->xmax+0.5f, rct->ymax+0.5f, radius+0.5f);
	glDisable( GL_LINE_SMOOTH );
	
	glDisable(GL_BLEND);
}

