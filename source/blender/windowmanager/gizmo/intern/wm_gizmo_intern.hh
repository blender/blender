/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

struct BLI_Buffer;
struct wmGizmoMap;
struct wmKeyConfig;

#include "wm_gizmo_fn.hh"

/* -------------------------------------------------------------------- */
/* wmGizmo */

/**
 * Add/Remove \a gizmo to selection.
 * Reallocates memory for selected gizmos so better not call for selecting multiple ones.
 *
 * \return if the selection has changed.
 */
bool wm_gizmo_select_set_ex(
    wmGizmoMap *gzmap, wmGizmo *gz, bool select, bool use_array, bool use_callback);
bool wm_gizmo_select_and_highlight(bContext *C, wmGizmoMap *gzmap, wmGizmo *gz);

void wm_gizmo_calculate_scale(wmGizmo *gz, const bContext *C);
void wm_gizmo_update(wmGizmo *gz, const bContext *C, bool refresh_map);

int wm_gizmo_is_visible(wmGizmo *gz);
enum {
  WM_GIZMO_IS_VISIBLE_UPDATE = (1 << 0),
  WM_GIZMO_IS_VISIBLE_DRAW = (1 << 1),
};

/* -------------------------------------------------------------------- */
/* wmGizmoGroup */

enum {
  TWEAK_MODAL_CANCEL = 1,
  TWEAK_MODAL_CONFIRM,
  TWEAK_MODAL_PRECISION_ON,
  TWEAK_MODAL_PRECISION_OFF,
  TWEAK_MODAL_SNAP_ON,
  TWEAK_MODAL_SNAP_OFF,
};

/**
 * Create a new gizmo-group from \a gzgt.
 */
wmGizmoGroup *wm_gizmogroup_new_from_type(wmGizmoMap *gzmap, wmGizmoGroupType *gzgt);
void wm_gizmogroup_free(bContext *C, wmGizmoGroup *gzgroup);
/**
 * Add \a gizmo to \a gzgroup and make sure its name is unique within the group.
 */
void wm_gizmogroup_gizmo_register(wmGizmoGroup *gzgroup, wmGizmo *gz);
wmGizmoGroup *wm_gizmogroup_find_by_type(const wmGizmoMap *gzmap, const wmGizmoGroupType *gzgt);
wmGizmo *wm_gizmogroup_find_intersected_gizmo(wmWindowManager *wm,
                                              const wmGizmoGroup *gzgroup,
                                              bContext *C,
                                              int event_modifier,
                                              const int mval[2],
                                              int *r_part);
/**
 * Adds all gizmos of \a gzgroup that can be selected to the head of \a listbase.
 * Added items need freeing!
 */
void wm_gizmogroup_intersectable_gizmos_to_list(wmWindowManager *wm,
                                                const wmGizmoGroup *gzgroup,
                                                int event_modifier,
                                                BLI_Buffer *visible_gizmos);
bool wm_gizmogroup_is_visible_in_drawstep(const wmGizmoGroup *gzgroup,
                                          eWM_GizmoFlagMapDrawStep drawstep);

void wm_gizmogrouptype_setup_keymap(wmGizmoGroupType *gzgt, wmKeyConfig *keyconf);

wmKeyMap *wm_gizmogroup_tweak_modal_keymap(wmKeyConfig *keyconf);

/* -------------------------------------------------------------------- */
/* wmGizmoMap */

struct wmGizmoMapSelectState {
  struct wmGizmo **items;
  int len, len_alloc;
};

struct wmGizmoMap {
  wmGizmoMapType *type;
  ListBase groups; /* wmGizmoGroup */

  /* private, update tagging (enum defined in C source). */
  char update_flag[WM_GIZMOMAP_DRAWSTEP_MAX];

  /** Private, true when not yet used. */
  bool is_init;

  /** When set, one of the items in 'groups' has #wmGizmoGroup.tag_remove set. */
  bool tag_remove_group;

  /**
   * \brief Gizmo map runtime context
   *
   * Contains information about this gizmo-map. Currently
   * highlighted gizmo, currently selected gizmos, ...
   */
  struct {
    /* we redraw the gizmo-map when this changes */
    wmGizmo *highlight;
    /* User has clicked this gizmo and it gets all input. */
    wmGizmo *modal;
    /* array for all selected gizmos */
    wmGizmoMapSelectState select;
    /* cursor location at point of entering modal (see: WM_GIZMO_MOVE_CURSOR) */
    int event_xy[2];
    short event_grabcursor;
    /* until we have nice cursor push/pop API. */
    int last_cursor;
  } gzmap_context;
};

/**
 * This is a container for all gizmo types that can be instantiated in a region.
 * (similar to drop-boxes).
 *
 * \note There is only ever one of these for every (area, region) combination.
 */
struct wmGizmoMapType {
  wmGizmoMapType *next, *prev;
  short spaceid, regionid;
  /* types of gizmo-groups for this gizmo-map type */
  ListBase grouptype_refs;

  /* eGizmoMapTypeUpdateFlags */
  eWM_GizmoFlagMapTypeUpdateFlag type_update_flag;
};

void wm_gizmomap_select_array_clear(wmGizmoMap *gzmap);
/**
 * Deselect all selected gizmos in \a gzmap.
 * \return if selection has changed.
 */
bool wm_gizmomap_deselect_all(wmGizmoMap *gzmap);
void wm_gizmomap_select_array_shrink(wmGizmoMap *gzmap, int len_subtract);
void wm_gizmomap_select_array_push_back(wmGizmoMap *gzmap, wmGizmo *gz);
void wm_gizmomap_select_array_remove(wmGizmoMap *gzmap, wmGizmo *gz);
