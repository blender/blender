/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 */

#pragma once

struct BLI_Buffer;
struct wmGizmoMap;
struct wmKeyConfig;

#include "wm_gizmo_fn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/* wmGizmo */

/**
 * Add/Remove \a gizmo to selection.
 * Reallocates memory for selected gizmos so better not call for selecting multiple ones.
 *
 * \return if the selection has changed.
 */
bool wm_gizmo_select_set_ex(
    struct wmGizmoMap *gzmap, struct wmGizmo *gz, bool select, bool use_array, bool use_callback);
bool wm_gizmo_select_and_highlight(bContext *C, struct wmGizmoMap *gzmap, struct wmGizmo *gz);

void wm_gizmo_calculate_scale(struct wmGizmo *gz, const bContext *C);
void wm_gizmo_update(struct wmGizmo *gz, const bContext *C, bool refresh_map);

int wm_gizmo_is_visible(struct wmGizmo *gz);
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
struct wmGizmoGroup *wm_gizmogroup_new_from_type(struct wmGizmoMap *gzmap,
                                                 struct wmGizmoGroupType *gzgt);
void wm_gizmogroup_free(bContext *C, struct wmGizmoGroup *gzgroup);
/**
 * Add \a gizmo to \a gzgroup and make sure its name is unique within the group.
 */
void wm_gizmogroup_gizmo_register(struct wmGizmoGroup *gzgroup, struct wmGizmo *gz);
struct wmGizmoGroup *wm_gizmogroup_find_by_type(const struct wmGizmoMap *gzmap,
                                                const struct wmGizmoGroupType *gzgt);
struct wmGizmo *wm_gizmogroup_find_intersected_gizmo(wmWindowManager *wm,
                                                     const struct wmGizmoGroup *gzgroup,
                                                     struct bContext *C,
                                                     int event_modifier,
                                                     const int mval[2],
                                                     int *r_part);
/**
 * Adds all gizmos of \a gzgroup that can be selected to the head of \a listbase.
 * Added items need freeing!
 */
void wm_gizmogroup_intersectable_gizmos_to_list(wmWindowManager *wm,
                                                const struct wmGizmoGroup *gzgroup,
                                                int event_modifier,
                                                struct BLI_Buffer *visible_gizmos);
bool wm_gizmogroup_is_visible_in_drawstep(const struct wmGizmoGroup *gzgroup,
                                          eWM_GizmoFlagMapDrawStep drawstep);

void wm_gizmogrouptype_setup_keymap(struct wmGizmoGroupType *gzgt, struct wmKeyConfig *keyconf);

wmKeyMap *wm_gizmogroup_tweak_modal_keymap(struct wmKeyConfig *keyconf);

/* -------------------------------------------------------------------- */
/* wmGizmoMap */

typedef struct wmGizmoMapSelectState {
  struct wmGizmo **items;
  int len, len_alloc;
} wmGizmoMapSelectState;

struct wmGizmoMap {

  struct wmGizmoMapType *type;
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
    struct wmGizmo *highlight;
    /* User has clicked this gizmo and it gets all input. */
    struct wmGizmo *modal;
    /* array for all selected gizmos */
    struct wmGizmoMapSelectState select;
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
  struct wmGizmoMapType *next, *prev;
  short spaceid, regionid;
  /* types of gizmo-groups for this gizmo-map type */
  ListBase grouptype_refs;

  /* eGizmoMapTypeUpdateFlags */
  eWM_GizmoFlagMapTypeUpdateFlag type_update_flag;
};

void wm_gizmomap_select_array_clear(struct wmGizmoMap *gzmap);
/**
 * Deselect all selected gizmos in \a gzmap.
 * \return if selection has changed.
 */
bool wm_gizmomap_deselect_all(struct wmGizmoMap *gzmap);
void wm_gizmomap_select_array_shrink(struct wmGizmoMap *gzmap, int len_subtract);
void wm_gizmomap_select_array_push_back(struct wmGizmoMap *gzmap, wmGizmo *gz);
void wm_gizmomap_select_array_remove(struct wmGizmoMap *gzmap, wmGizmo *gz);

#ifdef __cplusplus
}
#endif
