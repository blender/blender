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

/** \file blender/gpu/gwn_primitive.h
 *  \ingroup gpu
 *
 * Gawain geometric primitives
 */

#ifndef __GWN_PRIMITIVE_H__
#define __GWN_PRIMITIVE_H__

#include "GPU_common.h"

typedef enum {
	GWN_PRIM_POINTS,
	GWN_PRIM_LINES,
	GWN_PRIM_TRIS,
	GWN_PRIM_LINE_STRIP,
	GWN_PRIM_LINE_LOOP, /* GL has this, Vulkan does not */
	GWN_PRIM_TRI_STRIP,
	GWN_PRIM_TRI_FAN,

	GWN_PRIM_LINES_ADJ,
	GWN_PRIM_TRIS_ADJ,
	GWN_PRIM_LINE_STRIP_ADJ,

	GWN_PRIM_NONE
} Gwn_PrimType;

/* what types of primitives does each shader expect? */
typedef enum {
	GWN_PRIM_CLASS_NONE    = 0,
	GWN_PRIM_CLASS_POINT   = (1 << 0),
	GWN_PRIM_CLASS_LINE    = (1 << 1),
	GWN_PRIM_CLASS_SURFACE = (1 << 2),
	GWN_PRIM_CLASS_ANY     = GWN_PRIM_CLASS_POINT | GWN_PRIM_CLASS_LINE | GWN_PRIM_CLASS_SURFACE
} Gwn_PrimClass;

Gwn_PrimClass GWN_primtype_class(Gwn_PrimType);
bool GWN_primtype_belongs_to_class(Gwn_PrimType, Gwn_PrimClass);

#endif /* __GWN_PRIMITIVE_H__ */
