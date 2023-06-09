/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_utildefines.h"

/** #ToolSettings.vgroupsubset */
typedef enum eVGroupSelect {
  WT_VGROUP_ALL = 0,
  WT_VGROUP_ACTIVE = 1,
  WT_VGROUP_BONE_SELECT = 2,
  WT_VGROUP_BONE_DEFORM = 3,
  WT_VGROUP_BONE_DEFORM_OFF = 4,
} eVGroupSelect;

typedef enum eSculptBoundary {
  SCULPT_BOUNDARY_NONE = 0,
  SCULPT_BOUNDARY_MESH = 1 << 0,
  SCULPT_BOUNDARY_FACE_SET = 1 << 1,
  SCULPT_BOUNDARY_SEAM = 1 << 2,
  SCULPT_BOUNDARY_SHARP_MARK = 1 << 3,  /* Edges marked as sharp. */
  SCULPT_BOUNDARY_SHARP_ANGLE = 1 << 4, /* Edges whose face angle is above a limit */
  SCULPT_BOUNDARY_UV = 1 << 5,
  SCULPT_BOUNDARY_NEEDS_UPDATE = 1 << 6,
  SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE = 1 << 7,
  SCULPT_BOUNDARY_UPDATE_UV = 1 << 8,

  SCULPT_BOUNDARY_ALL = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 5),
  SCULPT_BOUNDARY_DEFAULT = (1 << 0) | (1 << 3) | (1 << 4)  // mesh and sharp
} eSculptBoundary;
ENUM_OPERATORS(eSculptBoundary, SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE);

/* Note: This is stored in a single attribute with boundary flags */
typedef enum eSculptCorner {
  SCULPT_CORNER_NONE = 0,
  SCULPT_CORNER_BIT_SHIFT =
      16, /* Shift boundary flags by this much to get matching corner flags. */
  SCULPT_CORNER_MESH = 1 << 16,
  SCULPT_CORNER_FACE_SET = 1 << 17,
  SCULPT_CORNER_SEAM = 1 << 18,
  SCULPT_CORNER_SHARP_MARK = 1 << 19,
  SCULPT_CORNER_SHARP_ANGLE = 1 << 20,
  SCULPT_CORNER_UV = 1 << 21,
} eSculptCorner;
ENUM_OPERATORS(eSculptCorner, SCULPT_CORNER_UV);
