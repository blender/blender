/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup DNA
 */

#include "DNA_defs.h"
#include "DNA_listBase.h"

#include "BLI_enum_flags.hh"

namespace blender {

struct Collection;
struct FreestyleLineStyle;
struct Text;

/* FreestyleConfig::flags */
enum eFreestyleConfig_Flags : int {
  FREESTYLE_SUGGESTIVE_CONTOURS_FLAG = 1 << 0,
  FREESTYLE_RIDGES_AND_VALLEYS_FLAG = 1 << 1,
  FREESTYLE_MATERIAL_BOUNDARIES_FLAG = 1 << 2,
  FREESTYLE_FACE_SMOOTHNESS_FLAG = 1 << 3,
  /* FREESTYLE_ADVANCED_OPTIONS_FLAG = 1 << 4, */ /* UNUSED */
  FREESTYLE_CULLING = 1 << 5,
  FREESTYLE_VIEW_MAP_CACHE = 1 << 6,
  FREESTYLE_AS_RENDER_PASS = 1 << 7,
};
ENUM_OPERATORS(eFreestyleConfig_Flags)

/* FreestyleConfig::mode */
enum eFreestyleControl_Mode : int {
  FREESTYLE_CONTROL_SCRIPT_MODE = 1,
  FREESTYLE_CONTROL_EDITOR_MODE = 2,
};

/* FreestyleLineSet::flags */
enum eFreestyleLineSet_Flags : int {
  FREESTYLE_LINESET_CURRENT = 1 << 0,
  FREESTYLE_LINESET_ENABLED = 1 << 1,
  FREESTYLE_LINESET_FE_NOT = 1 << 2,
  FREESTYLE_LINESET_FE_AND = 1 << 3,
  FREESTYLE_LINESET_GR_NOT = 1 << 4,
  FREESTYLE_LINESET_FM_NOT = 1 << 5,
  FREESTYLE_LINESET_FM_BOTH = 1 << 6,
};
ENUM_OPERATORS(eFreestyleLineSet_Flags)

/* FreestyleLineSet::selection */
enum eFreestyleLineSet_Selection : int {
  FREESTYLE_SEL_VISIBILITY = 1 << 0,
  FREESTYLE_SEL_EDGE_TYPES = 1 << 1,
  FREESTYLE_SEL_GROUP = 1 << 2,
  FREESTYLE_SEL_IMAGE_BORDER = 1 << 3,
  FREESTYLE_SEL_FACE_MARK = 1 << 4,
};
ENUM_OPERATORS(eFreestyleLineSet_Selection)

/* FreestyleLineSet::edge_types, exclude_edge_types */
enum eFreestyleLineSet_EdgeTypes : int {
  FREESTYLE_FE_SILHOUETTE = 1 << 0,
  FREESTYLE_FE_BORDER = 1 << 1,
  FREESTYLE_FE_CREASE = 1 << 2,
  FREESTYLE_FE_RIDGE_VALLEY = 1 << 3,
  /* FREESTYLE_FE_VALLEY              = 1 << 4, */ /* No longer used */
  FREESTYLE_FE_SUGGESTIVE_CONTOUR = 1 << 5,
  FREESTYLE_FE_MATERIAL_BOUNDARY = 1 << 6,
  FREESTYLE_FE_CONTOUR = 1 << 7,
  FREESTYLE_FE_EXTERNAL_CONTOUR = 1 << 8,
  FREESTYLE_FE_EDGE_MARK = 1 << 9,
};
ENUM_OPERATORS(eFreestyleLineSet_EdgeTypes)

/* FreestyleLineSet::qi */
enum eFreestyleLineSet_QI : short {
  FREESTYLE_QI_VISIBLE = 1,
  FREESTYLE_QI_HIDDEN = 2,
  FREESTYLE_QI_RANGE = 3,
};

/* FreestyleConfig::raycasting_algorithm */
/* Defines should be replaced with ViewMapBuilder::visibility_algo */
enum eFreestyleRaycastingAlgorithm : int {
  FREESTYLE_ALGO_REGULAR = 1,
  FREESTYLE_ALGO_FAST = 2,
  FREESTYLE_ALGO_VERYFAST = 3,
  FREESTYLE_ALGO_CULLED_ADAPTIVE_TRADITIONAL = 4,
  FREESTYLE_ALGO_ADAPTIVE_TRADITIONAL = 5,
  FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE = 6,
  FREESTYLE_ALGO_ADAPTIVE_CUMULATIVE = 7,
};

struct FreestyleLineSet {
  struct FreestyleLineSet *next = nullptr, *prev = nullptr;

  char name[/*MAX_NAME*/ 64] = "";
  eFreestyleLineSet_Flags flags = {};

  /** Selection criteria. */
  eFreestyleLineSet_Selection selection = {};
  /** Quantitative invisibility. */
  eFreestyleLineSet_QI qi = {};
  char _pad1[2] = {};
  int qi_start = 0, qi_end = 0;
  /** Feature edge types. */
  eFreestyleLineSet_EdgeTypes edge_types = {}, exclude_edge_types = {};
  char _pad2[4] = {};
  /** Group of target objects. */
  struct Collection *group = nullptr;

  struct FreestyleLineStyle *linestyle = nullptr;
};

struct FreestyleModuleConfig {
  struct FreestyleModuleConfig *next = nullptr, *prev = nullptr;

  struct Text *script = nullptr;
  short is_displayed = 0;
  char _pad[6] = {};
};

struct FreestyleConfig {
  ListBaseT<FreestyleModuleConfig> modules = {nullptr, nullptr};

  /** Scripting, editor. */
  eFreestyleControl_Mode mode = {};
  DNA_DEPRECATED int raycasting_algorithm = 0;
  /** Suggestive contours, ridges/valleys, material boundaries. */
  eFreestyleConfig_Flags flags = {};
  float sphere_radius = 0;
  float dkr_epsilon = 0;
  /** In radians. */
  float crease_angle = 0;

  ListBaseT<FreestyleLineSet> linesets = {nullptr, nullptr};
};

}  // namespace blender
