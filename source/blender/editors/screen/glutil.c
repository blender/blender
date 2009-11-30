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

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"
#include "DNA_listBase.h"

#include "BKE_utildefines.h"
#include "BKE_colortools.h"

#include "BLI_math.h"
#include "BLI_threads.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE                        0x812F
#endif


/* ******************************************** */

/* defined in BIF_gl.h */
GLubyte stipple_halftone[128] = {
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55};


/*  repeat this pattern
	X000X000
	00000000
	00X000X0
	00000000 */


GLubyte stipple_quarttone[128] = { 
	136,136,136,136,0,0,0,0,34,34,34,34,0,0,0,0,
	136,136,136,136,0,0,0,0,34,34,34,34,0,0,0,0,
	136,136,136,136,0,0,0,0,34,34,34,34,0,0,0,0,
	136,136,136,136,0,0,0,0,34,34,34,34,0,0,0,0,
	136,136,136,136,0,0,0,0,34,34,34,34,0,0,0,0,
	136,136,136,136,0,0,0,0,34,34,34,34,0,0,0,0,
	136,136,136,136,0,0,0,0,34,34,34,34,0,0,0,0,
	136,136,136,136,0,0,0,0,34,34,34,34,0,0,0,0};


void fdrawbezier(float vec[4][3])
{
	float dist;
	float curve_res = 24, spline_step = 0.0f;
	
	dist= 0.5f*ABS(vec[0][0] - vec[3][0]);
	
	/* check direction later, for top sockets */
	vec[1][0]= vec[0][0]+dist;
	vec[1][1]= vec[0][1];
	
	vec[2][0]= vec[3][0]-dist;
	vec[2][1]= vec[3][1];
	/* we can reuse the dist variable here to increment the GL curve eval amount*/
	dist = 1.0f/curve_res;
	
	cpack(0x0);
	glMap1f(GL_MAP1_VERTEX_3, 0.0, 1.0, 3, 4, vec[0]);
	glBegin(GL_LINE_STRIP);
	while (spline_step < 1.000001f) {
		/*if(do_shaded)
			UI_ThemeColorBlend(th_col1, th_col2, spline_step);*/
		glEvalCoord1f(spline_step);
		spline_step += dist;
	}
	glEnd();
}

void fdrawline(float x1, float y1, float x2, float y2)
{
	float v[2];
	
	glBegin(GL_LINE_STRIP);
	v[0] = x1; v[1] = y1;
	glVertex2fv(v);
	v[0] = x2; v[1] = y2;
	glVertex2fv(v);
	glEnd();
}

void fdrawbox(float x1, float y1, float x2, float y2)
{
	float v[2];
	
	glBegin(GL_LINE_STRIP);
	
	v[0] = x1; v[1] = y1;
	glVertex2fv(v);
	v[0] = x1; v[1] = y2;
	glVertex2fv(v);
	v[0] = x2; v[1] = y2;
	glVertex2fv(v);
	v[0] = x2; v[1] = y1;
	glVertex2fv(v);
	v[0] = x1; v[1] = y1;
	glVertex2fv(v);
	
	glEnd();
}

void sdrawline(short x1, short y1, short x2, short y2)
{
	short v[2];
	
	glBegin(GL_LINE_STRIP);
	v[0] = x1; v[1] = y1;
	glVertex2sv(v);
	v[0] = x2; v[1] = y2;
	glVertex2sv(v);
	glEnd();
}

/*

	x1,y2
	|  \
	|   \
	|    \
	x1,y1-- x2,y1

*/

static void sdrawtripoints(short x1, short y1, short x2, short y2)
{
	short v[2];
	v[0]= x1; v[1]= y1;
	glVertex2sv(v);
	v[0]= x1; v[1]= y2;
	glVertex2sv(v);
	v[0]= x2; v[1]= y1;
	glVertex2sv(v);
}

