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

#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Collection;
struct FreestyleLineStyle;
struct Text;

/* FreestyleConfig::flags */
enum {
  FREESTYLE_SUGGESTIVE_CONTOURS_FLAG = 1 << 0,
  FREESTYLE_RIDGES_AND_VALLEYS_FLAG = 1 << 1,
  FREESTYLE_MATERIAL_BOUNDARIES_FLAG = 1 << 2,
  FREESTYLE_FACE_SMOOTHNESS_FLAG = 1 << 3,
  /* FREESTYLE_ADVANCED_OPTIONS_FLAG = 1 << 4, */ /* UNUSED */
  FREESTYLE_CULLING = 1 << 5,
  FREESTYLE_VIEW_MAP_CACHE = 1 << 6,
  FREESTYLE_AS_RENDER_PASS = 1 << 7,
};

/* FreestyleConfig::mode */
enum {
  FREESTYLE_CONTROL_SCRIPT_MODE = 1,
  FREESTYLE_CONTROL_EDITOR_MODE = 2,
};

/* FreestyleLineSet::flags */
enum {
  FREESTYLE_LINESET_CURRENT = 1 << 0,
  FREESTYLE_LINESET_ENABLED = 1 << 1,
  FREESTYLE_LINESET_FE_NOT = 1 << 2,
  FREESTYLE_LINESET_FE_AND = 1 << 3,
  FREESTYLE_LINESET_GR_NOT = 1 << 4,
  FREESTYLE_LINESET_FM_NOT = 1 << 5,
  FREESTYLE_LINESET_FM_BOTH = 1 << 6,
};

/* FreestyleLineSet::selection */
enum {
  FREESTYLE_SEL_VISIBILITY = 1 << 0,
  FREESTYLE_SEL_EDGE_TYPES = 1 << 1,
  FREESTYLE_SEL_GROUP = 1 << 2,
  FREESTYLE_SEL_IMAGE_BORDER = 1 << 3,
  FREESTYLE_SEL_FACE_MARK = 1 << 4,
};

/* FreestyleLineSet::edge_types, exclude_edge_types */
enum {
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

/* FreestyleLineSet::qi */
enum {
  FREESTYLE_QI_VISIBLE = 1,
  FREESTYLE_QI_HIDDEN = 2,
  FREESTYLE_QI_RANGE = 3,
};

/* FreestyleConfig::raycasting_algorithm */
/* Defines should be replaced with ViewMapBuilder::visibility_algo */
enum {
  FREESTYLE_ALGO_REGULAR = 1,
  FREESTYLE_ALGO_FAST = 2,
  FREESTYLE_ALGO_VERYFAST = 3,
  FREESTYLE_ALGO_CULLED_ADAPTIVE_TRADITIONAL = 4,
  FREESTYLE_ALGO_ADAPTIVE_TRADITIONAL = 5,
  FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE = 6,
  FREESTYLE_ALGO_ADAPTIVE_CUMULATIVE = 7,
};

typedef struct FreestyleLineSet {
  struct FreestyleLineSet *next, *prev;

  /** Line set name, MAX_NAME. */
  char name[64];
  int flags;

  /** Selection criteria. */
  int selection;
  /** Quantitative invisibility. */
  short qi;
  char _pad1[2];
  int qi_start, qi_end;
  /** Feature edge types. */
  int edge_types, exclude_edge_types;
  char _pad2[4];
  /** Group of target objects. */
  struct Collection *group;

  struct FreestyleLineStyle *linestyle;
} FreestyleLineSet;

typedef struct FreestyleModuleConfig {
  struct FreestyleModuleConfig *next, *prev;

  struct Text *script;
  short is_displayed;
  char _pad[6];
} FreestyleModuleConfig;

typedef struct FreestyleConfig {
  ListBase modules;

  /** Scripting, editor. */
  int mode;
  int raycasting_algorithm DNA_DEPRECATED;
  /** Suggestive contours, ridges/valleys, material boundaries. */
  int flags;
  float sphere_radius;
  float dkr_epsilon;
  /** In radians. */
  float crease_angle;

  ListBase linesets;
} FreestyleConfig;

#ifdef __cplusplus
}
#endif
