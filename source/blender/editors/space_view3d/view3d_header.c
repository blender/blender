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
 * The Original Code is Copyright (C) 2004-2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_header.c
 *  \ingroup spview3d
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_gpencil_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_screen.h"
#include "BKE_editmesh.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_undo.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"

static void do_view3d_header_buttons(bContext *C, void *arg, int event);

#define B_SEL_VERT  110
#define B_SEL_EDGE  111
#define B_SEL_FACE  112

/* XXX quickly ported across */
static void handle_view3d_lock(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = CTX_wm_view3d(C);

	if (v3d != NULL && sa != NULL) {
		if (v3d->localvd == NULL && v3d->scenelock && sa->spacetype == SPACE_VIEW3D) {
			/* copy to scene */
			scene->lay = v3d->lay;
			scene->layact = v3d->layact;
			scene->camera = v3d->camera;

			/* notifiers for scene update */
			WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);
		}
	}
}

/**
 * layer code is on three levels actually:
 * - here for operator
 * - uiTemplateLayers in interface/ code for buttons
 * - ED_view3d_view_layer_set for RNA
 */
static void view3d_layers_editmode_ensure(View3D *v3d, Object *obedit)
{
	/* sanity check - when in editmode disallow switching the editmode layer off since its confusing
	 * an alternative would be to always draw the editmode object. */
	if (obedit && (obedit->lay & v3d->lay) == 0) {
		int bit;
		for (bit = 0; bit < 32; bit++) {
			if (obedit->lay & (1u << bit)) {
				v3d->lay |= (1u << bit);
				break;
			}
		}
	}
}

static int view3d_layers_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	Object *obedit = CTX_data_edit_object(C);
	int nr = RNA_int_get(op->ptr, "nr");
	const bool toggle = RNA_boolean_get(op->ptr, "toggle");

	if (nr < 0)
		return OPERATOR_CANCELLED;

	if (nr == 0) {
		/* all layers */
		if (!v3d->lay_prev)
			v3d->lay_prev = 1;

		if (toggle && v3d->lay == ((1 << 20) - 1)) {
			/* return to active layer only */
			v3d->lay = v3d->lay_prev;

			view3d_layers_editmode_ensure(v3d, obedit);
		}
		else {
			v3d->lay_prev = v3d->lay;
			v3d->lay |= (1 << 20) - 1;
		}
	}
	else {
		int bit;
		nr--;

		if (RNA_boolean_get(op->ptr, "extend")) {
			if (toggle && v3d->lay & (1 << nr) && (v3d->lay & ~(1 << nr)))
				v3d->lay &= ~(1 << nr);
			else
				v3d->lay |= (1 << nr);
		}
		else {
			v3d->lay = (1 << nr);
		}

		view3d_layers_editmode_ensure(v3d, obedit);

		/* set active layer, ensure to always have one */
		if (v3d->lay & (1 << nr))
			v3d->layact = 1 << nr;
		else if ((v3d->lay & v3d->layact) == 0) {
			for (bit = 0; bit < 32; bit++) {
				if (v3d->lay & (1u << bit)) {
					v3d->layact = (1u << bit);
					break;
				}
			}
		}
	}

	if (v3d->scenelock) handle_view3d_lock(C);

	DEG_on_visible_update(CTX_data_main(C), false);

	ED_area_tag_redraw(sa);

	return OPERATOR_FINISHED;
}

/* applies shift and alt, lazy coding or ok? :) */
/* the local per-keymap-entry keymap will solve it */
static int view3d_layers_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (event->ctrl || event->oskey)
		return OPERATOR_PASS_THROUGH;

	if (event->shift)
		RNA_boolean_set(op->ptr, "extend", true);
	else
		RNA_boolean_set(op->ptr, "extend", false);

	if (event->alt) {
		const int nr = RNA_int_get(op->ptr, "nr") + 10;
		RNA_int_set(op->ptr, "nr", nr);
	}
	view3d_layers_exec(C, op);

	return OPERATOR_FINISHED;
}

static bool view3d_layers_poll(bContext *C)
{
	return (ED_operator_view3d_active(C) && CTX_wm_view3d(C)->localvd == NULL);
}

