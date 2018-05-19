
// Gawain geometric primitives
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2017 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "gwn_primitive.h"
#include "gwn_primitive_private.h"

Gwn_PrimClass GWN_primtype_class(Gwn_PrimType prim_type)
	{
	static const Gwn_PrimClass classes[] =
		{
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
	if (prim_class == GWN_PRIM_CLASS_NONE && prim_type == GWN_PRIM_NONE)
		return true;

	return prim_class & GWN_primtype_class(prim_type);
	}

GLenum convert_prim_type_to_gl(Gwn_PrimType prim_type)
	{
#if TRUST_NO_ONE
	assert(prim_type != GWN_PRIM_NONE);
#endif

	static const GLenum table[] =
		{
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
