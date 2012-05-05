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

/** \file blender/editors/screen/glutil.c
 *  \ingroup edscr
 */


#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_vec_types.h"

#include "BLI_utildefines.h"

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
 *
 *     X000X000
 *     00000000
 *     00X000X0
 *     00000000 */


GLubyte stipple_quarttone[128] = { 
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0};


GLubyte stipple_diag_stripes_pos[128] = {
    0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f};


GLubyte stipple_diag_stripes_neg[128] = {
    0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80};


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
#if 0
		if (do_shaded)
			UI_ThemeColorBlend(th_col1, th_col2, spline_step);
#endif
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

void fdrawcheckerboard(float x1, float y1, float x2, float y2)
{
	unsigned char col1[4]= {40, 40, 40}, col2[4]= {50, 50, 50};

	GLubyte checker_stipple[32 * 32 / 8] = {
		255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
		255,  0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
		0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
		0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
		255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
		255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
		0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
		0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255};
	
	glColor3ubv(col1);
	glRectf(x1, y1, x2, y2);
	glColor3ubv(col2);

	glEnable(GL_POLYGON_STIPPLE);
	glPolygonStipple(checker_stipple);
	glRectf(x1, y1, x2, y2);
	glDisable(GL_POLYGON_STIPPLE);
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
 *     x1,y2
 *     |  \
 *     |   \
 *     |    \
 *     x1,y1-- x2,y1
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
	if (nr==0) {
		glDisable(GL_LINE_STIPPLE);
	}
	else {
		
		glEnable(GL_LINE_STIPPLE);
		glLineStipple(nr, 0xAAAA);
	}
}

	/* Invert line handling */
	
#define gl_toggle(mode, onoff)	(((onoff)?glEnable:glDisable)(mode))

void set_inverted_drawing(int enable) 
{
	glLogicOp(enable?GL_INVERT:GL_COPY);
	gl_toggle(GL_COLOR_LOGIC_OP, enable);
	gl_toggle(GL_DITHER, !enable);
}

