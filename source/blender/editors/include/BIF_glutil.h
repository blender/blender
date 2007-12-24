/**
 * @file BIF_glutil.h
 * 
 * OpenGL drawing utility functions.
 * 
 * $Id: BIF_glutil.h 10207 2007-03-06 03:39:15Z halley $
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

#ifndef BIF_GLUTIL_H
#define BIF_GLUTIL_H

struct rcti;
struct rctf;

void sdrawXORline(int x0, int y0, int x1, int y1);
void sdrawXORline4(int nr, int x0, int y0, int x1, int y1);

void fdrawXORellipse(float xofs, float yofs, float hw, float hh);
void fdrawXORcirc(float xofs, float yofs, float rad);

	/**
	 * Draw an XOR'd line in the front buffer between
	 * the given points. 
	 *
	 * @attention This function also handles flushing the GL
	 * pipeline, which means it is inappropriate for drawing
	 * a large number of lines at once.
	 */
void glutil_draw_front_xor_line(int x0, int y0, int x1, int y1);

	/**
	 * Draw a lined (non-looping) arc with the given
	 * @a radius, starting at angle @a start and arcing 
	 * through @a angle. The arc is centered at the origin 
	 * and drawn in the XY plane.
	 * 
	 * @param start The initial angle (in radians).
	 * @param angle The length of the arc (in radians).
	 * @param radius The arc radius.
	 * @param nsegments The number of segments to use in drawing the arc.
	 */
void glutil_draw_lined_arc	(float start, float angle, float radius, int nsegments);

	/**
	 * Draw a filled arc with the given @a radius, 
	 * starting at angle @a start and arcing through 
	 * @a angle. The arc is centered at the origin 
	 * and drawn in the XY plane.
	 * 
	 * @param start The initial angle (in radians).
	 * @param angle The length of the arc (in radians).
	 * @param radius The arc radius.
	 * @param nsegments The number of segments to use in drawing the arc.
	 */
void glutil_draw_filled_arc	(float start, float angle, float radius, int nsegments);

	/**
	 * Routines an integer value as obtained by glGetIntegerv.
	 * The param must cause only one value to be gotten from GL.
	 */
int glaGetOneInteger		(int param);

	/**
	 * Routines a float value as obtained by glGetIntegerv.
	 * The param must cause only one value to be gotten from GL.
	 */
float glaGetOneFloat		(int param);

	/**
	 * Functions like glRasterPos2i, except ensures that the resulting
	 * raster position is valid. @a known_good_x and @a known_good_y
	 * should be coordinates of a point known to be within the current
	 * view frustum.
	 * @attention This routine should be used when the distance of @a x 
	 * and @y away from the known good point is small (ie. for small icons
	 * and for bitmap characters), when drawing large+zoomed images it is
	 * possible for overflow to occur, the glaDrawPixelsSafe routine should
	 * be used instead.
	 */
void glaRasterPosSafe2f		(float x, float y, float known_good_x, float known_good_y);

	/**
	 * Functions like a limited glDrawPixels, except ensures that 
	 * the image is displayed onscreen even if the @a x and @a y 
	 * coordinates for would be clipped. The routine respects the
	 * glPixelZoom values, pixel unpacking parameters are _not_ 
	 * respected.

	 * @attention This routine makes many assumptions: the rect data
	 * is expected to be in RGBA unsigned byte format, the coordinate
	 * (0.375, 0.375) is assumed to be within the view frustum, and the 
	 * modelview and projection matrices are assumed to define a 
	 * 1-to-1 mapping to screen space.
	 * @attention Furthmore, in the case of zoomed or unpixel aligned
	 * images extending outside the view frustum, but still within the 
	 * window, some portion of the image may be visible left and/or
	 * below of the given @a x and @a y coordinates. It is recommended
	 * to use the glScissor functionality if images are to be drawn
	 * with an inset view matrix.
	 */
