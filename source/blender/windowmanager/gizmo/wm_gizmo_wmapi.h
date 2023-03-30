/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation */

/** \file
 * \ingroup wm
 *
 * \name Gizmos Window Manager API
 * API for usage in window manager code only. It should contain all functionality
 * needed to hook up the gizmo system with Blender's window manager. It's
 * mostly the event system that needs to communicate with gizmo code.
 *
 * Only included in wm.h and lower level files.
 */

#pragma once

struct wmEventHandler_Gizmo;
struct wmEventHandler_Op;
struct wmGizmoMap;
struct wmOperatorType;

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name #wmGizmo
 * \{ */

/* wm_gizmo_type.c, for init/exit */

void wm_gizmotype_free(void);
/**
 * Called on initialize #WM_init().
 */
void wm_gizmotype_init(void);

/* wm_gizmogroup_type.c, for init/exit */

void wm_gizmogrouptype_free(void);
/**
 * Called on initialize #WM_init().
 */
void wm_gizmogrouptype_init(void);

/** \} */

/* -------------------------------------------------------------------- */
/** \name #wmGizmoGroup
 * \{ */

void GIZMOGROUP_OT_gizmo_select(struct wmOperatorType *ot);
void GIZMOGROUP_OT_gizmo_tweak(struct wmOperatorType *ot);

bool wm_gizmogroup_is_any_selected(const struct wmGizmoGroup *gzgroup);

/** \} */

/* -------------------------------------------------------------------- */
/** \name #wmGizmoMap
 * \{ */

void wm_gizmomap_remove(struct wmGizmoMap *gzmap);

/**
 * Initialize key-maps for all existing gizmo-groups
 */
void wm_gizmos_keymap(struct wmKeyConfig *keyconf);

void wm_gizmomaps_handled_modal_update(bContext *C,
                                       struct wmEvent *event,
                                       struct wmEventHandler_Op *handler);
/**
 * Prepare context for gizmo handling (but only if area/region is
 * part of screen). Version of #wm_handler_op_context for gizmos.
 */
void wm_gizmomap_handler_context_op(bContext *C, struct wmEventHandler_Op *handler);
void wm_gizmomap_handler_context_gizmo(bContext *C, struct wmEventHandler_Gizmo *handler);

/**
 * Try to find a gizmo under the mouse position. 2D intersections have priority over
 * 3D ones (could check for smallest screen-space distance but not needed right now).
 */
struct wmGizmo *wm_gizmomap_highlight_find(struct wmGizmoMap *gzmap,
                                           bContext *C,
                                           const struct wmEvent *event,
                                           int *r_part);
bool wm_gizmomap_highlight_set(struct wmGizmoMap *gzmap,
                               const bContext *C,
                               struct wmGizmo *gz,
                               int part);
struct wmGizmo *wm_gizmomap_highlight_get(struct wmGizmoMap *gzmap);
/**
 * Caller should call exit when (enable == False).
 */
void wm_gizmomap_modal_set(struct wmGizmoMap *gzmap,
                           bContext *C,
                           struct wmGizmo *gz,
                           const struct wmEvent *event,
                           bool enable);

struct wmGizmo *wm_gizmomap_modal_get(struct wmGizmoMap *gzmap);
struct wmGizmo **wm_gizmomap_selected_get(wmGizmoMap *gzmap, int *r_selected_len);
struct ListBase *wm_gizmomap_groups_get(wmGizmoMap *gzmap);

/** \} */

/* -------------------------------------------------------------------- */
/** \name #wmGizmoMapType
 * \{ */

void wm_gizmomaptypes_free(void);

/** \} */

#ifdef __cplusplus
}
#endif
