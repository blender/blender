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
 * The Original Code is Copyright (C) 2017 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Blender Foundation, Dalai Felinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_legacy_stubs.h
 *  \ingroup gpu
 *
 * This is to mark the transition to OpenGL core profile
 * The idea is to allow Blender 2.8 to be built with OpenGL 3.3 even if it means breaking things
 *
 * This file should be removed in the future
 */

#ifndef __GPU_LEGACY_STUBS_H__
#define __GPU_LEGACY_STUBS_H__

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include <stdlib.h>  /* for abort(). */

#include "BLI_utildefines.h"

/**
 * Empty function, use for breakpoint when a depreacated
 * OpenGL function is called.
 */
static void gl_deprecated(void)
{
	BLI_assert(true);
}

#define _GL_BOOL BLI_INLINE GLboolean
#define _GL_BOOL_RET { \
	gl_deprecated();   \
	return false;      \
}

#define _GL_ENUM BLI_INLINE GLenum
#define _GL_ENUM_RET { \
	gl_deprecated();   \
	return 0;          \
}

#define _GL_INT BLI_INLINE GLint
#define _GL_INT_RET { \
	gl_deprecated();  \
	return 0;         \
}


#define _GL_UINT BLI_INLINE GLuint
#define _GL_UINT_RET { \
	gl_deprecated();   \
	return 0;          \
}

#define _GL_VOID BLI_INLINE void
#define _GL_VOID_RET { \
	gl_deprecated();   \
}

static bool disable_enable_check(GLenum cap)
{
	const bool is_deprecated = \
	        ELEM(
	            cap,
	            GL_ALPHA_TEST,
	            GL_LINE_STIPPLE,
	            GL_POINT_SPRITE,
	            GL_TEXTURE_1D,
	            GL_TEXTURE_2D,
	            GL_TEXTURE_GEN_S,
	            GL_TEXTURE_GEN_T,
	            -1
	            );

	if (is_deprecated) {
		gl_deprecated();
	}

	return is_deprecated;
}

_GL_VOID USE_CAREFULLY_glDisable (GLenum cap)
{
	if (!disable_enable_check(cap)) {
		glDisable(cap);
	}
}
#define glDisable USE_CAREFULLY_glDisable

_GL_VOID USE_CAREFULLY_glEnable (GLenum cap)
{
	if (!disable_enable_check(cap)) {
		glEnable(cap);
	}
}
#define glEnable USE_CAREFULLY_glEnable

/**
 * Hand written cases
 */

_GL_VOID DO_NOT_USE_glClientActiveTexture (GLenum texture) _GL_VOID_RET


/**
 * List automatically generated from `gl-deprecated.h` and `glew.h`
 */

/**
 * ENUM values
 */
#define DO_NOT_USE_GL_CURRENT_FOG_COORDINATE 0
#define DO_NOT_USE_GL_FOG_COORDINATE 0
#define DO_NOT_USE_GL_FOG_COORDINATE_ARRAY 0
#define DO_NOT_USE_GL_FOG_COORDINATE_ARRAY_BUFFER_BINDING 0
#define DO_NOT_USE_GL_FOG_COORDINATE_ARRAY_POINTER 0
#define DO_NOT_USE_GL_FOG_COORDINATE_ARRAY_STRIDE 0
#define DO_NOT_USE_GL_FOG_COORDINATE_ARRAY_TYPE 0
#define DO_NOT_USE_GL_FOG_COORDINATE_SOURCE 0
#define DO_NOT_USE_GL_POINT_SIZE_GRANULARITY 0
#define DO_NOT_USE_GL_POINT_SIZE_RANGE 0
#define DO_NOT_USE_GL_SOURCE0_ALPHA 0
#define DO_NOT_USE_GL_SOURCE0_RGB 0
#define DO_NOT_USE_GL_SOURCE1_ALPHA 0
#define DO_NOT_USE_GL_SOURCE1_RGB 0
#define DO_NOT_USE_GL_SOURCE2_ALPHA 0
#define DO_NOT_USE_GL_SOURCE2_RGB 0

