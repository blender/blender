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

/** \file blender/gpu/GPU_primitive.h
 *  \ingroup gpu
 *
 * GPU geometric primitives
 */

#ifndef __GPU_PRIMITIVE_H__
#define __GPU_PRIMITIVE_H__

#include "GPU_common.h"

typedef enum {
	GPU_PRIM_POINTS,
	GPU_PRIM_LINES,
	GPU_PRIM_TRIS,
	GPU_PRIM_LINE_STRIP,
	GPU_PRIM_LINE_LOOP, /* GL has this, Vulkan does not */
	GPU_PRIM_TRI_STRIP,
	GPU_PRIM_TRI_FAN,

	GPU_PRIM_LINES_ADJ,
	GPU_PRIM_TRIS_ADJ,
	GPU_PRIM_LINE_STRIP_ADJ,

	GPU_PRIM_NONE
} GPUPrimType;

/* what types of primitives does each shader expect? */
typedef enum {
	GPU_PRIM_CLASS_NONE    = 0,
	GPU_PRIM_CLASS_POINT   = (1 << 0),
	GPU_PRIM_CLASS_LINE    = (1 << 1),
	GPU_PRIM_CLASS_SURFACE = (1 << 2),
	GPU_PRIM_CLASS_ANY     = GPU_PRIM_CLASS_POINT | GPU_PRIM_CLASS_LINE | GPU_PRIM_CLASS_SURFACE
} GPUPrimClass;

GPUPrimClass GPU_primtype_class(GPUPrimType);
bool GPU_primtype_belongs_to_class(GPUPrimType, GPUPrimClass);

#endif /* __GPU_PRIMITIVE_H__ */
