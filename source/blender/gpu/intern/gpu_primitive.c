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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gwn_primitive.c
 *  \ingroup gpu
 *
 * Gawain geometric primitives
 */

#include "GPU_primitive.h"
#include "gpu_primitive_private.h"

Gwn_PrimClass GWN_primtype_class(Gwn_PrimType prim_type)
{
	static const Gwn_PrimClass classes[] = {
		[GWN_PRIM_POINTS] = GWN_PRIM_CLASS_POINT,
		[GWN_PRIM_LINES] = GWN_PRIM_CLASS_LINE,
		[GWN_PRIM_LINE_STRIP] = GWN_PRIM_CLASS_LINE,
		[GWN_PRIM_LINE_LOOP] = GWN_PRIM_CLASS_LINE,
		[GWN_PRIM_TRIS] = GWN_PRIM_CLASS_SURFACE,
		[GWN_PRIM_TRI_STRIP] = GWN_PRIM_CLASS_SURFACE,
		[GWN_PRIM_TRI_FAN] = GWN_PRIM_CLASS_SURFACE,

		[GWN_PRIM_LINES_ADJ] = GWN_PRIM_CLASS_LINE,
		[GWN_PRIM_LINE_STRIP_ADJ] = GWN_PRIM_CLASS_LINE,
		[GWN_PRIM_TRIS_ADJ] = GWN_PRIM_CLASS_SURFACE,

		[GWN_PRIM_NONE] = GWN_PRIM_CLASS_NONE
	};

	return classes[prim_type];
}

bool GWN_primtype_belongs_to_class(Gwn_PrimType prim_type, Gwn_PrimClass prim_class)
{
	if (prim_class == GWN_PRIM_CLASS_NONE && prim_type == GWN_PRIM_NONE) {
		return true;
	}
	return prim_class & GWN_primtype_class(prim_type);
}

GLenum convert_prim_type_to_gl(Gwn_PrimType prim_type)
{
#if TRUST_NO_ONE
	assert(prim_type != GWN_PRIM_NONE);
#endif
	static const GLenum table[] = {
		[GWN_PRIM_POINTS] = GL_POINTS,
		[GWN_PRIM_LINES] = GL_LINES,
		[GWN_PRIM_LINE_STRIP] = GL_LINE_STRIP,
		[GWN_PRIM_LINE_LOOP] = GL_LINE_LOOP,
		[GWN_PRIM_TRIS] = GL_TRIANGLES,
		[GWN_PRIM_TRI_STRIP] = GL_TRIANGLE_STRIP,
		[GWN_PRIM_TRI_FAN] = GL_TRIANGLE_FAN,

		[GWN_PRIM_LINES_ADJ] = GL_LINES_ADJACENCY,
		[GWN_PRIM_LINE_STRIP_ADJ] = GL_LINE_STRIP_ADJACENCY,
		[GWN_PRIM_TRIS_ADJ] = GL_TRIANGLES_ADJACENCY,
	};

	return table[prim_type];
}