void sdrawtri(short x1, short y1, short x2, short y2)
{
	glBegin(GL_LINE_STRIP);
	sdrawtripoints(x1, y1, x2, y2);
	glEnd();
}

void sdrawtrifill(short x1, short y1, short x2, short y2)
{
	glBegin(GL_TRIANGLES);
	sdrawtripoints(x1, y1, x2, y2);
	glEnd();
}

void sdrawbox(short x1, short y1, short x2, short y2)
{
	short v[2];
	
	glBegin(GL_LINE_STRIP);
	
	v[0] = x1; v[1] = y1;
	glVertex2sv(v);
	v[0] = x1; v[1] = y2;
	glVertex2sv(v);
	v[0] = x2; v[1] = y2;
	glVertex2sv(v);
	v[0] = x2; v[1] = y1;
	glVertex2sv(v);
	v[0] = x1; v[1] = y1;
	glVertex2sv(v);
	
	glEnd();
}


/* ******************************************** */

void setlinestyle(int nr)
{
	if(nr==0) {
		glDisable(GL_LINE_STIPPLE);
	}
	else {
		
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(nr, 0xAAAA);
	}
}

	/* Invert line handling */
	
#define glToggle(mode, onoff)	(((onoff)?glEnable:glDisable)(mode))

void set_inverted_drawing(int enable) 
{
	glLogicOp(enable?GL_INVERT:GL_COPY);

	/* Use GL_BLEND_EQUATION_EXT on sgi (if we have it),
	 * apparently GL_COLOR_LOGIC_OP doesn't work on O2?
	 * Is this an sgi bug or our bug?
	 */
#if defined(__sgi) && defined(GL_BLEND_EQUATION_EXT)
	glBlendEquationEXT(enable?GL_LOGIC_OP:GL_FUNC_ADD_EXT);
	glToggle(GL_BLEND, enable);
#else
	glToggle(GL_COLOR_LOGIC_OP, enable);
#endif

	glToggle(GL_DITHER, !enable);
}

void sdrawXORline(int x0, int y0, int x1, int y1)
{
	if(x0==x1 && y0==y1) return;

	set_inverted_drawing(1);
	
	glBegin(GL_LINES);
	glVertex2i(x0, y0);
	glVertex2i(x1, y1);
	glEnd();
	
	set_inverted_drawing(0);
}

void sdrawXORline4(int nr, int x0, int y0, int x1, int y1)
{
	static short old[4][2][2];
	static char flags[4]= {0, 0, 0, 0};
	
		/* with builtin memory, max 4 lines */

	set_inverted_drawing(1);
		
	glBegin(GL_LINES);
	if(nr== -1) { /* flush */
		for (nr=0; nr<4; nr++) {
			if (flags[nr]) {
				glVertex2sv(old[nr][0]);
				glVertex2sv(old[nr][1]);
				flags[nr]= 0;
			}
		}
	} else {
		if(nr>=0 && nr<4) {
			if(flags[nr]) {
				glVertex2sv(old[nr][0]);
				glVertex2sv(old[nr][1]);
			}

			old[nr][0][0]= x0;
			old[nr][0][1]= y0;
			old[nr][1][0]= x1;
			old[nr][1][1]= y1;
			
			flags[nr]= 1;
		}
		
		glVertex2i(x0, y0);
		glVertex2i(x1, y1);
	}
	glEnd();
	
	set_inverted_drawing(0);
}

void fdrawXORellipse(float xofs, float yofs, float hw, float hh)
{
	if(hw==0) return;

	set_inverted_drawing(1);

	glPushMatrix();
	glTranslatef(xofs, yofs, 0.0);
	glScalef(1,hh/hw,1);
	glutil_draw_lined_arc(0.0, M_PI*2.0, hw, 20);
	glPopMatrix();

	set_inverted_drawing(0);
}
void fdrawXORcirc(float xofs, float yofs, float rad)
{
	set_inverted_drawing(1);

	glPushMatrix();
	glTranslatef(xofs, yofs, 0.0);
	glutil_draw_lined_arc(0.0, M_PI*2.0, rad, 20);
	glPopMatrix();

	set_inverted_drawing(0);
}

