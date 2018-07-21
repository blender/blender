/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/gizmo/wm_gizmo_wmapi.h
 *  \ingroup wm
 *
 * \name Gizmos Window Manager API
 * API for usage in window manager code only. It should contain all functionality
 * needed to hook up the gizmo system with Blender's window manager. It's
 * mostly the event system that needs to communicate with gizmo code.
 *
 * Only included in wm.h and lower level files.
 */


#ifndef __WM_GIZMO_WMAPI_H__
#define __WM_GIZMO_WMAPI_H__

struct wmEventHandler;
struct wmGizmoMap;
struct wmOperatorType;
struct wmOperator;


/* -------------------------------------------------------------------- */
/* wmGizmo */

/* wm_gizmo_type.c, for init/exit */
void wm_gizmotype_free(void);
void wm_gizmotype_init(void);

/* wm_gizmogroup_type.c, for init/exit */
void wm_gizmogrouptype_free(void);
void wm_gizmogrouptype_init(void);

/* -------------------------------------------------------------------- */
/* wmGizmoGroup */

void GIZMOGROUP_OT_gizmo_select(struct wmOperatorType *ot);
void GIZMOGROUP_OT_gizmo_tweak(struct wmOperatorType *ot);

bool wm_gizmogroup_is_any_selected(const struct wmGizmoGroup *gzgroup);

/* -------------------------------------------------------------------- */
/* wmGizmoMap */

void wm_gizmomap_remove(struct wmGizmoMap *gzmap);

void wm_gizmos_keymap(struct wmKeyConfig *keyconf);

void wm_gizmomaps_handled_modal_update(
        bContext *C, struct wmEvent *event, struct wmEventHandler *handler);
void wm_gizmomap_handler_context(bContext *C, struct wmEventHandler *handler);

struct wmGizmo *wm_gizmomap_highlight_find(
        struct wmGizmoMap *gzmap, bContext *C, const struct wmEvent *event,
        int *r_part);
bool wm_gizmomap_highlight_set(
        struct wmGizmoMap *gzmap, const bContext *C,
        struct wmGizmo *gz, int part);
struct wmGizmo *wm_gizmomap_highlight_get(struct wmGizmoMap *gzmap);
void wm_gizmomap_modal_set(
        struct wmGizmoMap *gzmap, bContext *C, struct wmGizmo *gz,
        const struct wmEvent *event, bool enable);

struct wmGizmo *wm_gizmomap_modal_get(struct wmGizmoMap *gzmap);
struct wmGizmo **wm_gizmomap_selected_get(wmGizmoMap *gzmap, int *r_selected_len);
struct ListBase *wm_gizmomap_groups_get(wmGizmoMap *gzmap);

/* -------------------------------------------------------------------- */
/* wmGizmoMapType */

void wm_gizmomaptypes_free(void);

#endif  /* __WM_GIZMO_WMAPI_H__ */
