/*
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
 */

#pragma once

/** \file
 * \ingroup DNA
 */

#include "DNA_ID.h"
#include "DNA_listBase.h"

/* Notice that we need to have this file although no struct defines.
 * Edge flags and usage flags are used by with scene/object/gpencil modifier bits, and those values
 * needs to stay consistent throughout. */

/* These flags are used for 1 time calculation, not stroke selection afterwards. */
typedef enum eLineartMainFlags {
  LRT_INTERSECTION_AS_CONTOUR = (1 << 0),
  LRT_EVERYTHING_AS_CONTOUR = (1 << 1),
  LRT_ALLOW_DUPLI_OBJECTS = (1 << 2),
  LRT_ALLOW_OVERLAPPING_EDGES = (1 << 3),
  LRT_ALLOW_CLIPPING_BOUNDARIES = (1 << 4),
  LRT_REMOVE_DOUBLES = (1 << 5),
  LRT_LOOSE_AS_CONTOUR = (1 << 6),
  LRT_GPENCIL_INVERT_SOURCE_VGROUP = (1 << 7),
  LRT_GPENCIL_MATCH_OUTPUT_VGROUP = (1 << 8),
  LRT_FILTER_FACE_MARK = (1 << 9),
  LRT_FILTER_FACE_MARK_INVERT = (1 << 10),
  LRT_FILTER_FACE_MARK_BOUNDARIES = (1 << 11),
  LRT_CHAIN_LOOSE_EDGES = (1 << 12),
  LRT_CHAIN_GEOMETRY_SPACE = (1 << 13),
  LRT_ALLOW_OVERLAP_EDGE_TYPES = (1 << 14),
  LRT_USE_CREASE_ON_SMOOTH_SURFACES = (1 << 15),
  LRT_USE_CREASE_ON_SHARP_EDGES = (1 << 16),
  LRT_USE_CUSTOM_CAMERA = (1 << 17),
  LRT_USE_IMAGE_BOUNDARY_TRIMMING = (1 << 20),
} eLineartMainFlags;

typedef enum eLineartEdgeFlag {
  LRT_EDGE_FLAG_EDGE_MARK = (1 << 0),
  LRT_EDGE_FLAG_CONTOUR = (1 << 1),
  LRT_EDGE_FLAG_CREASE = (1 << 2),
  LRT_EDGE_FLAG_MATERIAL = (1 << 3),
  LRT_EDGE_FLAG_INTERSECTION = (1 << 4),
  LRT_EDGE_FLAG_LOOSE = (1 << 5),
  LRT_EDGE_FLAG_CHAIN_PICKED = (1 << 6),
  LRT_EDGE_FLAG_CLIPPED = (1 << 7),
  /** Limited to 8 bits, DON'T ADD ANYMORE until improvements on the data structure. */
} eLineartEdgeFlag;

#define LRT_EDGE_FLAG_ALL_TYPE 0x3f
