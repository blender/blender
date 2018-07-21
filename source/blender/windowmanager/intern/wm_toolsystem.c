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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_toolsystem.c
 *  \ingroup wm
 *
 * Experimental tool-system>
 */

#include <string.h>

#include "CLG_log.h"

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_listbase.h"

#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_paint.h"
#include "BKE_idprop.h"
#include "BKE_workspace.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"
#include "WM_toolsystem.h"  /* own include */

static void toolsystem_reinit_with_toolref(
        bContext *C, WorkSpace *UNUSED(workspace), bToolRef *tref);
static void toolsystem_reinit_ensure_toolref(
        bContext *C, WorkSpace *workspace, const bToolKey *tkey, const char *default_tool);
static void toolsystem_refresh_screen_from_active_tool(
        Main *bmain, WorkSpace *workspace, bToolRef *tref);

/* -------------------------------------------------------------------- */
/** \name Tool Reference API
 * \{ */

struct bToolRef *WM_toolsystem_ref_from_context(struct bContext *C)
{
	WorkSpace *workspace = CTX_wm_workspace(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	ScrArea *sa = CTX_wm_area(C);
	const bToolKey tkey = {
		.space_type = sa->spacetype,
		.mode = WM_toolsystem_mode_from_spacetype(view_layer, sa, sa->spacetype),
	};
	bToolRef *tref = WM_toolsystem_ref_find(workspace, &tkey);
	/* We could return 'sa->runtime.tool' in this case. */
	if (sa->runtime.is_tool_set) {
		BLI_assert(tref == sa->runtime.tool);
	}
	return tref;
}

struct bToolRef_Runtime *WM_toolsystem_runtime_from_context(struct bContext *C)
{
	bToolRef *tref = WM_toolsystem_ref_from_context(C);
	return tref ? tref->runtime : NULL;
}

bToolRef *WM_toolsystem_ref_find(WorkSpace *workspace, const bToolKey *tkey)
{
	LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
		if ((tref->space_type == tkey->space_type) &&
		    (tref->mode == tkey->mode))
		{
			return tref;
		}
	}
	return NULL;
}

bToolRef_Runtime *WM_toolsystem_runtime_find(WorkSpace *workspace, const bToolKey *tkey)
{
	bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
	return tref ? tref->runtime : NULL;
}

bool WM_toolsystem_ref_ensure(
        struct WorkSpace *workspace, const bToolKey *tkey,
        bToolRef **r_tref)
{
	bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
	if (tref) {
		*r_tref = tref;
		return false;
	}
	tref = MEM_callocN(sizeof(*tref), __func__);
	BLI_addhead(&workspace->tools, tref);
	tref->space_type = tkey->space_type;
	tref->mode = tkey->mode;
	*r_tref = tref;
	return true;
}

/** \} */


static void toolsystem_unlink_ref(bContext *C, WorkSpace *workspace, bToolRef *tref)
{
	bToolRef_Runtime *tref_rt = tref->runtime;

	if (tref_rt->gizmo_group[0]) {
		wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(tref_rt->gizmo_group, false);
		if (gzgt != NULL) {
			bool found = false;

			/* TODO(campbell) */
			Main *bmain = CTX_data_main(C);
#if 0
			wmWindowManager *wm = bmain->wm.first;
			/* Check another workspace isn't using this tool. */
			for (wmWindow *win = wm->windows.first; win; win = win->next) {
				const WorkSpace *workspace_iter = WM_window_get_active_workspace(win);
				if (workspace != workspace_iter) {
					if (STREQ(workspace->tool.gizmo_group, workspace_iter->tool.gizmo_group)) {
						found = true;
						break;
					}
				}
			}
#else
			UNUSED_VARS(workspace);
#endif
			if (!found) {
				wmGizmoMapType *gzmap_type = WM_gizmomaptype_ensure(&gzgt->gzmap_params);
				WM_gizmomaptype_group_unlink(C, bmain, gzmap_type, gzgt);
			}
		}
	}
}
void WM_toolsystem_unlink(bContext *C, WorkSpace *workspace, const bToolKey *tkey)
{
	bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
	if (tref && tref->runtime) {
		toolsystem_unlink_ref(C, workspace, tref);
	}
}

