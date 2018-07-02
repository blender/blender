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

/* hacking pointsize and linewidth */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define glPointSize(f)  glPointSize(U.pixelsize * _Generic((f), double: (float)(f), default: (f)))
#  define glLineWidth(f)  glLineWidth(U.pixelsize * _Generic((f), double: (float)(f), default: (f)))
#else
#  define glPointSize(f)  glPointSize(U.pixelsize * (f))
#  define glLineWidth(f)  glLineWidth(U.pixelsize * (f))
#endif  /* C11 */

#define GLA_PIXEL_OFS 0.375f

#endif /* #ifdef __BIF_GL_H__ */
