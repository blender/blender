/*
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
 */

/** \file \ingroup glew-mx
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

		if (error == GLEW_ERROR_NO_GL_VERSION)
			return GLEW_OK;

#ifndef NDEBUG
		fprintf(stderr,
		        "%s(%d):[%s] -> GLEW Error (0x%04X): %s: %s\n",
		        file, line, text, error,
		        code ? code : "<no symbol>",
		        msg  ? msg  : "<no message>");
#else
		(void) file;
		(void) line;
		(void) text;
		fprintf(stderr,
		        "GLEW Error (0x%04X): %s: %s\n",
		        error,
		        code ? code : "<no symbol>",
		        msg  ? msg  : "<no message>");
#endif
	}

	return error;
}