static void toolsystem_ref_link(bContext *C, WorkSpace *workspace, bToolRef *tref)
{
	bToolRef_Runtime *tref_rt = tref->runtime;
	if (tref_rt->gizmo_group[0]) {
		const char *idname = tref_rt->gizmo_group;
		wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(idname, false);
		if (gzgt != NULL) {
			WM_gizmo_group_type_ensure_ptr(gzgt);
		}
		else {
			CLOG_WARN(WM_LOG_TOOLS, "'%s' widget not found", idname);
		}
	}

	if (tref_rt->data_block[0]) {
		Main *bmain = CTX_data_main(C);

		/* Currently only brush data-blocks supported. */
		struct Brush *brush = (struct Brush *)BKE_libblock_find_name(bmain, ID_BR, tref_rt->data_block);

		if (brush) {
			wmWindowManager *wm = bmain->wm.first;
			for (wmWindow *win = wm->windows.first; win; win = win->next) {
				if (workspace == WM_window_get_active_workspace(win)) {
					Scene *scene = WM_window_get_active_scene(win);
					ViewLayer *view_layer = WM_window_get_active_view_layer(win);
					Paint *paint = BKE_paint_get_active(scene, view_layer);
					if (paint) {
						if (brush) {
							BKE_paint_brush_set(paint, brush);
						}
					}
				}
			}
		}
	}
}

static void toolsystem_refresh_ref(bContext *C, WorkSpace *workspace, bToolRef *tref)
{
	if (tref->runtime == NULL) {
		return;
	}
	/* currently same operation. */
	toolsystem_ref_link(C, workspace, tref);
}
void WM_toolsystem_refresh(bContext *C, WorkSpace *workspace, const bToolKey *tkey)
{
	bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
	if (tref) {
		toolsystem_refresh_ref(C, workspace, tref);
	}
}

static void toolsystem_reinit_ref(bContext *C, WorkSpace *workspace, bToolRef *tref)
{
	toolsystem_reinit_with_toolref(C, workspace, tref);
}
void WM_toolsystem_reinit(bContext *C, WorkSpace *workspace, const bToolKey *tkey)
{
	bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);
	if (tref) {
		toolsystem_reinit_ref(C, workspace, tref);
	}
}

/* Operate on all active tools. */
void WM_toolsystem_unlink_all(struct bContext *C, struct WorkSpace *workspace)
{
	LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
		tref->tag = 0;
	}

	LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
		if (tref->runtime) {
			if (tref->tag == 0) {
				toolsystem_unlink_ref(C, workspace, tref);
				tref->tag = 1;
			}
		}
	}
}

void WM_toolsystem_refresh_all(struct bContext *C, struct WorkSpace *workspace)
{
	BLI_assert(0);
	LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
		toolsystem_refresh_ref(C, workspace, tref);
	}
}
void WM_toolsystem_reinit_all(struct bContext *C, wmWindow *win)
{
	bScreen *screen = WM_window_get_active_screen(win);
	ViewLayer *view_layer = WM_window_get_active_view_layer(win);
	for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
		WorkSpace *workspace = WM_window_get_active_workspace(win);
		const bToolKey tkey = {
			.space_type = sa->spacetype,
			.mode = WM_toolsystem_mode_from_spacetype(view_layer, sa, sa->spacetype),
		};
		bToolRef *tref = WM_toolsystem_ref_find(workspace, &tkey);
		if (tref) {
			if (tref->tag == 0) {
				toolsystem_reinit_ref(C, workspace, tref);
				tref->tag = 1;
			}
		}
	}
}

void WM_toolsystem_ref_set_from_runtime(
        struct bContext *C, struct WorkSpace *workspace, bToolRef *tref,
        const bToolRef_Runtime *tref_rt, const char *idname)
{
	Main *bmain = CTX_data_main(C);

	if (tref->runtime) {
		toolsystem_unlink_ref(C, workspace, tref);
	}

	STRNCPY(tref->idname, idname);

	/* BAD DESIGN WARNING: used for topbar. */
	workspace->tools_space_type = tref->space_type;
	workspace->tools_mode = tref->mode;

	if (tref->runtime == NULL) {
		tref->runtime = MEM_callocN(sizeof(*tref->runtime), __func__);
	}

	if (tref_rt != tref->runtime) {
		*tref->runtime = *tref_rt;
	}

	toolsystem_ref_link(C, workspace, tref);

	toolsystem_refresh_screen_from_active_tool(bmain, workspace, tref);

	{
		struct wmMsgBus *mbus = CTX_wm_message_bus(C);
		WM_msg_publish_rna_prop(
		        mbus, &workspace->id, workspace, WorkSpace, tools);
	}
}

void WM_toolsystem_init(bContext *C)
{
	Main *bmain = CTX_data_main(C);

	BLI_assert(CTX_wm_window(C) == NULL);

	LISTBASE_FOREACH (WorkSpace *, workspace, &bmain->workspaces) {
		LISTBASE_FOREACH (bToolRef *, tref, &workspace->tools) {
			MEM_SAFE_FREE(tref->runtime);
			tref->tag = 0;
		}
	}

	for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
		for (wmWindow *win = wm->windows.first; win; win = win->next) {
			CTX_wm_window_set(C, win);
			WorkSpace *workspace = WM_window_get_active_workspace(win);
			bScreen *screen = WM_window_get_active_screen(win);
			ViewLayer *view_layer = WM_window_get_active_view_layer(win);
			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				const bToolKey tkey = {
					.space_type = sa->spacetype,
					.mode = WM_toolsystem_mode_from_spacetype(view_layer, sa, sa->spacetype),
				};
				bToolRef *tref = WM_toolsystem_ref_find(workspace, &tkey);
				if (tref) {
					if (tref->tag == 0) {
						toolsystem_reinit_ref(C, workspace, tref);
						tref->tag = 1;
					}
				}
			}
			CTX_wm_window_set(C, NULL);
		}
	}
}