void glaDrawPixelsSafe		(float x, float y, int img_w, int img_h, int row_w, int format, int type, void *rect);

	/**
	 * Functions like a limited glDrawPixels, but actually draws the
	 * image using textures, which can be tremendously faster on low-end
	 * cards, and also avoids problems with the raster position being
	 * clipped when offscreen. The routine respects the glPixelZoom values, 
	 * pixel unpacking parameters are _not_ respected.

	 * @attention This routine makes many assumptions: the rect data
	 * is expected to be in RGBA byte or float format, and the 
	 * modelview and projection matrices are assumed to define a 
	 * 1-to-1 mapping to screen space.
	 */

	/* only for float rects, converts to 32 bits and draws */
void glaDrawPixelsSafe_to32(float fx, float fy, int img_w, int img_h, int row_w, float *rectf);


void glaDrawPixelsTex		(float x, float y, int img_w, int img_h, int format, void *rect);

	/* 2D Drawing Assistance */

	/** Define a 2D area (viewport, scissor, matrices) for OpenGL rendering.
	 * This routine sets up an OpenGL state appropriate for drawing using
	 * both vertice (glVertex, etc) and raster (glRasterPos, glRect) commands.
	 * All coordinates should be at integer positions. There is little to
	 * no reason to use glVertex2f etc. functions during 2D rendering, and
	 * thus no reason to +-0.5 the coordinates or perform other silly
	 * tricks.
	 *
	 * @param screen_rect The screen rectangle to be defined for 2D drawing.
	 */
void glaDefine2DArea		(struct rcti *screen_rect);

typedef struct gla2DDrawInfo gla2DDrawInfo;

	/** Save the current OpenGL state and initialize OpenGL for 2D
	 * rendering. glaEnd2DDraw should be called on the returned structure
	 * to free it and to return OpenGL to its previous state. The
	 * scissor rectangle is set to match the viewport.
	 *
	 * This routine sets up an OpenGL state appropriate for drawing using
	 * both vertice (glVertex, etc) and raster (glRasterPos, glRect) commands.
	 * All coordinates should be at integer positions. There is little to
	 * no reason to use glVertex2f etc. functions during 2D rendering, and
	 * thus no reason to +-0.5 the coordinates or perform other silly
	 * tricks.
	 *
	 * @param screen_rect The screen rectangle to be used for 2D drawing.
	 * @param world_rect The world rectangle that the 2D area represented
	 * by @a screen_rect is supposed to represent. If NULL it is assumed the
	 * world has a 1 to 1 mapping to the screen.
	 */
gla2DDrawInfo*	glaBegin2DDraw			(struct rcti *screen_rect, struct rctf *world_rect);

	/** Translate the (@a wo_x, @a wo_y) point from world coordinates into screen space. */
void			gla2DDrawTranslatePt	(gla2DDrawInfo *di, float wo_x, float wo_y, int *sc_x_r, int *sc_y_r);

	/** Translate the @a world point from world coordiantes into screen space. */
void			gla2DDrawTranslatePtv	(gla2DDrawInfo *di, float world[2], int screen_r[2]);

	/* Restores the previous OpenGL state and free's the auxilary
	 * gla data.
	 */
void			glaEnd2DDraw			(gla2DDrawInfo *di);

	/** Adjust the transformation mapping of a 2d area */
void gla2DGetMap(gla2DDrawInfo *di, struct rctf *rect);
void gla2DSetMap(gla2DDrawInfo *di, struct rctf *rect);


/* use this for platform hacks. glPointSize is solved here */
void bglBegin(int mode);
void bglEnd(void);
void bglVertex3fv(float *vec);
void bglVertex3f(float x, float y, float z);
void bglVertex2fv(float *vec);
/* intel gfx cards frontbuffer problem */
void bglFlush(void);
int is_a_really_crappy_intel_card(void);
void set_inverted_drawing(int enable);


/* own working polygon offset */
void bglPolygonOffset(float dist);

/* For caching opengl matrices (gluProject/gluUnProject) */
typedef struct bglMats {
	double modelview[16];
	double projection[16];
	int viewport[4];
} bglMats;
void bgl_get_mats(bglMats *mats);

#endif /* BIF_GLUTIL_H */

