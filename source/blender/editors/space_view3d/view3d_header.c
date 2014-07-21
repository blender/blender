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

#include "DNA_brush_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_modifier.h"
#include "BKE_paint.h"
#include "BKE_screen.h"
#include "BKE_editmesh.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_util.h"
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
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = CTX_wm_view3d(C);
	
	if (v3d != NULL && sa != NULL) {
		if (v3d->localvd == NULL && v3d->scenelock && sa->spacetype == SPACE_VIEW3D) {
			/* copy to scene */
			scene->lay = v3d->lay;
			scene->layact = v3d->layact;
			scene->camera = v3d->camera;

			/* not through notifier, listener don't have context
			 * and non-open screens or spaces need to be updated too */
			BKE_screen_view3d_main_sync(&bmain->screen, scene);
			
			/* notifiers for scene update */
			WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);
		}
	}
}

/**
 * layer code is on three levels actually:
 * - here for operator
 * - uiTemplateLayers in interface/ code for buttons
 * - ED_view3d_scene_layer_set for RNA
 */
static void view3d_layers_editmode_ensure(Scene *scene, View3D *v3d)
{
	/* sanity check - when in editmode disallow switching the editmode layer off since its confusing
	 * an alternative would be to always draw the editmode object. */
	if (scene->obedit && (scene->obedit->lay & v3d->lay) == 0) {
		int bit;
		for (bit = 0; bit < 32; bit++) {
			if (scene->obedit->lay & (1 << bit)) {
				v3d->lay |= 1 << bit;
				break;
			}
		}
	}
}

static int view3d_layers_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
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

			view3d_layers_editmode_ensure(scene, v3d);
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

		view3d_layers_editmode_ensure(scene, v3d);

		/* set active layer, ensure to always have one */
		if (v3d->lay & (1 << nr))
			v3d->layact = 1 << nr;
		else if ((v3d->lay & v3d->layact) == 0) {
			for (bit = 0; bit < 32; bit++) {
				if (v3d->lay & (1 << bit)) {
					v3d->layact = 1 << bit;
					break;
				}
			}
		}
	}
	
	if (v3d->scenelock) handle_view3d_lock(C);
	
	DAG_on_visible_update(CTX_data_main(C), false);

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

static int view3d_layers_poll(bContext *C)
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

	uiBlockSetHandleFunc(block, do_view3d_header_buttons, NULL);

	if (obedit && (obedit->type == OB_MESH)) {
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		uiLayout *row;

		row = uiLayoutRow(layout, true);
		block = uiLayoutGetBlock(row);
		uiDefIconButBitS(block, TOG, SCE_SELECT_VERTEX, B_SEL_VERT, ICON_VERTEXSEL,
		                 0, 0, UI_UNIT_X, UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0,
		                 TIP_("Vertex select - Shift-Click for multiple modes, Ctrl-Click contracts selection"));
		uiDefIconButBitS(block, TOG, SCE_SELECT_EDGE, B_SEL_EDGE, ICON_EDGESEL,
		                 0, 0, UI_UNIT_X, UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0,
		                 TIP_("Edge select - Shift-Click for multiple modes, Ctrl-Click expands/contracts selection"));
		uiDefIconButBitS(block, TOG, SCE_SELECT_FACE, B_SEL_FACE, ICON_FACESEL,
		                 0, 0, UI_UNIT_X, UI_UNIT_Y, &em->selectmode, 1.0, 0.0, 0, 0,
		                 TIP_("Face select - Shift-Click for multiple modes, Ctrl-Click expands selection"));
	}
}

void uiTemplateHeader3D(uiLayout *layout, struct bContext *C)
{
	bScreen *screen = CTX_wm_screen(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	PointerRNA v3dptr, toolsptr, sceneptr;
	Object *ob = OBACT;
	Object *obedit = CTX_data_edit_object(C);
	uiBlock *block;
	uiLayout *row;
	bool is_paint = false;
	int modeselect;
	
	RNA_pointer_create(&screen->id, &RNA_SpaceView3D, v3d, &v3dptr);
	RNA_pointer_create(&scene->id, &RNA_ToolSettings, ts, &toolsptr);
	RNA_pointer_create(&scene->id, &RNA_Scene, scene, &sceneptr);

	block = uiLayoutGetBlock(layout);
	uiBlockSetHandleFunc(block, do_view3d_header_buttons, NULL);

	/* other buttons: */
	uiBlockSetEmboss(block, UI_EMBOSS);
	
	/* mode */
	if (ob) {
		modeselect = ob->mode;
		is_paint = ELEM(ob->mode, OB_MODE_SCULPT, OB_MODE_VERTEX_PAINT, OB_MODE_WEIGHT_PAINT, OB_MODE_TEXTURE_PAINT);
	}
	else {
		modeselect = OB_MODE_OBJECT;
	}

	row = uiLayoutRow(layout, false);
	{
		EnumPropertyItem *item = object_mode_items;
		const char *name = "";
		int icon = ICON_OBJECT_DATAMODE;

		while (item->identifier) {
			if (item->value == modeselect && item->identifier[0]) {
				name = IFACE_(item->name);
				icon = item->icon;
				break;
			}
			item++;
		}

		uiItemMenuEnumO(row, C, "OBJECT_OT_mode_set", "mode", name, icon);
	}

	/* Draw type */
	uiItemR(layout, &v3dptr, "viewport_shade", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

	if (obedit == NULL && is_paint) {
		if (ob->mode & OB_MODE_ALL_PAINT) {
			/* Only for Weight Paint. makes no sense in other paint modes. */
			row = uiLayoutRow(layout, true);
			uiItemR(row, &v3dptr, "pivot_point", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		}

		/* Manipulators aren't used in paint modes */
		if (!ELEM(ob->mode, OB_MODE_SCULPT, OB_MODE_PARTICLE_EDIT)) {
			/* masks aren't used for sculpt and particle painting */
			PointerRNA meshptr;

			RNA_pointer_create(ob->data, &RNA_Mesh, ob->data, &meshptr);
			if (ob->mode & (OB_MODE_TEXTURE_PAINT | OB_MODE_VERTEX_PAINT)) {
				uiItemR(layout, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			}
			else {
				row = uiLayoutRow(layout, true);
				uiItemR(row, &meshptr, "use_paint_mask", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
				uiItemR(row, &meshptr, "use_paint_mask_vertex", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
			}
		}
	}
	else {
		row = uiLayoutRow(layout, true);
		uiItemR(row, &v3dptr, "pivot_point", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

		/* pose/object only however we want to allow in weight paint mode too
		 * so don't be totally strict and just check not-editmode for now 
		 * XXX We never get here when we are in Weight Paint mode
		 */
		if (obedit == NULL) {
			uiItemR(row, &v3dptr, "use_pivot_point_align", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		}

		/* Transform widget / manipulators */
		row = uiLayoutRow(layout, true);
		uiItemR(row, &v3dptr, "show_manipulator", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		if (v3d->twflag & V3D_USE_MANIPULATOR) {
			uiItemR(row, &v3dptr, "transform_manipulators", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
		}
		uiItemR(row, &v3dptr, "transform_orientation", 0, "", ICON_NONE);
	}

	if (obedit == NULL && v3d->localvd == NULL) {
		unsigned int ob_lay = ob ? ob->lay : 0;

		/* Layers */
		uiTemplateLayers(layout, v3d->scenelock ? &sceneptr : &v3dptr, "layers", &v3dptr, "layers_used", ob_lay);

		/* Scene lock */
		uiItemR(layout, &v3dptr, "lock_camera_and_layers", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);
	}
	
	uiTemplateEditModeSelection(layout, C);
}