void glutil_draw_filled_arc(float start, float angle, float radius, int nsegments) {
	int i;
	
	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(0.0, 0.0);
	for (i=0; i<nsegments; i++) {
		float t= (float) i/(nsegments-1);
		float cur= start + t*angle;
		
		glVertex2f(cos(cur)*radius, sin(cur)*radius);
	}
	glEnd();
}

void glutil_draw_lined_arc(float start, float angle, float radius, int nsegments) {
	int i;
	
	glBegin(GL_LINE_STRIP);
	for (i=0; i<nsegments; i++) {
		float t= (float) i/(nsegments-1);
		float cur= start + t*angle;
		
		glVertex2f(cos(cur)*radius, sin(cur)*radius);
	}
	glEnd();
}

int glaGetOneInteger(int param)
{
	GLint i;
	glGetIntegerv(param, &i);
	return i;
}

float glaGetOneFloat(int param)
{
	GLfloat v;
	glGetFloatv(param, &v);
	return v;
}

void glaRasterPosSafe2f(float x, float y, float known_good_x, float known_good_y)
{
	GLubyte dummy= 0;

		/* As long as known good coordinates are correct
		 * this is guarenteed to generate an ok raster
		 * position (ignoring potential (real) overflow
		 * issues).
		 */
	glRasterPos2f(known_good_x, known_good_y);

		/* Now shift the raster position to where we wanted
		 * it in the first place using the glBitmap trick.
		 */
	glBitmap(0, 0, 0, 0, x - known_good_x, y - known_good_y, &dummy);
}

static int get_cached_work_texture(int *w_r, int *h_r)
{
	static GLint texid= -1;
	static int tex_w= 256;
	static int tex_h= 256;

	if (texid==-1) {
		GLint ltexid= glaGetOneInteger(GL_TEXTURE_2D);
		unsigned char *tbuf;

		glGenTextures(1, (GLuint *)&texid);

		glBindTexture(GL_TEXTURE_2D, texid);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		tbuf= MEM_callocN(tex_w*tex_h*4, "tbuf");
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tbuf);
		MEM_freeN(tbuf);

		glBindTexture(GL_TEXTURE_2D, ltexid);
	}

	*w_r= tex_w;
	*h_r= tex_h;
	return texid;
}

void glaDrawPixelsTexScaled(float x, float y, int img_w, int img_h, int format, void *rect, float scaleX, float scaleY)
{
	unsigned char *uc_rect= (unsigned char*) rect;
	float *f_rect= (float *)rect;
	float xzoom= glaGetOneFloat(GL_ZOOM_X), yzoom= glaGetOneFloat(GL_ZOOM_Y);
	int ltexid= glaGetOneInteger(GL_TEXTURE_2D);
	int lrowlength= glaGetOneInteger(GL_UNPACK_ROW_LENGTH);
	int subpart_x, subpart_y, tex_w, tex_h;
	int texid= get_cached_work_texture(&tex_w, &tex_h);
	int nsubparts_x= (img_w+(tex_w-1))/tex_w;
	int nsubparts_y= (img_h+(tex_h-1))/tex_h;

	/* Specify the color outside this function, and tex will modulate it.
	 * This is useful for changing alpha without using glPixelTransferf()
	 */
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, img_w);
	glBindTexture(GL_TEXTURE_2D, texid);

	/* don't want nasty border artifacts */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#ifdef __APPLE__
	/* workaround for os x 10.5/10.6 driver bug: http://lists.apple.com/archives/Mac-opengl/2008/Jul/msg00117.html */
	glPixelZoom(1.f, 1.f);
