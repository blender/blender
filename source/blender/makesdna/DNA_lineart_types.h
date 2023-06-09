/* SPDX-FileCopyrightText: 2010 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup DNA
 */

#include "DNA_ID.h"
#include "DNA_listBase.h"

/* Notice that we need to have this file although no struct defines.
 * Edge flags and usage flags are used by with scene/object/gpencil modifier bits, and those values
 * needs to stay consistent throughout. */

/** These flags are used for 1 time calculation, not stroke selection afterwards. */
typedef enum eLineartMainFlags {
  LRT_INTERSECTION_AS_CONTOUR = (1 << 0),
  LRT_EVERYTHING_AS_CONTOUR = (1 << 1),
  LRT_ALLOW_DUPLI_OBJECTS = (1 << 2),
  LRT_ALLOW_OVERLAPPING_EDGES = (1 << 3),
  LRT_ALLOW_CLIPPING_BOUNDARIES = (1 << 4),
  /* LRT_REMOVE_DOUBLES = (1 << 5), Deprecated */
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
  LRT_FILTER_FACE_MARK_KEEP_CONTOUR = (1 << 18),
  LRT_USE_BACK_FACE_CULLING = (1 << 19),
  LRT_USE_IMAGE_BOUNDARY_TRIMMING = (1 << 20),
  LRT_CHAIN_PRESERVE_DETAILS = (1 << 22),
  LRT_SHADOW_USE_SILHOUETTE = (1 << 24),
} eLineartMainFlags;

typedef enum eLineartEdgeFlag {
  LRT_EDGE_FLAG_EDGE_MARK = (1 << 0),
  LRT_EDGE_FLAG_CONTOUR = (1 << 1),
  LRT_EDGE_FLAG_CREASE = (1 << 2),
  LRT_EDGE_FLAG_MATERIAL = (1 << 3),
  LRT_EDGE_FLAG_INTERSECTION = (1 << 4),
  LRT_EDGE_FLAG_LOOSE = (1 << 5),
  LRT_EDGE_FLAG_LIGHT_CONTOUR = (1 << 6),
  /* LRT_EDGE_FLAG_FOR_FUTURE = (1 << 7), */
  /**
   * It's a legacy limit of 8 bits for feature lines that come from original mesh edges. It should
   * not be needed in current object loading scheme, but might still be relevant if we are to
   * implement edit-mesh loading, so don't exceed 8 bits just yet.
   */
  LRT_EDGE_FLAG_PROJECTED_SHADOW = (1 << 8),
  /* To determine an edge to be occluded from the front or back face it's lying on. */
  LRT_EDGE_FLAG_SHADOW_FACING_LIGHT = (1 << 9),
  /** Also used as discarded line mark. */
  LRT_EDGE_FLAG_CHAIN_PICKED = (1 << 10),
  LRT_EDGE_FLAG_CLIPPED = (1 << 11),
  /** Used to specify contour from viewing camera when computing shadows. */
  LRT_EDGE_FLAG_CONTOUR_SECONDARY = (1 << 12),
  /** Limited to 16 bits for the entire thing. */

  /** For object loading code to use only. */
  LRT_EDGE_FLAG_INHIBIT = (1 << 14),
  /** For discarding duplicated edge types in culling stage. */
  LRT_EDGE_FLAG_NEXT_IS_DUPLICATION = (1 << 15),
} eLineartEdgeFlag;

#define LRT_EDGE_FLAG_ALL_TYPE 0x01ff
#define LRT_EDGE_FLAG_INIT_TYPE 0x37 /* Without material & light contour */
#define LRT_EDGE_FLAG_TYPE_MAX_BITS 7
