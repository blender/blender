/*
 * zbuf_ext.h
 * external interface for zbuf.h
 *
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

#ifndef ZBUF_H
#define ZBUF_H

#ifdef __cplusplus
extern "C" { 
#endif

struct LampRen;
struct VlakRen;

/*-----------------------------------------------------------*/ 
/* Includes                                                  */
/*-----------------------------------------------------------*/ 

#include "zbuf_types.h"
#include "render_types.h"
#include "radio_types.h" /* for RadView */

/*-----------------------------------------------------------*/ 
/* Function                                                  */
/* (11 so far )                                              */
/*-----------------------------------------------------------*/ 
 
/**
 * Fill a 'rectangle' with a fixed value. The rectangle contains x by
 * y points. The rows are assumed to be contiguous in memory, and to
 * consist of uints. This function is used for initializing the z
 * buffer.
 * (why is x int and y uint? called in envmap, render, zbuf)
 * @param rect  Pointer to the data representing the rectangle.
 * @param x     The width of the rectangle
 * @param y     The height of the rectangle
 * @param val   The value used to fill the rectangle.
 */
void fillrect(unsigned int *rect, int x, unsigned int y, unsigned int val);

/**
 * Converts a world coordinate into a homogenous coordinate in view
 * coordinates. The transformation matrix is only allowed to have a
 * scaling and translation component.
 * Also called in: shadbuf.c render.c radfactors.c
 *                 initrender.c envmap.c editmesh.c
 * @param v1  [3 floats] the world coordinate
 * @param adr [4 floats] the homogenous view coordinate
 */
void projectvert(float *v1,float *adr);


/** 
 * Do a z buffer calculation pass for shadow calculations.
 * Also called in: shadbuf.c
 * Note: Uses globals.
 * @param lar lamp definition data
 */
void zbuffershad(struct LampRen *lar);

	/* to the external interface, temp, I hope... */
/**
 * Tests whether the first three coordinates should be clipped
 * wrt. the fourth component. Bits 1 and 2 test on x, 3 and 4 test on
 * y, 5 and 6 test on z:
 * xyz >  test => set first bit   (01),
 * xyz < -test => set second bit  (10),
 * xyz == test => reset both bits (00).
 * Note: functionality is duplicated from an internal function
 * Also called in: initrender.c, radfactors.c
 * @param  v [4 floats] a coordinate 
 * @return a vector of bitfields
 */
/*  int testclip(float *v); */


/* The following are only used in zbuf.c and render.c ---------------*/
/**
 * Fills the entire in the alpha DA buffer. (All of it!)
 * Note: Uses globals.
 * Also called in: render.c
 * @param y the line number to set
 */
void abufsetrow(int y);


/**
 * Calculate the z buffer for all faces (or edges when in wireframe
 * mode) presently visible. 
 * Note: Uses globals.
 * Also called in: render.c
 */
void zbufferall(void);


/**
 * Initialize accumulation buffers for alpha z buffering.
 * The buffers are global variables. Also resets Accu buffer 
 * y bounds.
 * <LI>
 * <IT> Acolrow : colour buffer for one line
 * <IT> Arectz  : distance buffer for one line, depth ABUFPART
 * <IT> APixbuf : pixel data buffer for one line, depth ABUFPART 
 * </LI>
 * Also called in: render.c (should migrate)
 * Note: Uses globals.
 */
void bgnaccumbuf(void);

/**
 * Discard accumulation buffers for alpha z buffering.
 * The buffers are global variables. The released buffers are Acolrow,
 * Arectz, APixBuf. 
 * Also called in: render.c  (should migrate)
 * Note: Uses globals.
 */
void endaccumbuf(void);

/**
 * Z face intersect?
 */
int vergzvlak(const void *x1, const void *x2);

/**
 * Clip and fill vertex into the z buffer. zbuffunc needs to be set
 * before entering, to assure that there is a buffer fill function
 * that can be called. Zvlnr must be set to the current valid face
 * index .
 * Note: uses globals
 * @param f1 [4 floats] vertex 1
 * @param f2 [4 floats] vertex 2
 * @param f3 [4 floats] vertex 3
 * @param c1 clip conditions?
 * @param c2 
 * @param c3
 */
void  zbufclip(float *f1, float *f2, float *f3, int c1, int c2, int c3);

/**
 * same, for edges
 */
void         zbufclipwire(struct VlakRen *vlr); 

#ifdef __cplusplus
}
#endif

#endif

