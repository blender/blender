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
 *  This is to mark the transition to OpenGL core profile
 *  The idea is to allow Blender 2.8 to be built with OpenGL 3.3 even if it means breaking things
 *
 *  This file should be removed in the future
 */

#ifndef __GPU_LEGACY_STUBS_H__
#define __GPU_LEGACY_STUBS_H__

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

#include "BLI_utildefines.h"

#define _GL_VOID static inline void
#define _GL_VOID_RET {}

#define _GL_INT static inline GLint
#define _GL_INT_RET { return 0; }

static bool disable_enable_check(GLenum cap)
{
	return ELEM(cap,
	            GL_ALPHA_TEST,
	            GL_LINE_STIPPLE,
	            GL_POINT_SPRITE,
	            GL_TEXTURE_1D,
	            GL_TEXTURE_2D,
	            GL_TEXTURE_GEN_S,
	            GL_TEXTURE_GEN_T,
	            -1
	            );
}

static bool tex_env_check(GLenum target, GLenum pname)
{
	return (ELEM(target, GL_TEXTURE_ENV) ||
	        (target == GL_TEXTURE_FILTER_CONTROL && pname == GL_TEXTURE_LOD_BIAS));
}

#define glAlphaFunc oldAlphaFunc
_GL_VOID oldAlphaFunc (GLenum func, GLclampf ref) _GL_VOID_RET

#define glBegin oldBegin
_GL_VOID oldBegin (GLenum mode) _GL_VOID_RET

#define glBitmap oldBitmap
_GL_VOID oldBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap) _GL_VOID_RET

#define glClipPlane oldClipPlane
_GL_VOID oldClipPlane (GLenum plane, const GLdouble *equation) _GL_VOID_RET

#define glColor3f oldColor3f
_GL_VOID oldColor3f (GLfloat red, GLfloat green, GLfloat blue) _GL_VOID_RET

#define glColor3fv oldColor3fv
_GL_VOID oldColor3fv (const GLfloat *v) _GL_VOID_RET

#define glColor3ub oldColor3ub
_GL_VOID oldColor3ub (GLubyte red, GLubyte green, GLubyte blue) _GL_VOID_RET

#define glColor3ubv oldColor3ubv
_GL_VOID oldColor3ubv (const GLubyte *v) _GL_VOID_RET

#define glColor4f oldColor4f
_GL_VOID oldColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) _GL_VOID_RET

#define glColor4ub oldColor4ub
_GL_VOID oldColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha) _GL_VOID_RET

#define glColor4ubv oldColor4ubv
_GL_VOID oldColor4ubv (const GLubyte *v) _GL_VOID_RET

#define glColorPointer oldColorPointer
_GL_VOID oldColorPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

_GL_VOID oldDisable (GLenum cap)
{
	if (!disable_enable_check(cap)) {
		glDisable(cap);
	}
}
#define glDisable oldDisable

#define glDisableClientState oldDisableClientState
_GL_VOID oldDisableClientState (GLenum array) _GL_VOID_RET

_GL_VOID oldEnable (GLenum cap)
{
	if (!disable_enable_check(cap)) {
		glEnable(cap);
	}
}
#define glEnable oldEnable

#define glEnableClientState oldEnableClientState
_GL_VOID oldEnableClientState (GLenum array) _GL_VOID_RET

#define glEnd oldEnd
_GL_VOID oldEnd (void) _GL_VOID_RET

#define glInitNames oldInitNames
_GL_VOID oldInitNames (void) _GL_VOID_RET

#define glLightf oldLightf
_GL_VOID oldLightf (GLenum light, GLenum pname, GLfloat param) _GL_VOID_RET

#define glLightfv oldLightfv
_GL_VOID oldLightfv (GLenum light, GLenum pname, const GLfloat *params) _GL_VOID_RET

#define glLineStipple oldLineStipple
_GL_VOID oldLineStipple (GLint factor, GLushort pattern) _GL_VOID_RET

#define glLoadName oldLoadName
_GL_VOID oldLoadName (GLuint name) _GL_VOID_RET

#define glMaterialfv oldMaterialfv
_GL_VOID oldMaterialfv (GLenum face, GLenum pname, const GLfloat *params) _GL_VOID_RET

#define glMateriali oldMateriali
_GL_VOID oldMateriali (GLenum face, GLenum pname, GLint param) _GL_VOID_RET

#define glNormal3fv oldNormal3fv
_GL_VOID oldNormal3fv (const GLfloat *v) _GL_VOID_RET

#define glNormal3sv oldNormal3sv
_GL_VOID oldNormal3sv (const GLshort *v) _GL_VOID_RET

#define glNormalPointer oldNormalPointer
_GL_VOID oldNormalPointer (GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

#define glPopName oldPopName
_GL_VOID oldPopName (void) _GL_VOID_RET

#define glPushName oldPushName
_GL_VOID oldPushName (GLuint name) _GL_VOID_RET

#define glRasterPos2f oldRasterPos2f
_GL_VOID oldRasterPos2f (GLfloat x, GLfloat y) _GL_VOID_RET

#define glRenderMode oldRenderMode
_GL_INT oldRenderMode (GLenum mode) _GL_INT_RET

#define glSelectBuffer oldSelectBuffer
_GL_VOID oldSelectBuffer (GLsizei size, GLuint *buffer) _GL_VOID_RET

#define glShadeModel oldShadeModel
_GL_VOID oldShadeModel (GLenum mode) _GL_VOID_RET

#define glTexCoord2fv oldTexCoord2fv
_GL_VOID oldTexCoord2fv (const GLfloat *v) _GL_VOID_RET

_GL_VOID oldTexEnvf(GLenum target, GLenum pname, GLint param)
{
	if (!tex_env_check(target, pname)) {
		glTexEnvf(target, pname, param);
	}
}
#define glTexEnvf oldTexEnvf

_GL_VOID oldTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	if (!tex_env_check(target, pname)) {
		glTexEnvfv(target, pname, params);
	}
}
#define glTexEnvfv oldTexEnvfv

_GL_VOID oldTexEnvi(GLenum target, GLenum pname, GLint param)
{
	if (!tex_env_check(target, pname)) {
		glTexEnvi(target, pname, param);
	}
}
#define glTexEnvi oldTexEnvi

_GL_VOID oldTexGeni(GLenum coord, GLenum pname, GLint param)
{
	if (pname != GL_TEXTURE_GEN_MODE) {
		glTexGeni(coord, pname, param);
	}
}
#define glTexGeni oldTexGeni

#define glVertex2f oldVertex2f
_GL_VOID oldVertex2f (GLfloat x, GLfloat y) _GL_VOID_RET

#define glVertex3f oldVertex3f
_GL_VOID oldVertex3f (GLfloat x, GLfloat y, GLfloat z) _GL_VOID_RET

#define glTexCoord3fv oldTexCoord3fv
_GL_VOID oldTexCoord3fv (const GLfloat *v) _GL_VOID_RET

#define glTexCoordPointer oldTexCoordPointer
_GL_VOID oldTexCoordPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

#define glVertexPointer oldVertexPointer
_GL_VOID oldVertexPointer (GLint size, GLenum type, GLsizei stride, const void *pointer) _GL_VOID_RET

#define glVertex3fv oldVertex3fv
_GL_VOID oldVertex3fv (const GLfloat *v) _GL_VOID_RET


#undef _GL_VOID
#undef _GL_VOID_RET

#undef _GL_INT
#undef _GL_INT_RET

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

#endif /* __GPU_LEGACY_STUBS_H__ */
