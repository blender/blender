/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup DNA
 */

#pragma once

/** #ToolSettings.vgroupsubset */
typedef enum eVGroupSelect {
  WT_VGROUP_ALL = 0,
  WT_VGROUP_ACTIVE = 1,
  WT_VGROUP_BONE_SELECT = 2,
  WT_VGROUP_BONE_DEFORM = 3,
  WT_VGROUP_BONE_DEFORM_OFF = 4,
} eVGroupSelect;