int WM_toolsystem_mode_from_spacetype(
        ViewLayer *view_layer, ScrArea *sa, int spacetype)
{
	int mode = -1;
	switch (spacetype) {
		case SPACE_VIEW3D:
		{
			/* 'sa' may be NULL in this case. */
			Object *obact = OBACT(view_layer);
			if (obact != NULL) {
				Object *obedit = OBEDIT_FROM_OBACT(obact);
				mode = CTX_data_mode_enum_ex(obedit, obact, obact->mode);
			}
			else {
				mode = CTX_MODE_OBJECT;
			}
			break;
		}
		case SPACE_IMAGE:
		{
			SpaceImage *sima = sa->spacedata.first;
			mode = sima->mode;
			break;
		}
	}
	return mode;
}

bool WM_toolsystem_key_from_context(
        ViewLayer *view_layer, ScrArea *sa, bToolKey *tkey)
{
	int space_type = SPACE_EMPTY;
	int mode = -1;

	if (sa != NULL) {
		space_type = sa->spacetype;
		mode = WM_toolsystem_mode_from_spacetype(view_layer, sa, space_type);
	}

	if (mode != -1) {
		tkey->space_type = space_type;
		tkey->mode = mode;
		return true;
	}
	return false;
}

void WM_toolsystem_refresh_screen_area(WorkSpace *workspace, ViewLayer *view_layer, ScrArea *sa)
{
	sa->runtime.tool = NULL;
	sa->runtime.is_tool_set = true;
	const int mode = WM_toolsystem_mode_from_spacetype(view_layer, sa, sa->spacetype);
	for (bToolRef *tref = workspace->tools.first; tref; tref = tref->next) {
		if (tref->space_type == sa->spacetype) {
			if (tref->mode == mode) {
				sa->runtime.tool = tref;
				break;
			}
		}
	}
}

void WM_toolsystem_refresh_screen_all(Main *bmain)
{
	/* Update all ScrArea's tools */
	for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
		for (wmWindow *win = wm->windows.first; win; win = win->next) {
			WorkSpace *workspace = WM_window_get_active_workspace(win);
			bool space_type_has_tools[SPACE_TYPE_LAST + 1] = {0};
			for (bToolRef *tref = workspace->tools.first; tref; tref = tref->next) {
				space_type_has_tools[tref->space_type] = true;
			}
			bScreen *screen = WM_window_get_active_screen(win);
			ViewLayer *view_layer = WM_window_get_active_view_layer(win);
			for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
				sa->runtime.tool = NULL;
				sa->runtime.is_tool_set = true;
				if (space_type_has_tools[sa->spacetype]) {
					WM_toolsystem_refresh_screen_area(workspace, view_layer, sa);
				}
			}
		}
	}
}

static void toolsystem_refresh_screen_from_active_tool(
        Main *bmain, WorkSpace *workspace, bToolRef *tref)
{
	/* Update all ScrArea's tools */
	for (wmWindowManager *wm = bmain->wm.first; wm; wm = wm->id.next) {
		for (wmWindow *win = wm->windows.first; win; win = win->next) {
			if (workspace == WM_window_get_active_workspace(win)) {
				bScreen *screen = WM_window_get_active_screen(win);
				ViewLayer *view_layer = WM_window_get_active_view_layer(win);
				for (ScrArea *sa = screen->areabase.first; sa; sa = sa->next) {
					if (sa->spacetype == tref->space_type) {
						int mode = WM_toolsystem_mode_from_spacetype(view_layer, sa, sa->spacetype);
						if (mode == tref->mode) {
							sa->runtime.tool = tref;
							sa->runtime.is_tool_set = true;
						}
					}
				}
			}
		}
	}
}

