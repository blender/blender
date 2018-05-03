
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

#include "gwn_common.h"

typedef enum {
	GWN_PRIM_POINTS,
	GWN_PRIM_LINES,
	GWN_PRIM_TRIS,
	GWN_PRIM_LINE_STRIP,
	GWN_PRIM_LINE_LOOP, // GL has this, Vulkan does not
	GWN_PRIM_TRI_STRIP,
	GWN_PRIM_TRI_FAN,

	GWN_PRIM_TRIS_ADJ,
	GWN_PRIM_LINE_STRIP_ADJ,

	GWN_PRIM_NONE
} Gwn_PrimType;

// what types of primitives does each shader expect?
typedef enum {
	GWN_PRIM_CLASS_NONE    = 0,
	GWN_PRIM_CLASS_POINT   = (1 << 0),
	GWN_PRIM_CLASS_LINE    = (1 << 1),
	GWN_PRIM_CLASS_SURFACE = (1 << 2),
	GWN_PRIM_CLASS_ANY     = GWN_PRIM_CLASS_POINT | GWN_PRIM_CLASS_LINE | GWN_PRIM_CLASS_SURFACE
} Gwn_PrimClass;

Gwn_PrimClass GWN_primtype_class(Gwn_PrimType);
bool GWN_primtype_belongs_to_class(Gwn_PrimType, Gwn_PrimClass);
