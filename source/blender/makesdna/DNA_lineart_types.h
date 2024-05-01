/* SPDX-FileCopyrightText: 2010 Blender Authors
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
  MOD_LINEART_INTERSECTION_AS_CONTOUR = (1 << 0),
  MOD_LINEART_EVERYTHING_AS_CONTOUR = (1 << 1),
  MOD_LINEART_ALLOW_DUPLI_OBJECTS = (1 << 2),
  MOD_LINEART_ALLOW_OVERLAPPING_EDGES = (1 << 3),
  MOD_LINEART_ALLOW_CLIPPING_BOUNDARIES = (1 << 4),
  /* LRT_REMOVE_DOUBLES = (1 << 5), Deprecated */
  MOD_LINEART_LOOSE_AS_CONTOUR = (1 << 6),
  MOD_LINEART_INVERT_SOURCE_VGROUP = (1 << 7),
  MOD_LINEART_MATCH_OUTPUT_VGROUP = (1 << 8),
  MOD_LINEART_FILTER_FACE_MARK = (1 << 9),
  MOD_LINEART_FILTER_FACE_MARK_INVERT = (1 << 10),
  MOD_LINEART_FILTER_FACE_MARK_BOUNDARIES = (1 << 11),
  MOD_LINEART_CHAIN_LOOSE_EDGES = (1 << 12),
  MOD_LINEART_CHAIN_GEOMETRY_SPACE = (1 << 13),
  MOD_LINEART_ALLOW_OVERLAP_EDGE_TYPES = (1 << 14),
  MOD_LINEART_USE_CREASE_ON_SMOOTH_SURFACES = (1 << 15),
  MOD_LINEART_USE_CREASE_ON_SHARP_EDGES = (1 << 16),
  MOD_LINEART_USE_CUSTOM_CAMERA = (1 << 17),
  MOD_LINEART_FILTER_FACE_MARK_KEEP_CONTOUR = (1 << 18),
  MOD_LINEART_USE_BACK_FACE_CULLING = (1 << 19),
  MOD_LINEART_USE_IMAGE_BOUNDARY_TRIMMING = (1 << 20),
  MOD_LINEART_CHAIN_PRESERVE_DETAILS = (1 << 22),
  MOD_LINEART_SHADOW_USE_SILHOUETTE = (1 << 24),
} eLineartMainFlags;

typedef enum eLineartEdgeFlag {
  MOD_LINEART_EDGE_FLAG_EDGE_MARK = (1 << 0),
  MOD_LINEART_EDGE_FLAG_CONTOUR = (1 << 1),
  MOD_LINEART_EDGE_FLAG_CREASE = (1 << 2),
  MOD_LINEART_EDGE_FLAG_MATERIAL = (1 << 3),
  MOD_LINEART_EDGE_FLAG_INTERSECTION = (1 << 4),
  MOD_LINEART_EDGE_FLAG_LOOSE = (1 << 5),
  MOD_LINEART_EDGE_FLAG_LIGHT_CONTOUR = (1 << 6),
  /* MOD_LINEART_EDGE_FLAG_FOR_FUTURE = (1 << 7), */
  /**
   * It's a legacy limit of 8 bits for feature lines that come from original mesh edges. It should
   * not be needed in current object loading scheme, but might still be relevant if we are to
   * implement edit-mesh loading, so don't exceed 8 bits just yet.
   */
  MOD_LINEART_EDGE_FLAG_PROJECTED_SHADOW = (1 << 8),
  /* To determine an edge to be occluded from the front or back face it's lying on. */
  MOD_LINEART_EDGE_FLAG_SHADOW_FACING_LIGHT = (1 << 9),
  /** Also used as discarded line mark. */
  MOD_LINEART_EDGE_FLAG_CHAIN_PICKED = (1 << 10),
  MOD_LINEART_EDGE_FLAG_CLIPPED = (1 << 11),
  /** Used to specify contour from viewing camera when computing shadows. */
  MOD_LINEART_EDGE_FLAG_CONTOUR_SECONDARY = (1 << 12),
  /** Limited to 16 bits for the entire thing. */

  /** For object loading code to use only. */
  MOD_LINEART_EDGE_FLAG_INHIBIT = (1 << 14),
  /** For discarding duplicated edge types in culling stage. */
  MOD_LINEART_EDGE_FLAG_NEXT_IS_DUPLICATION = (1 << 15),
} eLineartEdgeFlag;

#define MOD_LINEART_EDGE_FLAG_ALL_TYPE 0x01ff
#define MOD_LINEART_EDGE_FLAG_INIT_TYPE 0x37 /* Without material & light contour */
#define MOD_LINEART_EDGE_FLAG_TYPE_MAX_BITS 7
