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
 * Copyright (C) 2019 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_vec_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Each control point that makes up the profile.
 * \note The flags use the same enum as Bezier curves, but they aren't guaranteed
 * to have identical functionality, and all types aren't implemented.
 */
typedef struct CurveProfilePoint {
  /** Location of the point, keep together. */
  float x, y;
  /** Flag selection state and others. */
  short flag;
  /** Flags for both handle's type (eBezTriple_Handle auto, vect, free, and aligned supported). */
  char h1, h2;
  /** Handle locations, keep together.
   * \note For now the two handle types are set to the same type in RNA. */
  float h1_loc[2];
  float h2_loc[2];
  char _pad[4];
  /** Runtime pointer to the point's profile for updating the curve with no direct reference. */
  struct CurveProfile *profile;
} CurveProfilePoint;

/** #CurveProfilePoint.flag */
enum {
  PROF_SELECT = (1 << 0),
  PROF_H1_SELECT = (1 << 1),
  PROF_H2_SELECT = (1 << 2),
};

/** Defines a profile. */
typedef struct CurveProfile {
  /** Number of user-added points that define the profile. */
  short path_len;
  /** Number of sampled points. */
  short segments_len;
  /** Preset to use when reset. */
  int preset;
  /** Sequence of points defining the shape of the curve. */
  CurveProfilePoint *path;
  /** Display and evaluation table at higher resolution for curves. */
  CurveProfilePoint *table;
  /** The positions of the sampled points. Used to display a preview of where they will be. */
  CurveProfilePoint *segments;
  /** Flag for mode states, sampling options, etc... */
  int flag;
  /** Used for keeping track how many times the widget is changed. */
  int changed_timestamp;
  /** Widget's current view, and clipping rect (is default rect too). */
  rctf view_rect, clip_rect;
} CurveProfile;

/** #CurveProfile.flag */
enum {
  PROF_USE_CLIP = (1 << 0), /* Keep control points inside bounding rectangle. */
  /* PROF_SYMMETRY_MODE = (1 << 1),         Unused for now. */
  PROF_SAMPLE_STRAIGHT_EDGES = (1 << 2), /* Sample extra points on straight edges. */
  PROF_SAMPLE_EVEN_LENGTHS = (1 << 3),   /* Put segments evenly spaced along the path. */
  PROF_DIRTY_PRESET = (1 << 4),          /* Marks when the dynamic preset has been changed. */
};

typedef enum eCurveProfilePresets {
  PROF_PRESET_LINE = 0,     /* Default simple line between end points. */
  PROF_PRESET_SUPPORTS = 1, /* Support loops for a regular curved profile. */
  PROF_PRESET_CORNICE = 2,  /* Molding type example. */
  PROF_PRESET_CROWN = 3,    /* Second molding example. */
  PROF_PRESET_STEPS = 4,    /* Dynamic number of steps defined by segments_len. */
} eCurveProfilePresets;

#ifdef __cplusplus
}
#endif