bToolRef *WM_toolsystem_ref_set_by_name(
        bContext *C, WorkSpace *workspace, const bToolKey *tkey,
        const char *name, bool cycle)
{
	wmOperatorType *ot = WM_operatortype_find("WM_OT_tool_set_by_name", false);
	/* On startup, Python operatores are not yet loaded. */
	if (ot == NULL) {
		return NULL;
	}
	PointerRNA op_props;
	WM_operator_properties_create_ptr(&op_props, ot);
	RNA_string_set(&op_props, "name", name);

	/* Will get from context if not set. */
	bToolKey tkey_from_context;
	if (tkey == NULL) {
		ViewLayer *view_layer = CTX_data_view_layer(C);
		ScrArea *sa = CTX_wm_area(C);
		WM_toolsystem_key_from_context(view_layer, sa, &tkey_from_context);
		tkey = &tkey_from_context;
	}

	RNA_enum_set(&op_props, "space_type", tkey->space_type);
	RNA_boolean_set(&op_props, "cycle", cycle);

	WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &op_props);
	WM_operator_properties_free(&op_props);

	bToolRef *tref = WM_toolsystem_ref_find(workspace, tkey);

	if (tref) {
		Main *bmain = CTX_data_main(C);
		toolsystem_refresh_screen_from_active_tool(bmain, workspace, tref);
	}

	return (tref && STREQ(tref->idname, name)) ? tref : NULL;
}

static void toolsystem_reinit_with_toolref(
        bContext *C, WorkSpace *workspace, bToolRef *tref)
{
	bToolKey tkey = {
		.space_type = tref->space_type,
		.mode = tref->mode,
	};
	WM_toolsystem_ref_set_by_name(C, workspace, &tkey, tref->idname, false);
}

/**
 * Run after changing modes.
 */
static void toolsystem_reinit_ensure_toolref(
        bContext *C, WorkSpace *workspace, const bToolKey *tkey, const char *default_tool)
{
	bToolRef *tref;
	if (WM_toolsystem_ref_ensure(workspace, tkey, &tref)) {
		STRNCPY(tref->idname, default_tool);
	}

	toolsystem_reinit_with_toolref(C, workspace, tref);
}

void WM_toolsystem_update_from_context_view3d(bContext *C)
{
	WorkSpace *workspace = CTX_wm_workspace(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	int space_type = SPACE_VIEW3D;
	const bToolKey tkey = {
		.space_type = space_type,
		.mode = WM_toolsystem_mode_from_spacetype(view_layer, NULL, space_type),
	};
	toolsystem_reinit_ensure_toolref(C, workspace, &tkey, "Cursor");
}

/**
 * For paint modes to support non-brush tools.
 */
bool WM_toolsystem_active_tool_is_brush(const bContext *C)
{
	bToolRef_Runtime *tref_rt = WM_toolsystem_runtime_from_context((bContext *)C);
	return tref_rt && (tref_rt->data_block[0] != '\0');
}

/* Follow wmMsgNotifyFn spec */
void WM_toolsystem_do_msg_notify_tag_refresh(
        bContext *C, wmMsgSubscribeKey *UNUSED(msg_key), wmMsgSubscribeValue *msg_val)
{
	WorkSpace *workspace = CTX_wm_workspace(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	ScrArea *sa = msg_val->user_data;
	int space_type = sa->spacetype;
	const bToolKey tkey = {
		.space_type = space_type,
		.mode = WM_toolsystem_mode_from_spacetype(view_layer, sa, sa->spacetype),
	};
	WM_toolsystem_refresh(C, workspace, &tkey);
}

IDProperty *WM_toolsystem_ref_properties_ensure_idprops(bToolRef *tref)
{
	if (tref->properties == NULL) {
		IDPropertyTemplate val = {0};
		tref->properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
	}
	return tref->properties;
}

void WM_toolsystem_ref_properties_ensure(bToolRef *tref, wmOperatorType *ot, PointerRNA *ptr)
{
	IDProperty *group = WM_toolsystem_ref_properties_ensure_idprops(tref);
	IDProperty *prop = IDP_GetPropertyFromGroup(group, ot->idname);
	if (prop == NULL) {
		IDPropertyTemplate val = {0};
		prop = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
		STRNCPY(prop->name, ot->idname);
		IDP_ReplaceInGroup_ex(group, prop, NULL);
	}
	else {
		BLI_assert(prop->type == IDP_GROUP);
	}

	RNA_pointer_create(NULL, ot->srna, prop, ptr);
}

void WM_toolsystem_ref_properties_init_for_keymap(
        bToolRef *tref, PointerRNA *dst_ptr, PointerRNA *src_ptr, wmOperatorType *ot)
{
	*dst_ptr = *src_ptr;
	if (dst_ptr->data) {
		dst_ptr->data = IDP_CopyProperty(dst_ptr->data);
	}
	else {
		IDPropertyTemplate val = {0};
		dst_ptr->data = IDP_New(IDP_GROUP, &val, "wmOpItemProp");
	}
	if (tref->properties != NULL) {
		IDProperty *prop = IDP_GetPropertyFromGroup(tref->properties, ot->idname);
		if (prop) {
			IDP_MergeGroup(dst_ptr->data, prop, true);
		}
	}
}
