
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
		[PRIM_NONE] = PRIM_CLASS_NONE,
		[PRIM_POINTS] = PRIM_CLASS_POINT,
		[PRIM_LINES] = PRIM_CLASS_LINE,
		[PRIM_LINE_STRIP] = PRIM_CLASS_LINE,
		[PRIM_LINE_LOOP] = PRIM_CLASS_LINE,
		[PRIM_TRIANGLES] = PRIM_CLASS_SURFACE,
		[PRIM_TRIANGLE_STRIP] = PRIM_CLASS_SURFACE,
		[PRIM_TRIANGLE_FAN] = PRIM_CLASS_SURFACE,

#ifdef WITH_GL_PROFILE_COMPAT
		[PRIM_QUADS_XXX] = PRIM_CLASS_SURFACE,
#endif
		};

	return classes[prim_type];
	}

bool prim_type_belongs_to_class(PrimitiveType prim_type, PrimitiveClass prim_class)
	{
	if (prim_class == PRIM_CLASS_NONE && prim_type == PRIM_NONE)
		return true;

	return prim_class & prim_class_of_type(prim_type);
	}