#endif
	
	for (subpart_y=0; subpart_y<nsubparts_y; subpart_y++) {
		for (subpart_x=0; subpart_x<nsubparts_x; subpart_x++) {
			int subpart_w= (subpart_x==nsubparts_x-1)?(img_w-subpart_x*tex_w):tex_w;
			int subpart_h= (subpart_y==nsubparts_y-1)?(img_h-subpart_y*tex_h):tex_h;
			float rast_x= x+subpart_x*tex_w*xzoom;
			float rast_y= y+subpart_y*tex_h*yzoom;
			
			if(format==GL_FLOAT)
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, GL_RGBA, GL_FLOAT, &f_rect[(subpart_y*tex_w)*img_w*4 + (subpart_x*tex_w)*4]);
			else
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, GL_RGBA, GL_UNSIGNED_BYTE, &uc_rect[(subpart_y*tex_w)*img_w*4 + (subpart_x*tex_w)*4]);
							
			glEnable(GL_TEXTURE_2D);
			glBegin(GL_QUADS);
			glTexCoord2f(0, 0);
			glVertex2f(rast_x, rast_y);

			glTexCoord2f((float) (subpart_w-1)/tex_w, 0);
			glVertex2f(rast_x+subpart_w*xzoom*scaleX, rast_y);

			glTexCoord2f((float) (subpart_w-1)/tex_w, (float) (subpart_h-1)/tex_h);
			glVertex2f(rast_x+subpart_w*xzoom*scaleX, rast_y+subpart_h*yzoom*scaleY);

			glTexCoord2f(0, (float) (subpart_h-1)/tex_h);
			glVertex2f(rast_x, rast_y+subpart_h*yzoom*scaleY);
			glEnd();
			glDisable(GL_TEXTURE_2D);
		}
	}

	glBindTexture(GL_TEXTURE_2D, ltexid);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, lrowlength);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	
#ifdef __APPLE__
	/* workaround for os x 10.5/10.6 driver bug (above) */
	glPixelZoom(xzoom, yzoom);
#endif
}

void glaDrawPixelsTex(float x, float y, int img_w, int img_h, int format, void *rect)
{
	glaDrawPixelsTexScaled(x, y, img_w, img_h, format, rect, 1.0f, 1.0f);
}

void glaDrawPixelsSafe_to32(float fx, float fy, int img_w, int img_h, int row_w, float *rectf, int gamma_correct)
{
	unsigned char *rect32;
	
	/* copy imgw-imgh to a temporal 32 bits rect */
	if(img_w<1 || img_h<1) return;
	
	rect32= MEM_mallocN(img_w*img_h*sizeof(int), "temp 32 bits");
	
	if (gamma_correct) {
		floatbuf_to_srgb_byte(rectf, rect32, 0, img_w, 0, img_h, img_w);
	} else {
		floatbuf_to_byte(rectf, rect32, 0, img_w, 0, img_h, img_w);
 	}
	
	glaDrawPixelsSafe(fx, fy, img_w, img_h, img_w, GL_RGBA, GL_UNSIGNED_BYTE, rect32);

	MEM_freeN(rect32);
}

