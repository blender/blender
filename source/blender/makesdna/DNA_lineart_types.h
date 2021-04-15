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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __DNA_LRT_TYPES_H__
#define __DNA_LRT_TYPES_H__

/** \file DNA_lineart_types.h
 *  \ingroup DNA
 */

#include "DNA_ID.h"
#include "DNA_listBase.h"

/* Notice that we need to have this file although no struct defines.
 * Edge flags and usage flags are used by with scene/object/gpencil modifier bits, and those values
 * needs to stay consistent throughout. */

typedef enum eLineartMainFlags {
  LRT_INTERSECTION_AS_CONTOUR = (1 << 0),
  LRT_EVERYTHING_AS_CONTOUR = (1 << 1),
  LRT_ALLOW_DUPLI_OBJECTS = (1 << 2),
  LRT_ALLOW_OVERLAPPING_EDGES = (1 << 3),
  LRT_ALLOW_CLIPPING_BOUNDARIES = (1 << 4),
  LRT_REMOVE_DOUBLES = (1 << 5),
} eLineartMainFlags;

typedef enum eLineartEdgeFlag {
  LRT_EDGE_FLAG_EDGE_MARK = (1 << 0),
  LRT_EDGE_FLAG_CONTOUR = (1 << 1),
  LRT_EDGE_FLAG_CREASE = (1 << 2),
  LRT_EDGE_FLAG_MATERIAL = (1 << 3),
  LRT_EDGE_FLAG_INTERSECTION = (1 << 4),
  /** Floating edge, unimplemented yet. */
  LRT_EDGE_FLAG_FLOATING = (1 << 5),
  /** Also used as discarded line mark. */
  LRT_EDGE_FLAG_CHAIN_PICKED = (1 << 6),
  LRT_EDGE_FLAG_CLIPPED = (1 << 7),
  /** Limited to 8 bits, DON'T ADD ANYMORE until improvements on the data structure. */
} eLineartEdgeFlag;

#define LRT_EDGE_FLAG_ALL_TYPE 0x3f

#endif
