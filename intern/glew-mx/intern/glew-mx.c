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

/** \file glew-mx.c
 *  \ingroup glew-mx
 */

#include "glew-mx.h"

#include <stdio.h>
#include <stdlib.h>


#define CASE_CODE_RETURN_STR(code) case code: return #code;

static const char *get_glew_error_enum_string(GLenum error)
{
	switch (error) {
		CASE_CODE_RETURN_STR(GLEW_OK) /* also GLEW_NO_ERROR */
		CASE_CODE_RETURN_STR(GLEW_ERROR_NO_GL_VERSION)
		CASE_CODE_RETURN_STR(GLEW_ERROR_GL_VERSION_10_ONLY)
		CASE_CODE_RETURN_STR(GLEW_ERROR_GLX_VERSION_11_ONLY)
#ifdef WITH_GLEW_ES
		CASE_CODE_RETURN_STR(GLEW_ERROR_NOT_GLES_VERSION)
		CASE_CODE_RETURN_STR(GLEW_ERROR_GLES_VERSION)
		CASE_CODE_RETURN_STR(GLEW_ERROR_NO_EGL_VERSION)
		CASE_CODE_RETURN_STR(GLEW_ERROR_EGL_VERSION_10_ONLY)
#endif
		default:
			return NULL;
	}
}


GLenum glew_chk(GLenum error, const char *file, int line, const char *text)
{
	if (error != GLEW_OK) {
		const char *code = get_glew_error_enum_string(error);
		const char *msg  = (const char *)glewGetErrorString(error);

#ifndef NDEBUG
		fprintf(stderr,
		        "%s(%d):[%s] -> GLEW Error (0x%04X): %s: %s\n",
		        file, line, text, error,
		        code ? code : "<no symbol>",
		        msg  ? msg  : "<no message>");
#else
		fprintf(stderr,
		        "GLEW Error (0x%04X): %s: %s\n",
		        error,
		        code ? code : "<no symbol>",
		        msg  ? msg  : "<no message>");
#endif
	}

	return error;
}


#ifdef WITH_GLEW_MX
MXContext *_mx_context = NULL;
#endif


MXContext *mxCreateContext(void)
{
#if WITH_GLEW_MX
	MXContext* new_ctx = calloc(1, sizeof(MXContext));

	if (new_ctx != NULL) {
		MXContext* cur_ctx = _mx_context;
		_mx_context = new_ctx;
		GLEW_CHK(glewInit());
		_mx_context = cur_ctx;
	}

	return new_ctx;
#else
	GLEW_CHK(glewInit());
	return NULL;
#endif
}


MXContext *mxGetCurrentContext(void)
{
#if WITH_GLEW_MX
	return _mx_context;
#else
	return NULL;
#endif
}


void mxMakeCurrentContext(MXContext *ctx)
{
#if WITH_GLEW_MX
	_mx_context = ctx;
#else
	(void)ctx;
#endif
}


void mxDestroyContext(MXContext *ctx)
{
#if WITH_GLEW_MX
	if (_mx_context == ctx)
		_mx_context = NULL;

	free(ctx);
#else
	(void)ctx;
#endif
}