void glaDrawPixelsSafe(float x, float y, int img_w, int img_h, int row_w, int format, int type, void *rect)
{
	float xzoom= glaGetOneFloat(GL_ZOOM_X);
	float yzoom= glaGetOneFloat(GL_ZOOM_Y);
		
		/* The pixel space coordinate of the intersection of
		 * the [zoomed] image with the origin.
		 */
	float ix= -x/xzoom;
	float iy= -y/yzoom;
	
		/* The maximum pixel amounts the image can be cropped
		 * at the lower left without exceeding the origin.
		 */
	int off_x= floor(MAX2(ix, 0));
	int off_y= floor(MAX2(iy, 0));
		
		/* The zoomed space coordinate of the raster position 
		 * (starting at the lower left most unclipped pixel).
		 */
	float rast_x= x + off_x*xzoom;
	float rast_y= y + off_y*yzoom;

	GLfloat scissor[4];
	int draw_w, draw_h;

		/* Determine the smallest number of pixels we need to draw
		 * before the image would go off the upper right corner.
		 * 
		 * It may seem this is just an optimization but some graphics 
		 * cards (ATI) freak out if there is a large zoom factor and
		 * a large number of pixels off the screen (probably at some
		 * level the number of image pixels to draw is getting multiplied
		 * by the zoom and then clamped). Making sure we draw the
		 * fewest pixels possible keeps everyone mostly happy (still
		 * fails if we zoom in on one really huge pixel so that it
		 * covers the entire screen).
		 */
	glGetFloatv(GL_SCISSOR_BOX, scissor);
	draw_w = MIN2(img_w-off_x, ceil((scissor[2]-rast_x)/xzoom));
	draw_h = MIN2(img_h-off_y, ceil((scissor[3]-rast_y)/yzoom));

	if (draw_w>0 && draw_h>0) {
		int old_row_length = glaGetOneInteger(GL_UNPACK_ROW_LENGTH);

			/* Don't use safe RasterPos (slower) if we can avoid it. */
		if (rast_x>=0 && rast_y>=0) {
			glRasterPos2f(rast_x, rast_y);
		} else {
			glaRasterPosSafe2f(rast_x, rast_y, 0, 0);
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_w);
		if(format==GL_LUMINANCE || format==GL_RED) {
			if(type==GL_FLOAT) {
				float *f_rect= (float *)rect;
				glDrawPixels(draw_w, draw_h, format, type, f_rect + (off_y*row_w + off_x));
			}
			else if(type==GL_INT || type==GL_UNSIGNED_INT) {
				int *i_rect= (int *)rect;
				glDrawPixels(draw_w, draw_h, format, type, i_rect + (off_y*row_w + off_x));
			}
		}
		else { /* RGBA */
			if(type==GL_FLOAT) {
				float *f_rect= (float *)rect;
				glDrawPixels(draw_w, draw_h, format, type, f_rect + (off_y*row_w + off_x)*4);
			}
			else if(type==GL_UNSIGNED_BYTE) {
				unsigned char *uc_rect= (unsigned char *) rect;
				glDrawPixels(draw_w, draw_h, format, type, uc_rect + (off_y*row_w + off_x)*4);
			}
		}
		
		glPixelStorei(GL_UNPACK_ROW_LENGTH,  old_row_length);
	}
}

/* 2D Drawing Assistance */

void glaDefine2DArea(rcti *screen_rect)
{
	int sc_w= screen_rect->xmax - screen_rect->xmin + 1;
	int sc_h= screen_rect->ymax - screen_rect->ymin + 1;

	glViewport(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);
	glScissor(screen_rect->xmin, screen_rect->ymin, sc_w, sc_h);

		/* The 0.375 magic number is to shift the matrix so that
		 * both raster and vertex integer coordinates fall at pixel
		 * centers properly. For a longer discussion see the OpenGL
		 * Programming Guide, Appendix H, Correctness Tips.
		 */

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0.0, sc_w, 0.0, sc_h, -1, 1);
	glTranslatef(0.375, 0.375, 0.0);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

struct gla2DDrawInfo {
	int orig_vp[4], orig_sc[4];
	float orig_projmat[16], orig_viewmat[16];

	rcti screen_rect;
	rctf world_rect;

	float wo_to_sc[2];
};

void gla2DGetMap(gla2DDrawInfo *di, rctf *rect) 
{
	*rect= di->world_rect;
}

void gla2DSetMap(gla2DDrawInfo *di, rctf *rect) 
{
	int sc_w, sc_h;
	float wo_w, wo_h;

	di->world_rect= *rect;
	
	sc_w= (di->screen_rect.xmax-di->screen_rect.xmin);
	sc_h= (di->screen_rect.ymax-di->screen_rect.ymin);
	wo_w= (di->world_rect.xmax-di->world_rect.xmin);
	wo_h= (di->world_rect.ymax-di->world_rect.ymin);
	
	di->wo_to_sc[0]= sc_w/wo_w;
	di->wo_to_sc[1]= sc_h/wo_h;
}


