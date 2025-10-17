/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_enum_flags.hh"

/** #ToolSettings.vgroupsubset */
typedef enum eVGroupSelect {
  WT_VGROUP_ALL = 0,
  WT_VGROUP_ACTIVE = 1,
  WT_VGROUP_BONE_SELECT = 2,
  WT_VGROUP_BONE_DEFORM = 3,
  WT_VGROUP_BONE_DEFORM_OFF = 4,
} eVGroupSelect;

typedef enum eSeqImageFitMethod {
  SEQ_SCALE_TO_FIT,
  SEQ_SCALE_TO_FILL,
  SEQ_STRETCH_TO_FILL,
  SEQ_USE_ORIGINAL_SIZE,
} eSeqImageFitMethod;

/**
 * #Paint::symmetry_flags
 * (for now just a duplicate of sculpt symmetry flags).
 */
typedef enum ePaintSymmetryFlags {
  PAINT_SYMM_NONE = 0,
  PAINT_SYMM_X = (1 << 0),
  PAINT_SYMM_Y = (1 << 1),
  PAINT_SYMM_Z = (1 << 2),
  PAINT_SYMMETRY_FEATHER = (1 << 3),
  PAINT_TILE_X = (1 << 4),
  PAINT_TILE_Y = (1 << 5),
  PAINT_TILE_Z = (1 << 6),
} ePaintSymmetryFlags;
ENUM_OPERATORS(ePaintSymmetryFlags);
#define PAINT_SYMM_AXIS_ALL (PAINT_SYMM_X | PAINT_SYMM_Y | PAINT_SYMM_Z)

#ifdef __cplusplus
inline ePaintSymmetryFlags operator++(ePaintSymmetryFlags &flags, int)
{
  flags = ePaintSymmetryFlags(char(flags) + 1);
  return flags;
}
#endif

/** #UnifiedPaintSettings::flag */
typedef enum eUnifiedPaintSettingsFlags {
  UNIFIED_PAINT_SIZE = (1 << 0),
  UNIFIED_PAINT_ALPHA = (1 << 1),
  /** Only used if unified size is enabled, mirrors the brush flag #BRUSH_LOCK_SIZE. */
  UNIFIED_PAINT_BRUSH_LOCK_SIZE = (1 << 2),
  UNIFIED_PAINT_FLAG_UNUSED_0 = (1 << 3),
  UNIFIED_PAINT_FLAG_UNUSED_1 = (1 << 4),
  UNIFIED_PAINT_WEIGHT = (1 << 5),
  UNIFIED_PAINT_COLOR = (1 << 6),
  UNIFIED_PAINT_INPUT_SAMPLES = (1 << 7),
  UNIFIED_PAINT_COLOR_JITTER = (1 << 8),
} eUnifiedPaintSettingsFlags;

/** Paint::curve_visibility_flag*/
typedef enum PaintCurveVisibilityFlags {
  PAINT_CURVE_SHOW_STRENGTH = (1 << 0),
  PAINT_CURVE_SHOW_SIZE = (1 << 1),
  PAINT_CURVE_SHOW_JITTER = (1 << 2),
} PaintCurveVisibilityFlags;