void VIEW3D_OT_layers(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Layers";
	ot->description = "Toggle layer(s) visibility";
	ot->idname = "VIEW3D_OT_layers";

	/* api callbacks */
	ot->invoke = view3d_layers_invoke;
	ot->exec = view3d_layers_exec;
	ot->poll = view3d_layers_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "nr", 1, 0, 20, "Number", "The layer number to set, zero for all layers", 0, 20);
	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Add this layer to the current view layers");
	RNA_def_boolean(ot->srna, "toggle", 1, "Toggle", "Toggle the layer");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Toggle Matcap Flip Operator
 * \{ */

static int toggle_matcap_flip(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);
	v3d->shading.flag ^= V3D_SHADING_MATCAP_FLIP_X;
	ED_view3d_shade_update(CTX_data_main(C), v3d, CTX_wm_area(C));
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	return OPERATOR_FINISHED;
}

void VIEW3D_OT_toggle_matcap_flip(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip MatCap";
	ot->description = "Flip MatCap";
	ot->idname = "VIEW3D_OT_toggle_matcap_flip";

	/* api callbacks */
	ot->exec = toggle_matcap_flip;
	ot->poll = ED_operator_view3d_active;
}

/** \} */


static void do_view3d_header_buttons(bContext *C, void *UNUSED(arg), int event)
{
	wmWindow *win = CTX_wm_window(C);
	const int ctrl = win->eventstate->ctrl, shift = win->eventstate->shift;

	/* watch it: if sa->win does not exist, check that when calling direct drawing routines */

	switch (event) {
		case B_SEL_VERT:
			if (EDBM_selectmode_toggle(C, SCE_SELECT_VERTEX, -1, shift, ctrl)) {
				ED_undo_push(C, "Selectmode Set: Vertex");
			}
			break;
		case B_SEL_EDGE:
			if (EDBM_selectmode_toggle(C, SCE_SELECT_EDGE, -1, shift, ctrl)) {
				ED_undo_push(C, "Selectmode Set: Edge");
			}
			break;
		case B_SEL_FACE:
			if (EDBM_selectmode_toggle(C, SCE_SELECT_FACE, -1, shift, ctrl)) {
				ED_undo_push(C, "Selectmode Set: Face");
			}
			break;
		default:
			break;
	}
}

void uiTemplateEditModeSelection(uiLayout *layout, struct bContext *C)
{
	Object *obedit = CTX_data_edit_object(C);
	uiBlock *block = uiLayoutGetBlock(layout);

	UI_block_func_handle_set(block, do_view3d_header_buttons, NULL);

	if (obedit && (obedit->type == OB_MESH)) {
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		uiLayout *row;

		row = uiLayoutRow(layout, true);
		block = uiLayoutGetBlock(row);
		uiDefIconButBitS(block, UI_BTYPE_TOGGLE, SCE_SELECT_VERTEX, B_SEL_VERT, ICON_VERTEXSEL,
		                 0, 0, UI_UNIT_X, UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0,
		                 TIP_("Vertex select - Shift-Click for multiple modes, Ctrl-Click contracts selection"));
		uiDefIconButBitS(block, UI_BTYPE_TOGGLE, SCE_SELECT_EDGE, B_SEL_EDGE, ICON_EDGESEL,
		                 0, 0, UI_UNIT_X, UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0,
		                 TIP_("Edge select - Shift-Click for multiple modes, Ctrl-Click expands/contracts selection"));
		uiDefIconButBitS(block, UI_BTYPE_TOGGLE, SCE_SELECT_FACE, B_SEL_FACE, ICON_FACESEL,
		                 0, 0, UI_UNIT_X, UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0,
		                 TIP_("Face select - Shift-Click for multiple modes, Ctrl-Click expands selection"));
	}
}

