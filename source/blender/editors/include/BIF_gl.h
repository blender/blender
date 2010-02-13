/**
 * $Id$
 *
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

#ifndef BIF_GL_H
#define BIF_GL_H

#include "GL/glew.h"

	/*
	 * these should be phased out. cpack should be replaced in
	 * code with calls to glColor3ub. - zr
	 */
/* 
 *
 * This define converts a numerical value to the equivalent 24-bit
 * color, while not being endian-sensitive. On little-endians, this
 * is the same as doing a 'naive'indexing, on big-endian, it is not!
 * */
#define cpack(x)	glColor3ub( ((x)&0xFF), (((x)>>8)&0xFF), (((x)>>16)&0xFF) )

#define glMultMatrixf(x)		glMultMatrixf( (float *)(x))
#define glLoadMatrixf(x)		glLoadMatrixf( (float *)(x))

#endif /* #ifdef BIF_GL_H */