gla2DDrawInfo *glaBegin2DDraw(rcti *screen_rect, rctf *world_rect) 
{
	gla2DDrawInfo *di= MEM_mallocN(sizeof(*di), "gla2DDrawInfo");
	int sc_w, sc_h;
	float wo_w, wo_h;

	glGetIntegerv(GL_VIEWPORT, (GLint *)di->orig_vp);
	glGetIntegerv(GL_SCISSOR_BOX, (GLint *)di->orig_sc);
	glGetFloatv(GL_PROJECTION_MATRIX, (GLfloat *)di->orig_projmat);
	glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat *)di->orig_viewmat);

	di->screen_rect= *screen_rect;
	if (world_rect) {
		di->world_rect= *world_rect;
	} else {
		di->world_rect.xmin= di->screen_rect.xmin;
		di->world_rect.ymin= di->screen_rect.ymin;
		di->world_rect.xmax= di->screen_rect.xmax;
		di->world_rect.ymax= di->screen_rect.ymax;
	}

	sc_w= (di->screen_rect.xmax-di->screen_rect.xmin);
	sc_h= (di->screen_rect.ymax-di->screen_rect.ymin);
	wo_w= (di->world_rect.xmax-di->world_rect.xmin);
	wo_h= (di->world_rect.ymax-di->world_rect.ymin);

	di->wo_to_sc[0]= sc_w/wo_w;
	di->wo_to_sc[1]= sc_h/wo_h;

	glaDefine2DArea(&di->screen_rect);

	return di;
}

void gla2DDrawTranslatePt(gla2DDrawInfo *di, float wo_x, float wo_y, int *sc_x_r, int *sc_y_r)
{
	*sc_x_r= (wo_x - di->world_rect.xmin)*di->wo_to_sc[0];
	*sc_y_r= (wo_y - di->world_rect.ymin)*di->wo_to_sc[1];
}
void gla2DDrawTranslatePtv(gla2DDrawInfo *di, float world[2], int screen_r[2])
{
	screen_r[0]= (world[0] - di->world_rect.xmin)*di->wo_to_sc[0];
	screen_r[1]= (world[1] - di->world_rect.ymin)*di->wo_to_sc[1];
}

