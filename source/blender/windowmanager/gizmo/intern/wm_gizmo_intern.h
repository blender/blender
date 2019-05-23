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
 */

/** \file
 * \ingroup wm
 */

#ifndef __WM_GIZMO_INTERN_H__
#define __WM_GIZMO_INTERN_H__

struct BLI_Buffer;
struct GHashIterator;
struct GizmoGeomInfo;
struct wmGizmoMap;
struct wmKeyConfig;

#include "wm_gizmo_fn.h"

/* -------------------------------------------------------------------- */
/* wmGizmo */

bool wm_gizmo_select_set_ex(
    struct wmGizmoMap *gzmap, struct wmGizmo *gz, bool select, bool use_array, bool use_callback);
bool wm_gizmo_select_and_highlight(bContext *C, struct wmGizmoMap *gzmap, struct wmGizmo *gz);

void wm_gizmo_calculate_scale(struct wmGizmo *gz, const bContext *C);
void wm_gizmo_update(struct wmGizmo *gz, const bContext *C, const bool refresh_map);

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

struct wmGizmoGroup *wm_gizmogroup_new_from_type(struct wmGizmoMap *gzmap,
                                                 struct wmGizmoGroupType *gzgt);
void wm_gizmogroup_free(bContext *C, struct wmGizmoGroup *gzgroup);
void wm_gizmogroup_gizmo_register(struct wmGizmoGroup *gzgroup, struct wmGizmo *gz);
struct wmGizmo *wm_gizmogroup_find_intersected_gizmo(const struct wmGizmoGroup *gzgroup,
                                                     struct bContext *C,
                                                     const struct wmEvent *event,
                                                     int *r_part);
void wm_gizmogroup_intersectable_gizmos_to_list(const struct wmGizmoGroup *gzgroup,
                                                struct BLI_Buffer *visible_gizmos);
bool wm_gizmogroup_is_visible_in_drawstep(const struct wmGizmoGroup *gzgroup,
                                          const eWM_GizmoFlagMapDrawStep drawstep);

void wm_gizmogrouptype_setup_keymap(struct wmGizmoGroupType *gzgt, struct wmKeyConfig *keyconf);

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
 * (similar to dropboxes).
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
bool wm_gizmomap_deselect_all(struct wmGizmoMap *gzmap);
void wm_gizmomap_select_array_shrink(struct wmGizmoMap *gzmap, int len_subtract);
void wm_gizmomap_select_array_push_back(struct wmGizmoMap *gzmap, wmGizmo *gz);
void wm_gizmomap_select_array_remove(struct wmGizmoMap *gzmap, wmGizmo *gz);

#endif