static void uiTemplatePaintModeSelection(uiLayout *layout, struct bContext *C)
{
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object *ob = OBACT(view_layer);

	/* Gizmos aren't used in paint modes */
	if (!ELEM(ob->mode, OB_MODE_SCULPT, OB_MODE_PARTICLE_EDIT)) {
		/* masks aren't used for sculpt and particle painting */
		PointerRNA meshptr;

		RNA_pointer_create(ob->data, &RNA_Mesh, ob->data, &meshptr);
		if (ob->mode & (OB_MODE_TEXTURE_PAINT)) {
			uiItemR(layout, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		}
		else {
			uiLayout *row = uiLayoutRow(layout, true);
			uiItemR(row, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			if (ob->mode & OB_MODE_WEIGHT_PAINT) {
				uiItemR(row, &meshptr, "use_paint_mask_vertex", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			}
		}
	}
}

void uiTemplateHeader3D_mode(uiLayout *layout, struct bContext *C)
{
	/* Extracted from: uiTemplateHeader3D */
	ViewLayer *view_layer = CTX_data_view_layer(C);
	Object *ob = OBACT(view_layer);
	Object *obedit = CTX_data_edit_object(C);
	bGPdata *gpd = CTX_data_gpencil_data(C);

	bool is_paint = (
	        ob && !(gpd && (gpd->flag & GP_DATA_STROKE_EDITMODE)) &&
	        ELEM(ob->mode,
	             OB_MODE_SCULPT, OB_MODE_VERTEX_PAINT, OB_MODE_WEIGHT_PAINT, OB_MODE_TEXTURE_PAINT));

	uiTemplateEditModeSelection(layout, C);
	if ((obedit == NULL) && is_paint) {
		uiTemplatePaintModeSelection(layout, C);
	}
}

void uiTemplateHeader3D(uiLayout *layout, struct bContext *C)
{
	bScreen *screen = CTX_wm_screen(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	Scene *scene = CTX_data_scene(C);
	ViewLayer *view_layer = CTX_data_view_layer(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	PointerRNA v3dptr, toolsptr, sceneptr;
	Object *ob = OBACT(view_layer);
	Object *obedit = CTX_data_edit_object(C);
	bGPdata *gpd = CTX_data_gpencil_data(C);
	uiBlock *block;
	bool is_paint = (
	        ob && !(gpd && (gpd->flag & GP_DATA_STROKE_EDITMODE)) &&
	        ELEM(ob->mode,
	             OB_MODE_SCULPT, OB_MODE_VERTEX_PAINT, OB_MODE_WEIGHT_PAINT, OB_MODE_TEXTURE_PAINT));

	RNA_pointer_create(&screen->id, &RNA_SpaceView3D, v3d, &v3dptr);
	RNA_pointer_create(&scene->id, &RNA_ToolSettings, ts, &toolsptr);
	RNA_pointer_create(&scene->id, &RNA_Scene, scene, &sceneptr);

	block = uiLayoutGetBlock(layout);
	UI_block_func_handle_set(block, do_view3d_header_buttons, NULL);

	/* other buttons: */
	UI_block_emboss_set(block, UI_EMBOSS);

	/* moved to topbar */
#if 0
	uiLayout *row = uiLayoutRow(layout, true);
	uiItemR(row, &v3dptr, "pivot_point", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	if (!ob || ELEM(ob->mode, OB_MODE_OBJECT, OB_MODE_POSE, OB_MODE_WEIGHT_PAINT)) {
		uiItemR(row, &v3dptr, "use_pivot_point_align", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	}
#endif

	if (obedit == NULL && is_paint) {
		/* Currently Python calls this directly. */
#if 0
		uiTemplatePaintModeSelection(layout, C);
#endif

	}
	else {
		/* Moved to popover and topbar. */
#if 0
		/* Transform widget / gizmos */
		row = uiLayoutRow(layout, true);
		uiItemR(row, &v3dptr, "show_gizmo", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		uiItemR(row, &sceneptr, "transform_orientation", 0, "", ICON_NONE);
#endif
	}

	if (obedit == NULL && v3d->localvd == NULL) {
		/* Scene lock */
		uiItemR(layout, &v3dptr, "lock_camera_and_layers", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	}

	/* Currently Python calls this directly. */
#if 0
	uiTemplateEditModeSelection(layout, C);
#endif
}