void glaEnd2DDraw(gla2DDrawInfo *di)
{
	glViewport(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	glScissor(di->orig_vp[0], di->orig_vp[1], di->orig_vp[2], di->orig_vp[3]);
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(di->orig_projmat);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(di->orig_viewmat);

	MEM_freeN(di);
}

/* **************** glPoint hack ************************ */

static int curmode=0;
static int pointhack=0;
static GLubyte Squaredot[16] = { 0xff,0xff,0xff,0xff,
								 0xff,0xff,0xff,0xff,
								 0xff,0xff,0xff,0xff, 
								 0xff,0xff,0xff,0xff};

void bglBegin(int mode)
{
	curmode= mode;
	
	if(mode==GL_POINTS) {
		float value[4];
		glGetFloatv(GL_POINT_SIZE_RANGE, value);
		if(value[1]<2.0) {
			glGetFloatv(GL_POINT_SIZE, value);
			pointhack= floor(value[0]+0.5);
			if(pointhack>4) pointhack= 4;
		}
		else glBegin(mode);
	}
}

int bglPointHack() {
	float value[4];
	int pointhack;
	glGetFloatv(GL_POINT_SIZE_RANGE, value);
	if(value[1]<2.0) {
		glGetFloatv(GL_POINT_SIZE, value);
		pointhack= floor(value[0]+0.5);
		if(pointhack>4) pointhack= 4;
		return pointhack;
	}
	return 0;
}

void bglVertex3fv(float *vec)
{
	switch(curmode) {
	case GL_POINTS:
		if(pointhack) {
			glRasterPos3fv(vec);
			glBitmap(pointhack, pointhack, (float)pointhack/2.0, (float)pointhack/2.0, 0.0, 0.0, Squaredot);
		}
		else glVertex3fv(vec);
		break;
	}
}

void bglVertex3f(float x, float y, float z)
{
	switch(curmode) {
	case GL_POINTS:
		if(pointhack) {
			glRasterPos3f(x, y, z);
			glBitmap(pointhack, pointhack, (float)pointhack/2.0, (float)pointhack/2.0, 0.0, 0.0, Squaredot);
		}
		else glVertex3f(x, y, z);
		break;
	}
}

void bglVertex2fv(float *vec)
{
	switch(curmode) {
	case GL_POINTS:
		if(pointhack) {
			glRasterPos2fv(vec);
			glBitmap(pointhack, pointhack, (float)pointhack/2, pointhack/2, 0.0, 0.0, Squaredot);
		}
		else glVertex2fv(vec);
		break;
	}
}


void bglEnd(void)
{
	if(pointhack) pointhack= 0;
	else glEnd();
	
}

/* Uses current OpenGL state to get view matrices for gluProject/gluUnProject */
void bgl_get_mats(bglMats *mats)
{
	const double badvalue= 1.0e-6;

	glGetDoublev(GL_MODELVIEW_MATRIX, mats->modelview);
	glGetDoublev(GL_PROJECTION_MATRIX, mats->projection);
	glGetIntegerv(GL_VIEWPORT, (GLint *)mats->viewport);
	
	/* Very strange code here - it seems that certain bad values in the
	   modelview matrix can cause gluUnProject to give bad results. */
	if(mats->modelview[0] < badvalue &&
	   mats->modelview[0] > -badvalue)
		mats->modelview[0]= 0;
	if(mats->modelview[5] < badvalue &&
	   mats->modelview[5] > -badvalue)
		mats->modelview[5]= 0;
	
	/* Set up viewport so that gluUnProject will give correct values */
	mats->viewport[0] = 0;
	mats->viewport[1] = 0;
}

/* *************** glPolygonOffset hack ************* */

/* dist is only for ortho now... */
void bglPolygonOffset(float viewdist, float dist) 
{
	static float winmat[16], offset=0.0;	
	
	if(dist!=0.0) {
		float offs;
		
		// glEnable(GL_POLYGON_OFFSET_FILL);
		// glPolygonOffset(-1.0, -1.0);

		/* hack below is to mimic polygon offset */
		glMatrixMode(GL_PROJECTION);
		glGetFloatv(GL_PROJECTION_MATRIX, (float *)winmat);
		
		/* dist is from camera to center point */
		
		if(winmat[15]>0.5) offs= 0.00001*dist*viewdist;  // ortho tweaking
		else offs= 0.0005*dist;  // should be clipping value or so...
		
		winmat[14]-= offs;
		offset+= offs;
		
		glLoadMatrixf(winmat);
		glMatrixMode(GL_MODELVIEW);
	}
	else {

		glMatrixMode(GL_PROJECTION);
		winmat[14]+= offset;
		offset= 0.0;
		glLoadMatrixf(winmat);
		glMatrixMode(GL_MODELVIEW);
	}
}

int is_a_really_crappy_intel_card(void)
{
	static int well_is_it= -1;

		/* Do you understand the implication? Do you? */
	if (well_is_it==-1)
		well_is_it= (strcmp((char*) glGetString(GL_VENDOR), "Intel Inc.") == 0);

	return well_is_it;
}

void bglFlush(void) 
{
	glFlush();
#ifdef __APPLE__
//	if(is_a_really_crappy_intel_card())
// XXX		myswapbuffers(); //hack to get mac intel graphics to show frontbuffer
#endif
}

