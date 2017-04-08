
// Gawain geometric primitives
//
// This code is part of the Gawain library, with modifications
// specific to integration with Blender.
//
// Copyright 2017 Mike Erwin
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of
// the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.

#pragma once

#include "common.h"

typedef enum {
	PRIM_POINTS,
	PRIM_LINES,
	PRIM_TRIANGLES,
	PRIM_LINE_STRIP,
	PRIM_LINE_LOOP, // GL has this, Vulkan does not
	PRIM_TRIANGLE_STRIP,
	PRIM_TRIANGLE_FAN,

	PRIM_LINE_STRIP_ADJACENCY,

#ifdef WITH_GL_PROFILE_COMPAT
	PRIM_QUADS_XXX, // legacy GL has this, modern GL & Vulkan do not
#endif

	PRIM_NONE
} PrimitiveType;

// what types of primitives does each shader expect?
typedef enum {
	PRIM_CLASS_NONE    = 0,
	PRIM_CLASS_POINT   = (1 << 0),
	PRIM_CLASS_LINE    = (1 << 1),
	PRIM_CLASS_SURFACE = (1 << 2),
	PRIM_CLASS_ANY     = PRIM_CLASS_POINT | PRIM_CLASS_LINE | PRIM_CLASS_SURFACE
} PrimitiveClass;

PrimitiveClass prim_class_of_type(PrimitiveType);
bool prim_type_belongs_to_class(PrimitiveType, PrimitiveClass);

GLenum convert_prim_type_to_gl(PrimitiveType);
