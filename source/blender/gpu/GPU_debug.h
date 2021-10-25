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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/GPU_debug.h
 *  \ingroup gpu
 */

#ifndef __GPU_DEBUG_H__
#define __GPU_DEBUG_H__

#include "GPU_glew.h"

#ifdef __cplusplus
extern "C" {
#endif

/* prints something if debug mode is active only */
void GPU_print_error_debug(const char *str);

/* replacement for gluErrorString */
const char *gpuErrorString(GLenum err);

/* prints current OpenGL state */
void GPU_state_print(void);

void GPU_assert_no_gl_errors(const char *file, int line, const char *str);

#  define GPU_ASSERT_NO_GL_ERRORS(str) GPU_assert_no_gl_errors(__FILE__, __LINE__, (str))

#  define GPU_CHECK_ERRORS_AROUND(glProcCall)                      \
       (                                             \
       GPU_ASSERT_NO_GL_ERRORS("Pre: "  #glProcCall), \
       (glProcCall),                                 \
       GPU_ASSERT_NO_GL_ERRORS("Post: " #glProcCall)  \
       )


/* inserts a debug marker message for the debug context messaging system */
void GPU_string_marker(const char *str);

#ifdef __cplusplus
}
#endif

#endif /* __GPU_DEBUG_H__ */