void sdrawXORline(int x0, int y0, int x1, int y1)
{
	if (x0==x1 && y0==y1) return;

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
	if (nr== -1) { /* flush */
		for (nr=0; nr<4; nr++) {
			if (flags[nr]) {
				glVertex2sv(old[nr][0]);
				glVertex2sv(old[nr][1]);
				flags[nr]= 0;
			}
		}
	}
	else {
		if (nr>=0 && nr<4) {
			if (flags[nr]) {
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
	if (hw==0) return;

	set_inverted_drawing(1);

	glPushMatrix();
	glTranslatef(xofs, yofs, 0.0f);
	glScalef(1.0f, hh / hw, 1.0f);
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

void glutil_draw_filled_arc(float start, float angle, float radius, int nsegments)
{
	int i;
	
	glBegin(GL_TRIANGLE_FAN);
	glVertex2f(0.0, 0.0);
	for (i=0; i<nsegments; i++) {
		float t= (float) i/(nsegments-1);
		float cur= start + t*angle;
		
		glVertex2f(cosf(cur)*radius, sinf(cur)*radius);
	}
	glEnd();
}

void glutil_draw_lined_arc(float start, float angle, float radius, int nsegments)
{
	int i;
	
	glBegin(GL_LINE_STRIP);
	for (i=0; i<nsegments; i++) {
		float t= (float) i/(nsegments-1);
		float cur= start + t*angle;
		
		glVertex2f(cosf(cur)*radius, sinf(cur)*radius);
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
		 * this is guaranteed to generate an ok raster
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
	int seamless, offset_x, offset_y, nsubparts_x, nsubparts_y;
	int texid= get_cached_work_texture(&tex_w, &tex_h);
	
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
	
	/* setup seamless 2=on, 0=off */
	seamless= ((tex_w<img_w || tex_h<img_h) && tex_w>2 && tex_h>2)? 2: 0;
	
	offset_x= tex_w - seamless;
	offset_y= tex_h - seamless;
	
	nsubparts_x= (img_w + (offset_x - 1))/(offset_x);
	nsubparts_y= (img_h + (offset_y - 1))/(offset_y);

	for (subpart_y=0; subpart_y<nsubparts_y; subpart_y++) {
		for (subpart_x=0; subpart_x<nsubparts_x; subpart_x++) {
			int remainder_x= img_w-subpart_x*offset_x;
			int remainder_y= img_h-subpart_y*offset_y;
			int subpart_w= (remainder_x<tex_w)? remainder_x: tex_w;
			int subpart_h= (remainder_y<tex_h)? remainder_y: tex_h;
			int offset_left= (seamless && subpart_x!=0)? 1: 0;
			int offset_bot= (seamless && subpart_y!=0)? 1: 0;
			int offset_right= (seamless && remainder_x>tex_w)? 1: 0;
			int offset_top= (seamless && remainder_y>tex_h)? 1: 0;
			float rast_x= x+subpart_x*offset_x*xzoom;
			float rast_y= y+subpart_y*offset_y*yzoom;
			
			/* check if we already got these because we always get 2 more when doing seamless*/
			if (subpart_w<=seamless || subpart_h<=seamless)
				continue;
			
			if (format==GL_FLOAT) {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, GL_RGBA, GL_FLOAT, &f_rect[subpart_y*offset_y*img_w*4 + subpart_x*offset_x*4]);
				
				/* add an extra border of pixels so linear looks ok at edges of full image. */
				if (subpart_w<tex_w)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, 0, 1, subpart_h, GL_RGBA, GL_FLOAT, &f_rect[subpart_y*offset_y*img_w*4 + (subpart_x*offset_x+subpart_w-1)*4]);
				if (subpart_h<tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, subpart_h, subpart_w, 1, GL_RGBA, GL_FLOAT, &f_rect[(subpart_y*offset_y+subpart_h-1)*img_w*4 + subpart_x*offset_x*4]);
				if (subpart_w<tex_w && subpart_h<tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, subpart_h, 1, 1, GL_RGBA, GL_FLOAT, &f_rect[(subpart_y*offset_y+subpart_h-1)*img_w*4 + (subpart_x*offset_x+subpart_w-1)*4]);
			}
			else {
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, subpart_w, subpart_h, GL_RGBA, GL_UNSIGNED_BYTE, &uc_rect[subpart_y*offset_y*img_w*4 + subpart_x*offset_x*4]);
				
				if (subpart_w<tex_w)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, 0, 1, subpart_h, GL_RGBA, GL_UNSIGNED_BYTE, &uc_rect[subpart_y*offset_y*img_w*4 + (subpart_x*offset_x+subpart_w-1)*4]);
				if (subpart_h<tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, subpart_h, subpart_w, 1, GL_RGBA, GL_UNSIGNED_BYTE, &uc_rect[(subpart_y*offset_y+subpart_h-1)*img_w*4 + subpart_x*offset_x*4]);
				if (subpart_w<tex_w && subpart_h<tex_h)
					glTexSubImage2D(GL_TEXTURE_2D, 0, subpart_w, subpart_h, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &uc_rect[(subpart_y*offset_y+subpart_h-1)*img_w*4 + (subpart_x*offset_x+subpart_w-1)*4]);
			}

			glEnable(GL_TEXTURE_2D);
			glBegin(GL_QUADS);
			glTexCoord2f((float)(0 + offset_left)/tex_w, (float)(0 + offset_bot)/tex_h);
			glVertex2f(rast_x + (float)offset_left*xzoom, rast_y + (float)offset_bot*xzoom);

			glTexCoord2f((float)(subpart_w - offset_right)/tex_w, (float)(0 + offset_bot)/tex_h);
			glVertex2f(rast_x + (float)(subpart_w - offset_right)*xzoom*scaleX, rast_y + (float)offset_bot*xzoom);

			glTexCoord2f((float)(subpart_w - offset_right)/tex_w, (float)(subpart_h - offset_top)/tex_h);
			glVertex2f(rast_x + (float)(subpart_w - offset_right)*xzoom*scaleX, rast_y + (float)(subpart_h - offset_top)*yzoom*scaleY);

			glTexCoord2f((float)(0 + offset_left)/tex_w, (float)(subpart_h - offset_top)/tex_h);
			glVertex2f(rast_x + (float)offset_left*xzoom, rast_y + (float)(subpart_h - offset_top)*yzoom*scaleY);
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
		}
		else {
			glaRasterPosSafe2f(rast_x, rast_y, 0, 0);
		}

		glPixelStorei(GL_UNPACK_ROW_LENGTH, row_w);
		if (format==GL_LUMINANCE || format==GL_RED) {
			if (type==GL_FLOAT) {
				float *f_rect= (float *)rect;
				glDrawPixels(draw_w, draw_h, format, type, f_rect + (off_y*row_w + off_x));
			}
			else if (type==GL_INT || type==GL_UNSIGNED_INT) {
				int *i_rect= (int *)rect;
				glDrawPixels(draw_w, draw_h, format, type, i_rect + (off_y*row_w + off_x));
			}
		}
		else { /* RGBA */
			if (type==GL_FLOAT) {
				float *f_rect= (float *)rect;
				glDrawPixels(draw_w, draw_h, format, type, f_rect + (off_y*row_w + off_x)*4);
			}
			else if (type==GL_UNSIGNED_BYTE) {
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

#if 0 /* UNUSED */

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
	}
	else {
		di->world_rect.xmin = di->screen_rect.xmin;
		di->world_rect.ymin = di->screen_rect.ymin;
		di->world_rect.xmax = di->screen_rect.xmax;
		di->world_rect.ymax = di->screen_rect.ymax;
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
#endif

/* **************** GL_POINT hack ************************ */

static int curmode=0;
static int pointhack=0;
static GLubyte Squaredot[16] = {0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff,
                                0xff, 0xff, 0xff, 0xff};

void bglBegin(int mode)
{
	curmode= mode;
	
	if (mode==GL_POINTS) {
		float value[4];
		glGetFloatv(GL_POINT_SIZE_RANGE, value);
		if (value[1] < 2.0f) {
			glGetFloatv(GL_POINT_SIZE, value);
			pointhack= floor(value[0] + 0.5f);
			if (pointhack>4) pointhack= 4;
		}
		else glBegin(mode);
	}
}

#if 0 /* UNUSED */
int bglPointHack(void)
{
	float value[4];
	int pointhack_px;
	glGetFloatv(GL_POINT_SIZE_RANGE, value);
	if (value[1] < 2.0f) {
		glGetFloatv(GL_POINT_SIZE, value);
		pointhack_px= floorf(value[0]+0.5f);
		if (pointhack_px>4) pointhack_px= 4;
		return pointhack_px;
	}
	return 0;
}
#endif

void bglVertex3fv(const float vec[3])
{
	switch (curmode) {
	case GL_POINTS:
		if (pointhack) {
			glRasterPos3fv(vec);
			glBitmap(pointhack, pointhack, (float)pointhack/2.0f, (float)pointhack/2.0f, 0.0, 0.0, Squaredot);
		}
		else glVertex3fv(vec);
		break;
	}
}

void bglVertex3f(float x, float y, float z)
{
	switch (curmode) {
	case GL_POINTS:
		if (pointhack) {
			glRasterPos3f(x, y, z);
			glBitmap(pointhack, pointhack, (float)pointhack/2.0f, (float)pointhack/2.0f, 0.0, 0.0, Squaredot);
		}
		else glVertex3f(x, y, z);
		break;
	}
}

void bglVertex2fv(const float vec[2])
{
	switch (curmode) {
	case GL_POINTS:
		if (pointhack) {
			glRasterPos2fv(vec);
			glBitmap(pointhack, pointhack, (float)pointhack/2, pointhack/2, 0.0, 0.0, Squaredot);
		}
		else glVertex2fv(vec);
		break;
	}
}


void bglEnd(void)
{
	if (pointhack) pointhack= 0;
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
	 * modelview matrix can cause gluUnProject to give bad results. */
	if (mats->modelview[0] < badvalue &&
	    mats->modelview[0] > -badvalue)
	{
		mats->modelview[0] = 0;
	}
	if (mats->modelview[5] < badvalue &&
	    mats->modelview[5] > -badvalue)
	{
		mats->modelview[5] = 0;
	}
	
	/* Set up viewport so that gluUnProject will give correct values */
	mats->viewport[0] = 0;
	mats->viewport[1] = 0;
}

/* *************** glPolygonOffset hack ************* */

/* dist is only for ortho now... */
void bglPolygonOffset(float viewdist, float dist) 
{
	static float winmat[16], offset=0.0;	
	
	if (dist != 0.0f) {
		float offs;
		
		// glEnable(GL_POLYGON_OFFSET_FILL);
		// glPolygonOffset(-1.0, -1.0);

		/* hack below is to mimic polygon offset */
		glMatrixMode(GL_PROJECTION);
		glGetFloatv(GL_PROJECTION_MATRIX, (float *)winmat);
		
		/* dist is from camera to center point */
		
		if (winmat[15]>0.5f) offs= 0.00001f*dist*viewdist;  // ortho tweaking
		else offs= 0.0005f*dist;  // should be clipping value or so...
		
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

#if 0 /* UNUSED */
void bglFlush(void) 
{
	glFlush();
#ifdef __APPLE__
//	if (GPU_type_matches(GPU_DEVICE_INTEL, GPU_OS_MAC, GPU_DRIVER_OFFICIAL))
// XXX		myswapbuffers(); //hack to get mac intel graphics to show frontbuffer
#endif
}
#endif
