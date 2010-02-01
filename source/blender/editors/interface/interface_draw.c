/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#include "DNA_color_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "UI_interface.h"
#include "UI_interface_icons.h"

#include "interface_intern.h"

#define UI_RB_ALPHA 16
#define UI_DISABLED_ALPHA_OFFS	-160

static int roundboxtype= 15;

void uiSetRoundBox(int type)
{
	/* Not sure the roundbox function is the best place to change this
	 * if this is undone, its not that big a deal, only makes curves edges
	 * square for the  */
	roundboxtype= type;

	/* flags to set which corners will become rounded:

	1------2
	|      |
	8------4
	*/
	
}

int uiGetRoundBox(void)
{
	return roundboxtype;
}

void gl_round_box(int mode, float minx, float miny, float maxx, float maxy, float rad)
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
	if(roundboxtype & 4) {
		glVertex2f(maxx-rad, miny);
		for(a=0; a<7; a++) {
			glVertex2f(maxx-rad+vec[a][0], miny+vec[a][1]);
		}
		glVertex2f(maxx, miny+rad);
	}
	else glVertex2f(maxx, miny);
	
	/* corner right-top */
	if(roundboxtype & 2) {
		glVertex2f(maxx, maxy-rad);
		for(a=0; a<7; a++) {
			glVertex2f(maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		glVertex2f(maxx-rad, maxy);
	}
	else glVertex2f(maxx, maxy);
	
	/* corner left-top */
	if(roundboxtype & 1) {
		glVertex2f(minx+rad, maxy);
		for(a=0; a<7; a++) {
			glVertex2f(minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		glVertex2f(minx, maxy-rad);
	}
	else glVertex2f(minx, maxy);
	
	/* corner left-bottom */
	if(roundboxtype & 8) {
		glVertex2f(minx, miny+rad);
		for(a=0; a<7; a++) {
			glVertex2f(minx+vec[a][1], miny+rad-vec[a][0]);
		}
		glVertex2f(minx+rad, miny);
	}
	else glVertex2f(minx, miny);
	
	glEnd();
}

static void round_box_shade_col(float *col1, float *col2, float fac)
{
	float col[3];

	col[0]= (fac*col1[0] + (1.0-fac)*col2[0]);
	col[1]= (fac*col1[1] + (1.0-fac)*col2[1]);
	col[2]= (fac*col1[2] + (1.0-fac)*col2[2]);
	
	glColor3fv(col);
}


/* linear horizontal shade within button or in outline */
/* view2d scrollers use it */
void gl_round_box_shade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadetop, float shadedown)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	float div= maxy-miny;
	float coltop[3], coldown[3], color[4];
	int a;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}
	/* get current color, needs to be outside of glBegin/End */
	glGetFloatv(GL_CURRENT_COLOR, color);

	/* 'shade' defines strength of shading */	
	coltop[0]= color[0]+shadetop; if(coltop[0]>1.0) coltop[0]= 1.0;
	coltop[1]= color[1]+shadetop; if(coltop[1]>1.0) coltop[1]= 1.0;
	coltop[2]= color[2]+shadetop; if(coltop[2]>1.0) coltop[2]= 1.0;
	coldown[0]= color[0]+shadedown; if(coldown[0]<0.0) coldown[0]= 0.0;
	coldown[1]= color[1]+shadedown; if(coldown[1]<0.0) coldown[1]= 0.0;
	coldown[2]= color[2]+shadedown; if(coldown[2]<0.0) coldown[2]= 0.0;

	glShadeModel(GL_SMOOTH);
	glBegin(mode);

	/* start with corner right-bottom */
	if(roundboxtype & 4) {
		
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(maxx-rad, miny);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(coltop, coldown, vec[a][1]/div);
			glVertex2f(maxx-rad+vec[a][0], miny+vec[a][1]);
		}
		
		round_box_shade_col(coltop, coldown, rad/div);
		glVertex2f(maxx, miny+rad);
	}
	else {
		round_box_shade_col(coltop, coldown, 0.0);
		glVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if(roundboxtype & 2) {
		
		round_box_shade_col(coltop, coldown, (div-rad)/div);
		glVertex2f(maxx, maxy-rad);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(coltop, coldown, (div-rad+vec[a][1])/div);
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
	if(roundboxtype & 1) {
		
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(minx+rad, maxy);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(coltop, coldown, (div-vec[a][1])/div);
			glVertex2f(minx+rad-vec[a][0], maxy-vec[a][1]);
		}
		
		round_box_shade_col(coltop, coldown, (div-rad)/div);
		glVertex2f(minx, maxy-rad);
	}
	else {
		round_box_shade_col(coltop, coldown, 1.0);
		glVertex2f(minx, maxy);
	}
	
	/* corner left-bottom */
	if(roundboxtype & 8) {
		
		round_box_shade_col(coltop, coldown, rad/div);
		glVertex2f(minx, miny+rad);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(coltop, coldown, (rad-vec[a][1])/div);
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
void gl_round_box_vertical_shade(int mode, float minx, float miny, float maxx, float maxy, float rad, float shadeLeft, float shadeRight)
{
	float vec[7][2]= {{0.195, 0.02}, {0.383, 0.067}, {0.55, 0.169}, {0.707, 0.293},
	                  {0.831, 0.45}, {0.924, 0.617}, {0.98, 0.805}};
	float div= maxx-minx;
	float colLeft[3], colRight[3], color[4];
	int a;
	
	/* mult */
	for(a=0; a<7; a++) {
		vec[a][0]*= rad; vec[a][1]*= rad;
	}
	/* get current color, needs to be outside of glBegin/End */
	glGetFloatv(GL_CURRENT_COLOR, color);

	/* 'shade' defines strength of shading */	
	colLeft[0]= color[0]+shadeLeft; if(colLeft[0]>1.0) colLeft[0]= 1.0;
	colLeft[1]= color[1]+shadeLeft; if(colLeft[1]>1.0) colLeft[1]= 1.0;
	colLeft[2]= color[2]+shadeLeft; if(colLeft[2]>1.0) colLeft[2]= 1.0;
	colRight[0]= color[0]+shadeRight; if(colRight[0]<0.0) colRight[0]= 0.0;
	colRight[1]= color[1]+shadeRight; if(colRight[1]<0.0) colRight[1]= 0.0;
	colRight[2]= color[2]+shadeRight; if(colRight[2]<0.0) colRight[2]= 0.0;

	glShadeModel(GL_SMOOTH);
	glBegin(mode);

	/* start with corner right-bottom */
	if(roundboxtype & 4) {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx-rad, miny);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(colLeft, colRight, vec[a][0]/div);
			glVertex2f(maxx-rad+vec[a][0], miny+vec[a][1]);
		}
		
		round_box_shade_col(colLeft, colRight, rad/div);
		glVertex2f(maxx, miny+rad);
	}
	else {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, miny);
	}
	
	/* corner right-top */
	if(roundboxtype & 2) {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, maxy-rad);
		
		for(a=0; a<7; a++) {
			
			round_box_shade_col(colLeft, colRight, (div-rad-vec[a][0])/div);
			glVertex2f(maxx-vec[a][1], maxy-rad+vec[a][0]);
		}
		round_box_shade_col(colLeft, colRight, (div-rad)/div);
		glVertex2f(maxx-rad, maxy);
	}
	else {
		round_box_shade_col(colLeft, colRight, 0.0);
		glVertex2f(maxx, maxy);
	}
	
	/* corner left-top */
	if(roundboxtype & 1) {
		round_box_shade_col(colLeft, colRight, (div-rad)/div);
		glVertex2f(minx+rad, maxy);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(colLeft, colRight, (div-rad+vec[a][0])/div);
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
	if(roundboxtype & 8) {
		round_box_shade_col(colLeft, colRight, 1.0);
		glVertex2f(minx, miny+rad);
		
		for(a=0; a<7; a++) {
			round_box_shade_col(colLeft, colRight, (vec[a][0])/div);
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

	gl_round_box(GL_LINE_LOOP, minx, miny, maxx, maxy, rad);
   
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );
}

/* plain fake antialiased unfilled round rectangle */
void uiRoundRectFakeAA(float minx, float miny, float maxx, float maxy, float rad, float asp)
{
	float color[4], alpha;
	float raddiff;
	int i, passes=4;
	
	/* get the colour and divide up the alpha */
	glGetFloatv(GL_CURRENT_COLOR, color);
	alpha = 1; //color[3];
	color[3]= 0.5*alpha/(float)passes;
	glColor4fv(color);
	
	/* set the 'jitter amount' */
	raddiff = (1/(float)passes) * asp;
	
	glEnable( GL_BLEND );
	
	/* draw lots of lines on top of each other */
	for (i=passes; i>=(-passes); i--) {
		gl_round_box(GL_LINE_LOOP, minx, miny, maxx, maxy, rad+(i*raddiff));
	}
	
	glDisable( GL_BLEND );
	
	color[3] = alpha;
	glColor4fv(color);
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
	
	/* solid part */
	gl_round_box(GL_POLYGON, minx, miny, maxx, maxy, rad);
	
	/* set antialias line */
	glEnable( GL_LINE_SMOOTH );
	glEnable( GL_BLEND );
	
	gl_round_box(GL_LINE_LOOP, minx, miny, maxx, maxy, rad);
	
	glDisable( GL_BLEND );
	glDisable( GL_LINE_SMOOTH );
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

void ui_draw_but_IMAGE(ARegion *ar, uiBut *but, uiWidgetColors *wcol, rcti *rect)
{
	extern char datatoc_splash_png[];
	extern int datatoc_splash_png_size;
	ImBuf *ibuf;
	//GLint scissor[4];
	//int w, h;
	
	/* hardcoded to splash, loading and freeing every draw, eek! */
	ibuf= IMB_ibImageFromMemory((int *)datatoc_splash_png, datatoc_splash_png_size, IB_rect);

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
	
	IMB_freeImBuf(ibuf);
}

#if 0
#ifdef INTERNATIONAL
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
	
	/* <builtin> font in use. There are TTF <builtin> and non-TTF <builtin> fonts */
	if(!strcmp(G.selfont->name, "<builtin>"))
	{
		if(G.ui_international == TRUE)
		{
			charmax = 0xff;
		}
		else
		{
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

	/* Set the font, in case it is not <builtin> font */
	if(G.selfont && strcmp(G.selfont->name, "<builtin>"))
	{
		char tmpStr[256];

		// Is the font file packed, if so then use the packed file
		if(G.selfont->packedfile)
		{
			pf = G.selfont->packedfile;		
			FTF_SetFont(pf->data, pf->size, 14.0);
		}
		else
		{
			int err;

			strcpy(tmpStr, G.selfont->name);
			BLI_convertstringcode(tmpStr, G.sce);
			err = FTF_SetFont((unsigned char *)tmpStr, 0, 14.0);
		}
	}
	else
	{
		if(G.ui_international == TRUE)
		{
			FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, 14.0);
		}
	}

	/* Start drawing the button itself */
	glShadeModel(GL_SMOOTH);

	glColor3ub(200,  200,  200);
	glRectf((rect->xmin), (rect->ymin), (rect->xmax), (rect->ymax));

	glColor3ub(0,  0,  0);
	for(y = 0; y < 6; y++)
	{
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

			// Set the font to be either unicode or <builtin>				
			wstr[0] = cs;
			if(strcmp(G.selfont->name, "<builtin>"))
			{
				wcs2utf8s((char *)ustr, (wchar_t *)wstr);
			}
			else
			{
				if(G.ui_international == TRUE)
				{
					wcs2utf8s((char *)ustr, (wchar_t *)wstr);
				}
				else
				{
					ustr[0] = cs;
					ustr[1] = 0;
				}
			}

			if((G.selfont && strcmp(G.selfont->name, "<builtin>")) || (G.selfont && !strcmp(G.selfont->name, "<builtin>") && G.ui_international == TRUE))
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
	if(U.fontsize && U.fontname[0])
	{
		result = FTF_SetFont((unsigned char *)U.fontname, 0, U.fontsize);
	}
	else if (U.fontsize)
	{
		result = FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, U.fontsize);
	}

	if (result == 0)
	{
		result = FTF_SetFont((unsigned char *) datatoc_bfont_ttf, datatoc_bfont_ttf_size, 11);
	}
	
	/* resets the font size */
	if(G.ui_international == TRUE)
	{
		// uiSetCurFont(but->block, UI_HELV);
	}
}

#endif // INTERNATIONAL
#endif


void ui_draw_but_HISTOGRAM(ARegion *ar, uiBut *but, uiWidgetColors *wcol, rcti *recti)
{
	Histogram *hist = (Histogram *)but->poin;
	int res = hist->x_resolution;
	rctf rect;
	int i;
	int rgb;
	float w, h;
	float alpha;
	GLint scissor[4];
	
	if (hist==NULL) { printf("hist is null \n"); return; }
	
	rect.xmin = (float)recti->xmin;
	rect.xmax = (float)recti->xmax;
	rect.ymin = (float)recti->ymin;
	rect.ymax = (float)recti->ymax;
	
	w = rect.xmax - rect.xmin;
	h = rect.ymax - rect.ymin;
	h *= hist->ymax;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	
	glColor4f(0.f, 0.f, 0.f, 0.3f);
	uiSetRoundBox(15);
	gl_round_box(GL_POLYGON, rect.xmin-1, rect.ymin-1, rect.xmax+1, rect.ymax+1, 3.0f);
	
	glColor4f(1.f, 1.f, 1.f, 0.08f);
	/* draw grid lines here */
	for (i=1; i<4; i++) {
		fdrawline(rect.xmin, rect.ymin+(i/4.f)*h, rect.xmax, rect.ymin+(i/4.f)*h);
		fdrawline(rect.xmin+(i/4.f)*w, rect.ymin, rect.xmin+(i/4.f)*w, rect.ymax);
	}
	
	/* need scissor test, histogram can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + (rect.xmin-1), ar->winrct.ymin+(rect.ymin-1), (rect.xmax+1)-(rect.xmin-1), (rect.ymax+1)-(rect.ymin-1));
		
	for (rgb=0; rgb<3; rgb++) {
		float *data = NULL;
		
		if (rgb==0)			data = hist->data_r;
		else if (rgb==1)	data = hist->data_g;
		else if (rgb==2)	data = hist->data_b;
		
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		alpha = 0.75;
		if (rgb==0)			glColor4f(1.f, 0.f, 0.f, alpha);
		else if (rgb==1)	glColor4f(0.f, 1.f, 0.f, alpha);
		else if (rgb==2)	glColor4f(0.f, 0.f, 1.f, alpha);
		
		glShadeModel(GL_FLAT);
		glBegin(GL_QUAD_STRIP);
		glVertex2f(rect.xmin, rect.ymin);
		glVertex2f(rect.xmin, rect.ymin + (data[0]*h));
		for (i=1; i < res; i++) {
			float x = rect.xmin + i * (w/(float)res);
			glVertex2f(x, rect.ymin + (data[i]*h));
			glVertex2f(x, rect.ymin);
		}
		glEnd();
		
		glColor4f(0.f, 0.f, 0.f, 0.25f);
		
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LINE_SMOOTH);
		glBegin(GL_LINE_STRIP);
		for (i=0; i < res; i++) {
			float x = rect.xmin + i * (w/(float)res);
			glVertex2f(x, rect.ymin + (data[i]*h));
		}
		glEnd();
		glDisable(GL_LINE_SMOOTH);
	}
	
	/* restore scissortest */
	glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glColor4f(0.f, 0.f, 0.f, 0.5f);
	uiSetRoundBox(15);
	gl_round_box(GL_LINE_LOOP, rect.xmin-1, rect.ymin-1, rect.xmax+1, rect.ymax+1, 3.0f);
	
	glDisable(GL_BLEND);
}


void ui_draw_but_COLORBAND(uiBut *but, uiWidgetColors *wcol, rcti *rect)
{
	ColorBand *coba;
	CBData *cbd;
	float x1, y1, sizex, sizey;
	float dx, v3[2], v1[2], v2[2], v1a[2], v2a[2];
	int a;
	float pos, colf[4]= {0,0,0,0}; /* initialize incase the colorband isnt valid */
		
	coba= (ColorBand *)(but->editcoba? but->editcoba: but->poin);
	if(coba==NULL) return;
	
	x1= rect->xmin;
	y1= rect->ymin;
	sizex= rect->xmax-x1;
	sizey= rect->ymax-y1;
	
	/* first background, to show tranparency */
	dx= sizex/12.0;
	v1[0]= x1;
	for(a=0; a<12; a++) {
		if(a & 1) glColor3f(0.3, 0.3, 0.3); else glColor3f(0.8, 0.8, 0.8);
		glRectf(v1[0], y1, v1[0]+dx, y1+0.5*sizey);
		if(a & 1) glColor3f(0.8, 0.8, 0.8); else glColor3f(0.3, 0.3, 0.3);
		glRectf(v1[0], y1+0.5*sizey, v1[0]+dx, y1+sizey);
		v1[0]+= dx;
	}
	
	glShadeModel(GL_FLAT);
	glEnable(GL_BLEND);
	
	cbd= coba->data;
	
	v1[0]= v2[0]= x1;
	v1[1]= y1;
	v2[1]= y1+sizey;
	
	glBegin(GL_QUAD_STRIP);
	
	glColor4fv( &cbd->r );
	glVertex2fv(v1); glVertex2fv(v2);
	
	for( a = 1; a < sizex; a++ ) {
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
	v1[0]= x1; v1[1]= y1;
	
	cpack(0x0);
	glBegin(GL_LINE_LOOP);
	glVertex2fv(v1);
	v1[0]+= sizex;
	glVertex2fv(v1);
	v1[1]+= sizey;
	glVertex2fv(v1);
	v1[0]-= sizex;
	glVertex2fv(v1);
	glEnd();
	
	
	/* help lines */
	v1[0]= v2[0]=v3[0]= x1;
	v1[1]= y1;
	v1a[1]= y1+0.25*sizey;
	v2[1]= y1+0.5*sizey;
	v2a[1]= y1+0.75*sizey;
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
	uiSetRoundBox(15);
	gl_round_box(GL_POLYGON, rect->xmin, rect->ymin, rect->xmax, rect->ymax, 5.0f);
	
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
	if(fx > rect->xmin) fx -= dx*( floor(fx-rect->xmin));
	while(fx < rect->xmax) {
		glVertex2f(fx, rect->ymin); 
		glVertex2f(fx, rect->ymax);
		fx+= dx;
	}
	
	dy= step*zoomy;
	fy= rect->ymin + zoomy*(-offsy);
	if(fy > rect->ymin) fy -= dy*( floor(fy-rect->ymin));
	while(fy < rect->ymax) {
		glVertex2f(rect->xmin, fy); 
		glVertex2f(rect->xmax, fy);
		fy+= dy;
	}
	glEnd();
	
}

static void glColor3ubvShade(char *col, int shade)
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
	int a;

	cumap= (CurveMapping *)(but->editcumap? but->editcumap: but->poin);
	cuma= cumap->cm+cumap->cur;
	
	/* need scissor test, curve can draw outside of boundary */
	glGetIntegerv(GL_VIEWPORT, scissor);
	glScissor(ar->winrct.xmin + rect->xmin, ar->winrct.ymin+rect->ymin, rect->xmax-rect->xmin, rect->ymax-rect->ymin);
	
	/* calculate offset and zoom */
	zoomx= (rect->xmax-rect->xmin-2.0*but->aspect)/(cumap->curr.xmax - cumap->curr.xmin);
	zoomy= (rect->ymax-rect->ymin-2.0*but->aspect)/(cumap->curr.ymax - cumap->curr.ymin);
	offsx= cumap->curr.xmin-but->aspect/zoomx;
	offsy= cumap->curr.ymin-but->aspect/zoomy;
	
	/* backdrop */
	if(cumap->flag & CUMA_DO_CLIP) {
		glColor3ubvShade(wcol->inner, -20);
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
	glColor3ubvShade(wcol->inner, -16);
	ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 0.25f);
	/* grid, every 1.0 step */
	glColor3ubvShade(wcol->inner, -24);
	ui_draw_but_curve_grid(rect, zoomx, zoomy, offsx, offsy, 1.0f);
	/* axes */
	glColor3ubvShade(wcol->inner, -50);
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
			float col[3];
			
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


/* ****************************************************** */


static void ui_shadowbox(float minx, float miny, float maxx, float maxy, float shadsize, unsigned char alpha)
{
	glEnable(GL_BLEND);
	glShadeModel(GL_SMOOTH);
	
	/* right quad */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glVertex2f(maxx, maxy-0.3*shadsize);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx+shadsize, maxy-0.75*shadsize);
	glVertex2f(maxx+shadsize, miny);
	glEnd();
	
	/* corner shape */
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx+shadsize, miny);
	glVertex2f(maxx+0.7*shadsize, miny-0.7*shadsize);
	glVertex2f(maxx, miny-shadsize);
	glEnd();
	
	/* bottom quad */		
	glBegin(GL_POLYGON);
	glColor4ub(0, 0, 0, alpha);
	glVertex2f(minx+0.3*shadsize, miny);
	glVertex2f(maxx, miny);
	glColor4ub(0, 0, 0, 0);
	glVertex2f(maxx, miny-shadsize);
	glVertex2f(minx+0.5*shadsize, miny-shadsize);
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


void ui_dropshadow(rctf *rct, float radius, float aspect, int select)
{
	float rad;
	float a;
	char alpha= 2;
	
	glEnable(GL_BLEND);
	
	if(radius > (rct->ymax-rct->ymin-10.0f)/2.0f)
		rad= (rct->ymax-rct->ymin-10.0f)/2.0f;
	else
		rad= radius;
	
	if(select) a= 12.0f*aspect; else a= 12.0f*aspect;
	for(; a>0.0f; a-=aspect) {
		/* alpha ranges from 2 to 20 or so */
		glColor4ub(0, 0, 0, alpha);
		alpha+= 2;
		
		gl_round_box(GL_POLYGON, rct->xmin - a, rct->ymin - a, rct->xmax + a, rct->ymax-10.0f + a, rad+a);
	}
	
	/* outline emphasis */
	glEnable( GL_LINE_SMOOTH );
	glColor4ub(0, 0, 0, 100);
	gl_round_box(GL_LINE_LOOP, rct->xmin-0.5f, rct->ymin-0.5f, rct->xmax+0.5f, rct->ymax+0.5f, radius);
	glDisable( GL_LINE_SMOOTH );
	
	glDisable(GL_BLEND);
}

