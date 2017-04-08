
// Gawain geometric primitives
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2017 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#include "primitive.h"

PrimitiveClass prim_class_of_type(PrimitiveType prim_type)
	{
	static const PrimitiveClass classes[] =
		{
		[PRIM_POINTS] = PRIM_CLASS_POINT,
		[PRIM_LINES] = PRIM_CLASS_LINE,
		[PRIM_LINE_STRIP] = PRIM_CLASS_LINE,
		[PRIM_LINE_LOOP] = PRIM_CLASS_LINE,
		[PRIM_TRIANGLES] = PRIM_CLASS_SURFACE,
		[PRIM_TRIANGLE_STRIP] = PRIM_CLASS_SURFACE,
		[PRIM_TRIANGLE_FAN] = PRIM_CLASS_SURFACE,

		[PRIM_LINE_STRIP_ADJACENCY] = PRIM_CLASS_LINE,

#ifdef WITH_GL_PROFILE_COMPAT
		[PRIM_QUADS_XXX] = PRIM_CLASS_SURFACE,
#endif

		[PRIM_NONE] = PRIM_CLASS_NONE
		};

	return classes[prim_type];
	}

bool prim_type_belongs_to_class(PrimitiveType prim_type, PrimitiveClass prim_class)
	{
	if (prim_class == PRIM_CLASS_NONE && prim_type == PRIM_NONE)
		return true;

	return prim_class & prim_class_of_type(prim_type);
	}

GLenum convert_prim_type_to_gl(PrimitiveType prim_type)
	{
#if TRUST_NO_ONE
	assert(prim_type != PRIM_NONE);
#endif

	static const GLenum table[] =
		{
		[PRIM_POINTS] = GL_POINTS,
		[PRIM_LINES] = GL_LINES,
		[PRIM_LINE_STRIP] = GL_LINE_STRIP,
		[PRIM_LINE_LOOP] = GL_LINE_LOOP,
		[PRIM_TRIANGLES] = PRIM_CLASS_SURFACE,
		[PRIM_TRIANGLE_STRIP] = GL_TRIANGLE_STRIP,
		[PRIM_TRIANGLE_FAN] = GL_TRIANGLE_FAN,

		[PRIM_LINE_STRIP_ADJACENCY] = GL_LINE_STRIP_ADJACENCY,

#ifdef WITH_GL_PROFILE_COMPAT
		[PRIM_QUADS_XXX] = GL_QUADS,
#endif
		};

	return table[prim_type];
	}
