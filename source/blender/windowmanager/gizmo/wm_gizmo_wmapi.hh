/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Gizmos Window Manager API
 * API for usage in window manager code only. It should contain all functionality
 * needed to hook up the gizmo system with Blender's window manager. It's
 * mostly the event system that needs to communicate with gizmo code.
 *
 * Only included in `wm.hh` and lower level files.
 */

#pragma once

struct wmEventHandler_Gizmo;
struct wmEventHandler_Op;
struct wmGizmoMap;
struct wmOperatorType;

/* -------------------------------------------------------------------- */
/** \name #wmGizmo
 * \{ */

/* `wm_gizmo_type.cc`, for init/exit */

void wm_gizmotype_free();
/**
 * Called on initialize #WM_init().
 */
void wm_gizmotype_init();

/* wm_gizmogroup_type.c, for init/exit */

void wm_gizmogrouptype_free();
/**
 * Called on initialize #WM_init().
 */
void wm_gizmogrouptype_init();

/** \} */

/* -------------------------------------------------------------------- */
/** \name #wmGizmoGroup
 * \{ */

void GIZMOGROUP_OT_gizmo_select(wmOperatorType *ot);
void GIZMOGROUP_OT_gizmo_tweak(wmOperatorType *ot);

bool wm_gizmogroup_is_any_selected(const wmGizmoGroup *gzgroup);

/** \} */

/* -------------------------------------------------------------------- */
/** \name #wmGizmoMap
 * \{ */

void wm_gizmomap_remove(wmGizmoMap *gzmap);

/**
 * Initialize key-maps for all existing gizmo-groups
 */
void wm_gizmos_keymap(wmKeyConfig *keyconf);

void wm_gizmomaps_handled_modal_update(bContext *C, wmEvent *event, wmEventHandler_Op *handler);
/**
 * Prepare context for gizmo handling (but only if area/region is
 * part of screen). Version of #wm_handler_op_context for gizmos.
 */
void wm_gizmomap_handler_context_op(bContext *C, wmEventHandler_Op *handler);
void wm_gizmomap_handler_context_gizmo(bContext *C, wmEventHandler_Gizmo *handler);

/**
 * Try to find a gizmo under the mouse position. 2D intersections have priority over
 * 3D ones (could check for smallest screen-space distance but not needed right now).
 */
wmGizmo *wm_gizmomap_highlight_find(wmGizmoMap *gzmap,
                                    bContext *C,
                                    const wmEvent *event,
                                    int *r_part);
bool wm_gizmomap_highlight_set(wmGizmoMap *gzmap, const bContext *C, wmGizmo *gz, int part);
wmGizmo *wm_gizmomap_highlight_get(wmGizmoMap *gzmap);
/**
 * Caller should call exit when (enable == False).
 */
void wm_gizmomap_modal_set(
    wmGizmoMap *gzmap, bContext *C, wmGizmo *gz, const wmEvent *event, bool enable);

wmGizmo *wm_gizmomap_modal_get(wmGizmoMap *gzmap);
wmGizmo **wm_gizmomap_selected_get(wmGizmoMap *gzmap, int *r_selected_len);
ListBase *wm_gizmomap_groups_get(wmGizmoMap *gzmap);

/** \} */

/* -------------------------------------------------------------------- */
/** \name #wmGizmoMapType
 * \{ */

void wm_gizmomaptypes_free();

/** \} */