/**
 * Functions
 */
_GL_VOID DO_NOT_USE_glAccum (GLenum op, GLfloat value) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glAlphaFunc (GLenum func, GLclampf ref) _GL_VOID_RET
_GL_BOOL DO_NOT_USE_glAreTexturesResident (GLsizei n, const GLuint *textures, GLboolean *residences) _GL_BOOL_RET
_GL_VOID DO_NOT_USE_glArrayElement (GLint i) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glBegin (GLenum mode) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glCallList (GLuint list) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glCallLists (GLsizei n, GLenum type, const void *lists) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glClearAccum (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glClearIndex (GLfloat c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glClipPlane (GLenum plane, const GLdouble *equation) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3b (GLbyte red, GLbyte green, GLbyte blue) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3bv (const GLbyte *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3d (GLdouble red, GLdouble green, GLdouble blue) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3f (GLfloat red, GLfloat green, GLfloat blue) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3i (GLint red, GLint green, GLint blue) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3s (GLshort red, GLshort green, GLshort blue) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3ub (GLubyte red, GLubyte green, GLubyte blue) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3ubv (const GLubyte *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3ui (GLuint red, GLuint green, GLuint blue) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3uiv (const GLuint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3us (GLushort red, GLushort green, GLushort blue) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor3usv (const GLushort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4b (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4bv (const GLbyte *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4d (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4i (GLint red, GLint green, GLint blue, GLint alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4s (GLshort red, GLshort green, GLshort blue, GLshort alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4ubv (const GLubyte *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4ui (GLuint red, GLuint green, GLuint blue, GLuint alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4uiv (const GLuint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4us (GLushort red, GLushort green, GLushort blue, GLushort alpha) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColor4usv (const GLushort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColorMaterial (GLenum face, GLenum mode) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glColorPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glCopyPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glDeleteLists (GLuint list, GLsizei range) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glDisableClientState (GLenum array) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEdgeFlag (GLboolean flag) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEdgeFlagPointer (GLsizei stride, const void *pointer) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEdgeFlagv (const GLboolean *flag) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEnableClientState (GLenum array) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEnd (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEndList (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalCoord1d (GLdouble u) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalCoord1dv (const GLdouble *u) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalCoord1f (GLfloat u) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalCoord1fv (const GLfloat *u) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalCoord2d (GLdouble u, GLdouble v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalCoord2dv (const GLdouble *u) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalCoord2f (GLfloat u, GLfloat v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalCoord2fv (const GLfloat *u) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalMesh1 (GLenum mode, GLint i1, GLint i2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalMesh2 (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalPoint1 (GLint i) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glEvalPoint2 (GLint i, GLint j) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glFeedbackBuffer (GLsizei size, GLenum type, GLfloat *buffer) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glFogf (GLenum pname, GLfloat param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glFogfv (GLenum pname, const GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glFogi (GLenum pname, GLint param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glFogiv (GLenum pname, const GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) _GL_VOID_RET
_GL_UINT DO_NOT_USE_glGenLists (GLsizei range) _GL_UINT_RET
_GL_VOID DO_NOT_USE_glGetClipPlane (GLenum plane, GLdouble *equation) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetLightfv (GLenum light, GLenum pname, GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetLightiv (GLenum light, GLenum pname, GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetMapdv (GLenum target, GLenum query, GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetMapfv (GLenum target, GLenum query, GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetMapiv (GLenum target, GLenum query, GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetMaterialfv (GLenum face, GLenum pname, GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetMaterialiv (GLenum face, GLenum pname, GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetPixelMapfv (GLenum map, GLfloat *values) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetPixelMapuiv (GLenum map, GLuint *values) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetPixelMapusv (GLenum map, GLushort *values) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetPolygonStipple (GLubyte *mask) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetTexEnvfv (GLenum target, GLenum pname, GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetTexEnviv (GLenum target, GLenum pname, GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetTexGendv (GLenum coord, GLenum pname, GLdouble *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetTexGenfv (GLenum coord, GLenum pname, GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glGetTexGeniv (GLenum coord, GLenum pname, GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexMask (GLuint mask) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexPointer (GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexd (GLdouble c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexdv (const GLdouble *c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexf (GLfloat c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexfv (const GLfloat *c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexi (GLint c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexiv (const GLint *c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexs (GLshort c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexsv (const GLshort *c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexub (GLubyte c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glIndexubv (const GLubyte *c) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glInitNames (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glInterleavedArrays (GLenum format, GLsizei stride, const void *pointer) _GL_VOID_RET
_GL_BOOL DO_NOT_USE_glIsList (GLuint list) _GL_BOOL_RET
_GL_VOID DO_NOT_USE_glLightModelf (GLenum pname, GLfloat param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLightModelfv (GLenum pname, const GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLightModeli (GLenum pname, GLint param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLightModeliv (GLenum pname, const GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLightf (GLenum light, GLenum pname, GLfloat param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLightfv (GLenum light, GLenum pname, const GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLighti (GLenum light, GLenum pname, GLint param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLightiv (GLenum light, GLenum pname, const GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLineStipple (GLint factor, GLushort pattern) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glListBase (GLuint base) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLoadIdentity (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLoadMatrixd (const GLdouble *m) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLoadMatrixf (const GLfloat *m) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glLoadName (GLuint name) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMap1d (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMap1f (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMap2d (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMap2f (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMapGrid1d (GLint un, GLdouble u1, GLdouble u2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMapGrid1f (GLint un, GLfloat u1, GLfloat u2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMapGrid2d (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMapGrid2f (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMaterialf (GLenum face, GLenum pname, GLfloat param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMaterialfv (GLenum face, GLenum pname, const GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMateriali (GLenum face, GLenum pname, GLint param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMaterialiv (GLenum face, GLenum pname, const GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMatrixMode (GLenum mode) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMultMatrixd (const GLdouble *m) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glMultMatrixf (const GLfloat *m) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNewList (GLuint list, GLenum mode) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3b (GLbyte nx, GLbyte ny, GLbyte nz) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3bv (const GLbyte *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3d (GLdouble nx, GLdouble ny, GLdouble nz) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3i (GLint nx, GLint ny, GLint nz) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3s (GLshort nx, GLshort ny, GLshort nz) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormal3sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glNormalPointer (GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPassThrough (GLfloat token) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPixelMapfv (GLenum map, GLsizei mapsize, const GLfloat *values) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPixelMapuiv (GLenum map, GLsizei mapsize, const GLuint *values) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPixelMapusv (GLenum map, GLsizei mapsize, const GLushort *values) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPixelTransferf (GLenum pname, GLfloat param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPixelTransferi (GLenum pname, GLint param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPixelZoom (GLfloat xfactor, GLfloat yfactor) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPolygonStipple (const GLubyte *mask) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPopAttrib (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPopClientAttrib (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPopMatrix (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPopName (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPrioritizeTextures (GLsizei n, const GLuint *textures, const GLclampf *priorities) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPushAttrib (GLbitfield mask) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPushClientAttrib (GLbitfield mask) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPushMatrix (void) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glPushName (GLuint name) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos2d (GLdouble x, GLdouble y) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos2dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos2f (GLfloat x, GLfloat y) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos2fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos2i (GLint x, GLint y) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos2iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos2s (GLshort x, GLshort y) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos2sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos3d (GLdouble x, GLdouble y, GLdouble z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos3dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos3f (GLfloat x, GLfloat y, GLfloat z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos3fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos3i (GLint x, GLint y, GLint z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos3iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos3s (GLshort x, GLshort y, GLshort z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos3sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos4dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos4fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos4i (GLint x, GLint y, GLint z, GLint w) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos4iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos4s (GLshort x, GLshort y, GLshort z, GLshort w) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRasterPos4sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRectd (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRectdv (const GLdouble *v1, const GLdouble *v2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRectf (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRectfv (const GLfloat *v1, const GLfloat *v2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRecti (GLint x1, GLint y1, GLint x2, GLint y2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRectiv (const GLint *v1, const GLint *v2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRects (GLshort x1, GLshort y1, GLshort x2, GLshort y2) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRectsv (const GLshort *v1, const GLshort *v2) _GL_VOID_RET
_GL_INT DO_NOT_USE_glRenderMode (GLenum mode) _GL_INT_RET
_GL_VOID DO_NOT_USE_glRotated (GLdouble angle, GLdouble x, GLdouble y, GLdouble z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glScaled (GLdouble x, GLdouble y, GLdouble z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glScalef (GLfloat x, GLfloat y, GLfloat z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glSelectBuffer (GLsizei size, GLuint *buffer) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glShadeModel (GLenum mode) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord1d (GLdouble s) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord1dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord1f (GLfloat s) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord1fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord1i (GLint s) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord1iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord1s (GLshort s) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord1sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord2d (GLdouble s, GLdouble t) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord2dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord2f (GLfloat s, GLfloat t) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord2fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord2i (GLint s, GLint t) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord2iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord2s (GLshort s, GLshort t) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord2sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord3d (GLdouble s, GLdouble t, GLdouble r) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord3dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord3f (GLfloat s, GLfloat t, GLfloat r) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord3fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord3i (GLint s, GLint t, GLint r) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord3iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord3s (GLshort s, GLshort t, GLshort r) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord3sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord4d (GLdouble s, GLdouble t, GLdouble r, GLdouble q) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord4dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord4f (GLfloat s, GLfloat t, GLfloat r, GLfloat q) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord4fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord4i (GLint s, GLint t, GLint r, GLint q) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord4iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord4s (GLshort s, GLshort t, GLshort r, GLshort q) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoord4sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexEnvf (GLenum target, GLenum pname, GLfloat param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexEnvfv (GLenum target, GLenum pname, const GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexEnvi (GLenum target, GLenum pname, GLint param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexEnviv (GLenum target, GLenum pname, const GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexGend (GLenum coord, GLenum pname, GLdouble param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexGendv (GLenum coord, GLenum pname, const GLdouble *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexGenf (GLenum coord, GLenum pname, GLfloat param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexGenfv (GLenum coord, GLenum pname, const GLfloat *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexGeni (GLenum coord, GLenum pname, GLint param) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTexGeniv (GLenum coord, GLenum pname, const GLint *params) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTranslated (GLdouble x, GLdouble y, GLdouble z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glTranslatef (GLfloat x, GLfloat y, GLfloat z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex2d (GLdouble x, GLdouble y) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex2dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex2f (GLfloat x, GLfloat y) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex2fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex2i (GLint x, GLint y) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex2iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex2s (GLshort x, GLshort y) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex2sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex3d (GLdouble x, GLdouble y, GLdouble z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex3dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex3f (GLfloat x, GLfloat y, GLfloat z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex3fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex3i (GLint x, GLint y, GLint z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex3iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex3s (GLshort x, GLshort y, GLshort z) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex3sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex4dv (const GLdouble *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex4fv (const GLfloat *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex4i (GLint x, GLint y, GLint z, GLint w) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex4iv (const GLint *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex4s (GLshort x, GLshort y, GLshort z, GLshort w) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertex4sv (const GLshort *v) _GL_VOID_RET
_GL_VOID DO_NOT_USE_glVertexPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

/**
 * End of automatically generated list
 */



#undef _GL_BOOL
#undef _GL_BOOL_RET
#undef _GL_ENUM
#undef _GL_ENUM_RET
#undef _GL_INT
#undef _GL_INT_RET
#undef _GL_UINT
#undef _GL_UINT_RET
#undef _GL_VOID
#undef _GL_VOID_RET

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#endif /* __GPU_LEGACY_STUBS_H__ */
