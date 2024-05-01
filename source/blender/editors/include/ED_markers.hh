/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct Scene;
struct TimeMarker;
struct bAnimContext;
struct bContext;
struct wmKeyConfig;

/* -------------------------------------------------------------------- */
/** \name Drawing API
 * \{ */

/** Flags for drawing markers. */
enum {
  DRAW_MARKERS_LINES = (1 << 0),
  DRAW_MARKERS_LOCAL = (1 << 1),
  DRAW_MARKERS_MARGIN = (1 << 2),
};

/** Draw Scene-Markers in time window. */
void ED_markers_draw(const bContext *C, int flag);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Backend API
 * \{ */

/**
 * Public API for getting markers from context.
 */
ListBase *ED_context_get_markers(const bContext *C);
/**
 * Public API for getting markers from "animation" context.
 */
ListBase *ED_animcontext_get_markers(const bAnimContext *ac);

/**
 * Apply some transformation to markers after the fact
 *
 * \param markers: List of markers to affect - this may or may not be the scene markers list,
 * so don't assume anything.
 * \param scene: Current scene (for getting current frame)
 * \param mode: (TfmMode) transform mode that this transform is for
 * \param value: From the transform code, this is `t->vec[0]`
 * (which is delta transform for grab/extend, and scale factor for scale)
 * \param side: (B/L/R) for 'extend' functionality, which side of current frame to use
 */
int ED_markers_post_apply_transform(
    ListBase *markers, Scene *scene, int mode, float value, char side);

/**
 * Get the marker that is closest to this point.
 * XXX: for select, the min_dist should be small.
 */
TimeMarker *ED_markers_find_nearest_marker(ListBase *markers, float x);
/**
 * Return the time of the marker that occurs on a frame closest to the given time.
 */
int ED_markers_find_nearest_marker_time(ListBase *markers, float x);

void ED_markers_get_minmax(ListBase *markers, short sel, float *first, float *last);

/**
 * This function makes a list of all the markers. The only_sel
 * argument is used to specify whether only the selected markers
 * are added.
 */
void ED_markers_make_cfra_list(ListBase *markers, ListBase *lb, short sel);

void ED_markers_deselect_all(ListBase *markers, int action);

/**
 * Get the first selected marker.
 */
TimeMarker *ED_markers_get_first_selected(ListBase *markers);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operators
 * \{ */

/**
 * Called in `screen_ops.cc`, #ED_operatortypes_screen().
 */
void ED_operatortypes_marker();
/**
 * Called in `screen_ops.cc`, #ED_keymap_screen().
 */
void ED_keymap_marker(wmKeyConfig *keyconf);

/**
 * Debugging only: print debugging prints of list of markers.
 */
void debug_markers_print_list(ListBase *markers);

/** \} */
