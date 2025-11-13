/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ListBase;
struct Scene;
struct ScrArea;
struct TimeMarker;
struct bAnimContext;
struct bContext;
struct wmKeyConfig;
struct ARegion;

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
 * Public API for getting markers from the scene & area.
 *
 * \return A #TimeMarker list.
 */
ListBase *ED_scene_markers_get(const bContext *C, Scene *scene);

/**
 * Public API for getting markers from context.
 *
 * \return A #TimeMarker list.
 */
ListBase *ED_context_get_markers(const bContext *C);
ListBase *ED_sequencer_context_get_markers(const bContext *C);
ListBase *ED_scene_markers_get_from_area(Scene *scene, ViewLayer *view_layer, const ScrArea *area);

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
 * \return the marker that is closest to `frame`.
 * Non-empty `markers` is guaranteed to return a marker.
 *
 * \note For selecting, the caller is expected to exclude markers beyond a small threshold.
 */
TimeMarker *ED_markers_find_nearest_marker(ListBase *markers, float frame);
/**
 * Return the time of the marker that occurs on a frame closest to the given time.
 */
int ED_markers_find_nearest_marker_time(ListBase *markers, float x);

void ED_markers_get_minmax(ListBase *markers, short sel, float *r_first, float *r_last);

/**
 * This function makes a list of all the markers. The only_sel
 * argument is used to specify whether only the selected markers
 * are added.
 */
void ED_markers_make_cfra_list(ListBase *markers, ListBase *lb, bool only_selected);

void ED_markers_deselect_all(ListBase *markers, int action);

/**
 * Get the first selected marker.
 */
TimeMarker *ED_markers_get_first_selected(ListBase *markers);

/**
 * Returns true if the marker region is currently visible in the area.
 */
bool ED_markers_region_visible(const ScrArea *area, const ARegion *region);

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
