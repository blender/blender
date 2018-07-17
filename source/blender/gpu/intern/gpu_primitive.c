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

/** \file blender/gpu/intern/gpu_primitive.c
 *  \ingroup gpu
 *
 * GPU geometric primitives
 */

#include "GPU_primitive.h"
#include "gpu_primitive_private.h"

GPUPrimClass GPU_primtype_class(GPUPrimType prim_type)
{
	static const GPUPrimClass classes[] = {
		[GPU_PRIM_POINTS] = GPU_PRIM_CLASS_POINT,
		[GPU_PRIM_LINES] = GPU_PRIM_CLASS_LINE,
		[GPU_PRIM_LINE_STRIP] = GPU_PRIM_CLASS_LINE,
		[GPU_PRIM_LINE_LOOP] = GPU_PRIM_CLASS_LINE,
		[GPU_PRIM_TRIS] = GPU_PRIM_CLASS_SURFACE,
		[GPU_PRIM_TRI_STRIP] = GPU_PRIM_CLASS_SURFACE,
		[GPU_PRIM_TRI_FAN] = GPU_PRIM_CLASS_SURFACE,

		[GPU_PRIM_LINES_ADJ] = GPU_PRIM_CLASS_LINE,
		[GPU_PRIM_LINE_STRIP_ADJ] = GPU_PRIM_CLASS_LINE,
		[GPU_PRIM_TRIS_ADJ] = GPU_PRIM_CLASS_SURFACE,

		[GPU_PRIM_NONE] = GPU_PRIM_CLASS_NONE
	};

	return classes[prim_type];
}

bool GPU_primtype_belongs_to_class(GPUPrimType prim_type, GPUPrimClass prim_class)
{
	if (prim_class == GPU_PRIM_CLASS_NONE && prim_type == GPU_PRIM_NONE) {
		return true;
	}
	return prim_class & GPU_primtype_class(prim_type);
}

GLenum convert_prim_type_to_gl(GPUPrimType prim_type)
{
#if TRUST_NO_ONE
	assert(prim_type != GPU_PRIM_NONE);
#endif
	static const GLenum table[] = {
		[GPU_PRIM_POINTS] = GL_POINTS,
		[GPU_PRIM_LINES] = GL_LINES,
		[GPU_PRIM_LINE_STRIP] = GL_LINE_STRIP,
		[GPU_PRIM_LINE_LOOP] = GL_LINE_LOOP,
		[GPU_PRIM_TRIS] = GL_TRIANGLES,
		[GPU_PRIM_TRI_STRIP] = GL_TRIANGLE_STRIP,
		[GPU_PRIM_TRI_FAN] = GL_TRIANGLE_FAN,

		[GPU_PRIM_LINES_ADJ] = GL_LINES_ADJACENCY,
		[GPU_PRIM_LINE_STRIP_ADJ] = GL_LINE_STRIP_ADJACENCY,
		[GPU_PRIM_TRIS_ADJ] = GL_TRIANGLES_ADJACENCY,
	};

	return table[prim_type];
}
