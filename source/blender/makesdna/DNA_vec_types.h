/**
 * vec_types.h    dec 2000 Nzc
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * 
 */

#ifndef DNA_VEC_TYPES_H
#define DNA_VEC_TYPES_H

/* types */
typedef struct vec2s {
	short x, y;
} vec2s;

typedef struct vec2i {
	int x, y;
} vec2i;

typedef struct vec2f {
	float x, y;
} vec2f;

typedef struct vec2d {
	double x, y;
} vec2d;

typedef struct vec3i {
	int x, y, z;
} vec3i;

typedef struct vec3f {
	float x, y, z;
} vec3f;

typedef struct vec3d {
	double x, y, z;
} vec3d;

typedef struct vec4i {
	int x, y, z, w;
} vec4i;

typedef struct vec4f {
	float x, y, z, w;
} vec4f;

typedef struct vec4d {
	double x, y, z, w;
} vec4d;

typedef struct rcti {
    int xmin, xmax;
    int ymin, ymax;
} rcti;

typedef struct rctf {
    float xmin, xmax;
    float ymin, ymax;
} rctf;

#endif

