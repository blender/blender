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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * os dependent include locations of gl.h
 */

/** \file BIF_gl.h
 *  \ingroup editorui
 */

#ifndef __BIF_GL_H__
#define __BIF_GL_H__

#include "GPU_glew.h"
#include "BLI_utildefines.h"

/*
 * these should be phased out. cpack should be replaced in
 * code with calls to glColor3ub. - zr
 */
/*
 *
 * This define converts a numerical value to the equivalent 24-bit
 * color, while not being endian-sensitive. On little-endians, this
 * is the same as doing a 'naive' indexing, on big-endian, it is not!
 * */
void cpack(unsigned int x);

#ifdef WITH_GL_PROFILE_COMPAT
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define glMultMatrixf(x)  \
	glMultMatrixf(_Generic((x), \
	        float *:      (float *)(x), \
	        float [16]:   (float *)(x), \
	        float (*)[4]: (float *)(x), \
	        float [4][4]: (float *)(x), \
	        const float *:      (float *)(x), \
	        const float [16]:   (float *)(x), \
	        const float (*)[4]: (float *)(x), \
	        const float [4][4]: (float *)(x)) \
)
#  define glLoadMatrixf(x)  \
	glLoadMatrixf(_Generic((x), \
	        float *:      (float *)(x), \
	        float [16]:   (float *)(x), \
	        float (*)[4]: (float *)(x), \
	        float [4][4]: (float *)(x)) \
)
#else
#  define glMultMatrixf(x)  glMultMatrixf((float *)(x))
#  define glLoadMatrixf(x)  glLoadMatrixf((float *)(x))
#endif  /* C11 */
#endif  /* WITH_GL_PROFILE_COMPAT */


/* hacking pointsize and linewidth */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define glPointSize(f)  glPointSize(U.pixelsize * _Generic((f), double: (float)(f), default: (f)))
#  define glLineWidth(f)  glLineWidth(U.pixelsize * _Generic((f), double: (float)(f), default: (f)))
#else
#  define glPointSize(f)  glPointSize(U.pixelsize * (f))
#  define glLineWidth(f)  glLineWidth(U.pixelsize * (f))
#endif  /* C11 */

#define GLA_PIXEL_OFS 0.375f


BLI_INLINE void glTranslate3iv(const int vec[3])    { glTranslatef(UNPACK3_EX((const float), vec, ));       }
BLI_INLINE void glTranslate2iv(const int vec[2])    { glTranslatef(UNPACK2_EX((const float), vec, ), 0.0f); }
BLI_INLINE void glTranslate3fv(const float vec[3])  { glTranslatef(UNPACK3(vec));       }
BLI_INLINE void glTranslate2fv(const float vec[2])  { glTranslatef(UNPACK2(vec), 0.0f); }

BLI_INLINE void glScale3iv(const int vec[3])    { glTranslatef(UNPACK3_EX((const float), vec, ));       }
BLI_INLINE void glScale2iv(const int vec[2])    { glTranslatef(UNPACK2_EX((const float), vec, ), 0.0f); }
BLI_INLINE void glScale3fv(const float vec[3])  { glScalef(UNPACK3(vec));      }
BLI_INLINE void glScale2fv(const float vec[2])  { glScalef(UNPACK2(vec), 0.0); }

/* v2 versions don't make much sense for rotation */
BLI_INLINE void glRotate3fv(const float angle, const float vec[3])   { glRotatef(angle, UNPACK3(vec)); }

#endif /* #ifdef __BIF_GL_H__ */
