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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file glew-mx.h
 *  \ingroup glew-mx
 *
 * Support for GLEW Multiple rendering conteXts (MX)
 * Maintained as a Blender Library.
 *
 * Different rendering contexts may have different entry points
 * to extension functions of the same name.  So it can cause
 * problems if, for example, a second context uses a pointer to
 * say, glActiveTextureARB, that was queried from the first context.
 *
 * GLEW has basic support for multiple contexts by enabling WITH_GLEW_MX,
 * but it does not provide a full implementation.  This is because
 * there are too many questions about thread safety and memory
 * allocation that are up to the user of GLEW.
 *
 * This implementation is very basic and isn't thread safe.
 * For a single context the overhead should be
 * no more than using GLEW without WITH_GLEW_MX enabled.
 */

#ifndef __GLEW_MX_H__
#define __GLEW_MX_H__

#ifdef WITH_GLEW_MX
/* glew itself expects this */
#  define GLEW_MX 1
#  define glewGetContext() (&(_mx_context->glew_context))
#endif

#include <GL/glew.h>


#ifdef __cplusplus
extern "C" {
#endif

/* MXContext is used instead of GLEWContext directly so that
   extending what data is held by a context is easier.
 */
typedef struct MXContext {
#ifdef WITH_GLEW_MX
	GLEWContext glew_context;
#endif

	int reserved; /* structs need at least one member */

} MXContext;

#ifdef WITH_GLEW_MX
extern MXContext *_mx_context;
#endif


#include "intern/symbol-binding.h"


/* If compiling only for OpenGL 3.2 Core Profile then we should make sure
 * no legacy API entries or symbolic constants are used.
 */
#if defined(WITH_GL_PROFILE_CORE) && !defined(WITH_GL_PROFILE_COMPAT) && !defined(WITH_GL_PROFILE_ES20)
#  include "intern/gl-deprecated.h"
#endif


MXContext *mxCreateContext     (void);
MXContext *mxGetCurrentContext (void);
void       mxMakeCurrentContext(MXContext *ctx);
void       mxDestroyContext    (MXContext *ctx);


GLenum glew_chk(GLenum error, const char *file, int line, const char *text);

#ifndef NDEBUG
#  define GLEW_CHK(x) glew_chk((x), __FILE__, __LINE__, #x)
#else
#  define GLEW_CHK(x) glew_chk((x), NULL, 0, NULL)
#endif

#ifdef __cplusplus
}
#endif


#endif  /* __GLEW_MX_H__ */
