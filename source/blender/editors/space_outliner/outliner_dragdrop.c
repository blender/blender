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
 * The Original Code is Copyright (C) 2004 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_outliner/outliner_dragdrop.c
 *  \ingroup spoutliner
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_collection.h"
#include "BKE_context.h"
#include "BKE_layer.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_report.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "GPU_state.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "outliner_intern.h"

/* ******************** Drop Target Find *********************** */

static TreeElement *outliner_dropzone_element(TreeElement *te, const float fmval[2], const bool children)
{
	if ((fmval[1] > te->ys) && (fmval[1] < (te->ys + UI_UNIT_Y))) {
		/* name and first icon */
		if ((fmval[0] > te->xs + UI_UNIT_X) && (fmval[0] < te->xend))
			return te;
	}
	/* Not it.  Let's look at its children. */
	if (children && (TREESTORE(te)->flag & TSE_CLOSED) == 0 && (te->subtree.first)) {
		for (te = te->subtree.first; te; te = te->next) {
			TreeElement *te_valid = outliner_dropzone_element(te, fmval, children);
			if (te_valid)
				return te_valid;
		}
	}
	return NULL;
}

/* Find tree element to drop into. */
static TreeElement *outliner_dropzone_find(const SpaceOops *soops, const float fmval[2], const bool children)
{
	TreeElement *te;

	for (te = soops->tree.first; te; te = te->next) {
		TreeElement *te_valid = outliner_dropzone_element(te, fmval, children);
		if (te_valid)
			return te_valid;
	}
	return NULL;
}

static TreeElement *outliner_drop_find(bContext *C, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	float fmval[2];
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	return outliner_dropzone_find(soops, fmval, true);
}

static ID *outliner_ID_drop_find(bContext *C, const wmEvent *event, short idcode)
{
	TreeElement *te = outliner_drop_find(C, event);
	TreeStoreElem *tselem = (te) ? TREESTORE(te) : NULL;

	if (te && te->idcode == idcode && tselem->type == 0) {
		return tselem->id;
	}
	else {
		return NULL;
	}
}

/* ******************** Parent Drop Operator *********************** */

static bool parent_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event, const char **UNUSED(tooltip))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Object *ob = (Object *)WM_drag_ID(drag, ID_OB);
	if (!ob) {
		return false;
	}

	/* Ensure item under cursor is valid drop target */
	TreeElement *te = outliner_drop_find(C, event);
	TreeStoreElem *tselem = te ? TREESTORE(te) : NULL;

	if (!te) {
		/* pass */
	}
	else if (te->idcode == ID_OB && tselem->type == 0) {
		Scene *scene;
		ID *te_id = tselem->id;

		/* check if dropping self or parent */
		if (te_id == &ob->id || (Object *)te_id == ob->parent)
			return false;

		/* check that parent/child are both in the same scene */
		scene = (Scene *)outliner_search_back(soops, te, ID_SCE);

		/* currently outliner organized in a way that if there's no parent scene
		 * element for object it means that all displayed objects belong to
		 * active scene and parenting them is allowed (sergey)
		 */
		if (!scene) {
			return true;
		}
		else {
			for (ViewLayer *view_layer = scene->view_layers.first;
				 view_layer;
				 view_layer = view_layer->next)
			{
				if (BKE_view_layer_base_find(view_layer, ob)) {
					return true;
				}
			}
		}
	}

	return false;
}

static int parent_drop_exec(bContext *C, wmOperator *op)
{
	Object *par = NULL, *ob = NULL;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int partype = -1;
	char parname[MAX_ID_NAME], childname[MAX_ID_NAME];

	partype = RNA_enum_get(op->ptr, "type");
	RNA_string_get(op->ptr, "parent", parname);
	par = (Object *)BKE_libblock_find_name(bmain, ID_OB, parname);
	RNA_string_get(op->ptr, "child", childname);
	ob = (Object *)BKE_libblock_find_name(bmain, ID_OB, childname);

	if (ID_IS_LINKED(ob)) {
		BKE_report(op->reports, RPT_INFO, "Can't edit library linked object");
		return OPERATOR_CANCELLED;
	}

	ED_object_parent_set(op->reports, C, scene, ob, par, partype, false, false, NULL);

	DEG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);

	return OPERATOR_FINISHED;
}

static int parent_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te = outliner_drop_find(C, event);
	TreeStoreElem *tselem = te ? TREESTORE(te) : NULL;

	if (!(te && te->idcode == ID_OB && tselem->type == 0)) {
		return OPERATOR_CANCELLED;
	}

	Object *par = (Object *)tselem->id;
	Object *ob = (Object *)WM_drag_ID_from_event(event, ID_OB);

	if (ELEM(NULL, ob, par)) {
		return OPERATOR_CANCELLED;
	}
	if (ob == par) {
		return OPERATOR_CANCELLED;
	}
	if (ID_IS_LINKED(ob)) {
		BKE_report(op->reports, RPT_INFO, "Can't edit library linked object");
		return OPERATOR_CANCELLED;
	}

	char childname[MAX_ID_NAME];
	char parname[MAX_ID_NAME];
	STRNCPY(childname, ob->id.name);
	STRNCPY(parname, par->id.name);
	RNA_string_set(op->ptr, "child", childname);
	RNA_string_set(op->ptr, "parent", parname);

	Scene *scene = (Scene *)outliner_search_back(soops, te, ID_SCE);

	if (scene == NULL) {
		/* currently outlier organized in a way, that if there's no parent scene
		 * element for object it means that all displayed objects belong to
		 * active scene and parenting them is allowed (sergey)
		 */

		scene = CTX_data_scene(C);
	}

	if ((par->type != OB_ARMATURE) && (par->type != OB_CURVE) && (par->type != OB_LATTICE)) {
		int partype = 0;
		if (ED_object_parent_set(op->reports, C, scene, ob, par, partype, false, false, NULL)) {
			DEG_relations_tag_update(bmain);
			WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
			WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
		}
	}
	else {
		/* Menu creation */
		wmOperatorType *ot = WM_operatortype_find("OUTLINER_OT_parent_drop", false);
		uiPopupMenu *pup = UI_popup_menu_begin(C, IFACE_("Set Parent To"), ICON_NONE);
		uiLayout *layout = UI_popup_menu_layout(pup);
		PointerRNA ptr;

		/* Cannot use uiItemEnumO()... have multiple properties to set. */
		uiItemFullO_ptr(layout, ot, IFACE_("Object"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
		RNA_string_set(&ptr, "parent", parname);
		RNA_string_set(&ptr, "child", childname);
		RNA_enum_set(&ptr, "type", PAR_OBJECT);

		/* par becomes parent, make the associated menus */
		if (par->type == OB_ARMATURE) {
			uiItemFullO_ptr(layout, ot, IFACE_("Armature Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_ARMATURE);

			uiItemFullO_ptr(layout, ot, IFACE_("   With Empty Groups"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_ARMATURE_NAME);

			uiItemFullO_ptr(layout, ot, IFACE_("   With Envelope Weights"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_ARMATURE_ENVELOPE);

			uiItemFullO_ptr(layout, ot, IFACE_("   With Automatic Weights"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_ARMATURE_AUTO);

			uiItemFullO_ptr(layout, ot, IFACE_("Bone"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_BONE);
		}
		else if (par->type == OB_CURVE) {
			uiItemFullO_ptr(layout, ot, IFACE_("Curve Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_CURVE);

			uiItemFullO_ptr(layout, ot, IFACE_("Follow Path"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_FOLLOW);

			uiItemFullO_ptr(layout, ot, IFACE_("Path Constraint"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_PATH_CONST);
		}
		else if (par->type == OB_LATTICE) {
			uiItemFullO_ptr(layout, ot, IFACE_("Lattice Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, 0, &ptr);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_LATTICE);
		}

		UI_popup_menu_end(C, pup);

		return OPERATOR_INTERFACE;
	}

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_parent_drop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop to Set Parent";
	ot->description = "Drag to parent in Outliner";
	ot->idname = "OUTLINER_OT_parent_drop";

	/* api callbacks */
	ot->invoke = parent_drop_invoke;
	ot->exec = parent_drop_exec;

	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_string(ot->srna, "child", "Object", MAX_ID_NAME, "Child", "Child Object");
	RNA_def_string(ot->srna, "parent", "Object", MAX_ID_NAME, "Parent", "Parent Object");
	RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
}

static bool parenting_poll(bContext *C)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);

	if (soops) {
		if (soops->outlinevis == SO_SCENES) {
			return true;
		}
		else if ((soops->outlinevis == SO_VIEW_LAYER) &&
		         (soops->filter & SO_FILTER_NO_COLLECTION))
		{
			return true;
		}
	}

	return false;
}

/* ******************** Parent Clear Operator *********************** */

static bool parent_clear_poll(bContext *C, wmDrag *drag, const wmEvent *event, const char **UNUSED(tooltip))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);

	if (!ELEM(soops->outlinevis, SO_VIEW_LAYER)) {
		return false;
	}

	Object *ob = (Object *)WM_drag_ID(drag, ID_OB);
	if (!(ob && ob->parent)) {
		return false;
	}

	TreeElement *te = outliner_drop_find(C, event);
	if (te) {
		TreeStoreElem *tselem = TREESTORE(te);

		switch (te->idcode) {
			case ID_SCE:
				return (ELEM(tselem->type, TSE_R_LAYER_BASE, TSE_R_LAYER));
			case ID_OB:
				return (ELEM(tselem->type, TSE_MODIFIER_BASE, TSE_CONSTRAINT_BASE));
			/* Other codes to ignore? */
		}
	}
	return (te == NULL);
}

static int parent_clear_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = (Object *)WM_drag_ID_from_event(event, ID_OB);

	if (ob == NULL) {
		return OPERATOR_CANCELLED;
	}

	ED_object_parent_clear(ob, 0);

	DEG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop to Clear Parent";
	ot->description = "Drag to clear parent in Outliner";
	ot->idname = "OUTLINER_OT_parent_clear";

	/* api callbacks */
	ot->invoke = parent_clear_invoke;

	ot->poll = parenting_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ******************** Scene Drop Operator *********************** */

static bool scene_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event, const char **UNUSED(tooltip))
{
	/* Ensure item under cursor is valid drop target */
	Object *ob = (Object *)WM_drag_ID(drag, ID_OB);
	return (ob && (outliner_ID_drop_find(C, event, ID_SCE) != NULL));
}

static int scene_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = (Scene *)outliner_ID_drop_find(C, event, ID_SCE);
	Object *ob = (Object *)WM_drag_ID_from_event(event, ID_OB);

	if (ELEM(NULL, ob, scene) || ID_IS_LINKED(scene)) {
		return OPERATOR_CANCELLED;
	}

	if (BKE_scene_has_object(scene, ob)) {
		return OPERATOR_CANCELLED;
	}

	Collection *collection;
	if (scene != CTX_data_scene(C)) {
		/* when linking to an inactive scene link to the master collection */
		collection = BKE_collection_master(scene);
	}
	else {
		collection = CTX_data_collection(C);
	}

	BKE_collection_object_add(bmain, collection, ob);

	for (ViewLayer *view_layer = scene->view_layers.first; view_layer; view_layer = view_layer->next) {
		Base *base = BKE_view_layer_base_find(view_layer, ob);
		if (base) {
			ED_object_base_select(base, BA_SELECT);
		}
	}

	DEG_relations_tag_update(bmain);

	DEG_id_tag_update(&scene->id, DEG_TAG_SELECT_UPDATE);
	WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_scene_drop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop Object to Scene";
	ot->description = "Drag object to scene in Outliner";
	ot->idname = "OUTLINER_OT_scene_drop";

	/* api callbacks */
	ot->invoke = scene_drop_invoke;

	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ******************** Material Drop Operator *********************** */

static bool material_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event, const char **UNUSED(tooltip))
{
	/* Ensure item under cursor is valid drop target */
	Material *ma = (Material *)WM_drag_ID(drag, ID_MA);
	return (ma && (outliner_ID_drop_find(C, event, ID_OB) != NULL));
}

static int material_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Object *ob = (Object *)outliner_ID_drop_find(C, event, ID_OB);
	Material *ma = (Material *)WM_drag_ID_from_event(event, ID_MA);

	if (ELEM(NULL, ob, ma)) {
		return OPERATOR_CANCELLED;
	}

	assign_material(bmain, ob, ma, ob->totcol + 1, BKE_MAT_ASSIGN_USERPREF);

	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_material_drop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop Material on Object";
	ot->description = "Drag material to object in Outliner";
	ot->idname = "OUTLINER_OT_material_drop";

	/* api callbacks */
	ot->invoke = material_drop_invoke;

	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ******************** Collection Drop Operator *********************** */

static bool collection_drop_poll(bContext *C, wmDrag *drag, const wmEvent *event, const char **UNUSED(tooltip))
{
	Object *ob = (Object *)WM_drag_ID(drag, ID_OB);
	Collection *collection = (Collection *)WM_drag_ID(drag, ID_GR);

	if (ob || collection) {
		TreeElement *te = outliner_drop_find(C, event);
		return (te && outliner_is_collection_tree_element(te));
	}
	else {
		return false;
	}
}

static int collection_drop_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	TreeElement *te = outliner_drop_find(C, event);

	if (!te || !outliner_is_collection_tree_element(te)) {
		return OPERATOR_CANCELLED;
	}

	Collection *collection = outliner_collection_from_tree_element(te);

	// TODO: don't use scene, makes no sense anymore
	// TODO: move rather than link, change hover text
	Scene *scene = BKE_scene_find_from_collection(bmain, collection);
	Object *ob = (Object *)WM_drag_ID_from_event(event, ID_OB);
	if (ELEM(NULL, ob, scene, collection)) {
		return OPERATOR_CANCELLED;
	}

	BKE_collection_object_add(bmain, collection, ob);

	DEG_id_tag_update(&collection->id, DEG_TAG_COPY_ON_WRITE);
	DEG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER, scene);

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_collection_drop(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Link to Collection"; // TODO: rename to move?
	ot->description = "Drag to move to collection in Outliner";
	ot->idname = "OUTLINER_OT_collection_drop";

	/* api callbacks */
	ot->invoke = collection_drop_invoke;
	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/* ********************* Outliner Drag Operator ******************** */

typedef struct OutlinerDragDropTooltip {
	TreeElement *te;
	void *handle;
} OutlinerDragDropTooltip;

static bool outliner_item_drag_drop_poll(bContext *C)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	return ED_operator_outliner_active(C) &&
	       /* Only collection display modes supported for now. Others need more design work */
	       ELEM(soops->outlinevis, SO_VIEW_LAYER, SO_LIBRARIES);
}

static TreeElement *outliner_item_drag_element_find(SpaceOops *soops, ARegion *ar, const wmEvent *event)
{
	/* note: using EVT_TWEAK_ events to trigger dragging is fine,
	 * it sends coordinates from where dragging was started */
	const float my = UI_view2d_region_to_view_y(&ar->v2d, event->mval[1]);
	return outliner_find_item_at_y(soops, &soops->tree, my);
}

static void outliner_item_drag_end(wmWindow *win, OutlinerDragDropTooltip *data)
{
	MEM_SAFE_FREE(data->te->drag_data);

	if (data->handle) {
		WM_draw_cb_exit(win, data->handle);
	}

	MEM_SAFE_FREE(data);
}

static void outliner_item_drag_get_insert_data(
        const SpaceOops *soops, ARegion *ar, const wmEvent *event, TreeElement *te_dragged,
        TreeElement **r_te_insert_handle, TreeElementInsertType *r_insert_type)
{
	TreeElement *te_hovered;
	float view_mval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &view_mval[0], &view_mval[1]);
	te_hovered = outliner_find_item_at_y(soops, &soops->tree, view_mval[1]);

	if (te_hovered) {
		/* mouse hovers an element (ignoring x-axis), now find out how to insert the dragged item exactly */

		if (te_hovered == te_dragged) {
			*r_te_insert_handle = te_dragged;
		}
		else if (te_hovered != te_dragged) {
			const float margin = UI_UNIT_Y * (1.0f / 4);

			*r_te_insert_handle = te_hovered;
			if (view_mval[1] < (te_hovered->ys + margin)) {
				if (TSELEM_OPEN(TREESTORE(te_hovered), soops)) {
					/* inserting after a open item means we insert into it, but as first child */
					if (BLI_listbase_is_empty(&te_hovered->subtree)) {
						*r_insert_type = TE_INSERT_INTO;
					}
					else {
						*r_insert_type = TE_INSERT_BEFORE;
						*r_te_insert_handle = te_hovered->subtree.first;
					}
				}
				else {
					*r_insert_type = TE_INSERT_AFTER;
				}
			}
			else if (view_mval[1] > (te_hovered->ys + (3 * margin))) {
				*r_insert_type = TE_INSERT_BEFORE;
			}
			else {
				*r_insert_type = TE_INSERT_INTO;
			}
		}
	}
	else {
		/* mouse doesn't hover any item (ignoring x-axis), so it's either above list bounds or below. */

		TreeElement *first = soops->tree.first;
		TreeElement *last = soops->tree.last;

		if (view_mval[1] < last->ys) {
			*r_te_insert_handle = last;
			*r_insert_type = TE_INSERT_AFTER;
		}
		else if (view_mval[1] > (first->ys + UI_UNIT_Y)) {
			*r_te_insert_handle = first;
			*r_insert_type = TE_INSERT_BEFORE;
		}
		else {
			BLI_assert(0);
		}
	}
}

static void outliner_item_drag_handle(
        SpaceOops *soops, ARegion *ar, const wmEvent *event, TreeElement *te_dragged)
{
	TreeElement *te_insert_handle;
	TreeElementInsertType insert_type;

	outliner_item_drag_get_insert_data(soops, ar, event, te_dragged, &te_insert_handle, &insert_type);

	if (!te_dragged->reinsert_poll &&
	    /* there is no reinsert_poll, so we do some generic checks (same types and reinsert callback is available) */
	    (TREESTORE(te_dragged)->type == TREESTORE(te_insert_handle)->type) &&
	    te_dragged->reinsert)
	{
		/* pass */
	}
	else if (te_dragged == te_insert_handle) {
		/* nothing will happen anyway, no need to do poll check */
	}
	else if (!te_dragged->reinsert_poll ||
	         !te_dragged->reinsert_poll(te_dragged, &te_insert_handle, &insert_type))
	{
		te_insert_handle = NULL;
	}
	te_dragged->drag_data->insert_type = insert_type;
	te_dragged->drag_data->insert_handle = te_insert_handle;
}

/**
 * Returns true if it is a collection and empty.
 */
static bool is_empty_collection(TreeElement *te)
{
	Collection *collection = outliner_collection_from_tree_element(te);

	if (!collection) {
		return false;
	}

	return BLI_listbase_is_empty(&collection->gobject) &&
	       BLI_listbase_is_empty(&collection->children);
}

static bool outliner_item_drag_drop_apply(
        Main *bmain,
        Scene *scene,
        SpaceOops *soops,
        OutlinerDragDropTooltip *data,
        const wmEvent *event)
{
	TreeElement *dragged_te = data->te;
	TreeElement *insert_handle = dragged_te->drag_data->insert_handle;
	TreeElementInsertType insert_type = dragged_te->drag_data->insert_type;

	if ((insert_handle == dragged_te) || !insert_handle) {
		/* No need to do anything */
	}
	else if (dragged_te->reinsert) {
		BLI_assert(!dragged_te->reinsert_poll || dragged_te->reinsert_poll(dragged_te, &insert_handle,
		                                                                   &insert_type));
		/* call of assert above should not have changed insert_handle and insert_type at this point */
		BLI_assert(dragged_te->drag_data->insert_handle == insert_handle &&
		           dragged_te->drag_data->insert_type == insert_type);

		/* If the collection was just created and you moved objects/collections inside it,
		 * it is strange to have it closed and we not see the newly dragged elements. */
		const bool should_open_collection = (insert_type == TE_INSERT_INTO) && is_empty_collection(insert_handle);

		dragged_te->reinsert(bmain, scene, soops, dragged_te, insert_handle, insert_type, event);

		if (should_open_collection && !is_empty_collection(insert_handle)) {
			TREESTORE(insert_handle)->flag &= ~TSE_CLOSED;
		}
		return true;
	}

	return false;
}

static int outliner_item_drag_drop_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	OutlinerDragDropTooltip *data = op->customdata;
	TreeElement *te_dragged = data->te;
	int retval = OPERATOR_RUNNING_MODAL;
	bool redraw = false;
	bool skip_rebuild = true;

	switch (event->type) {
		case EVT_MODAL_MAP:
			if (event->val == OUTLINER_ITEM_DRAG_CONFIRM) {
				if (outliner_item_drag_drop_apply(bmain, scene, soops, data, event)) {
					skip_rebuild = false;
				}
				retval = OPERATOR_FINISHED;
			}
			else if (event->val == OUTLINER_ITEM_DRAG_CANCEL) {
				retval = OPERATOR_CANCELLED;
			}
			else {
				BLI_assert(0);
			}
			WM_event_add_mousemove(C); /* update highlight */
			outliner_item_drag_end(CTX_wm_window(C), data);
			redraw = true;
			break;
		case MOUSEMOVE:
			outliner_item_drag_handle(soops, ar, event, te_dragged);
			redraw = true;
			break;
	}

	if (redraw) {
		if (skip_rebuild) {
			ED_region_tag_redraw_no_rebuild(ar);
		}
		else {
			ED_region_tag_redraw(ar);
		}
	}

	return retval;
}

static const char *outliner_drag_drop_tooltip_get(
        const TreeElement *te_float)
{
	const char *name = NULL;

	const TreeElement *te_insert = te_float->drag_data->insert_handle;
	if (te_float && outliner_is_collection_tree_element(te_float)) {
		if (te_insert == NULL) {
			name = TIP_("Move collection");
		}
		else {
			switch (te_float->drag_data->insert_type) {
				case TE_INSERT_BEFORE:
					if (te_insert->prev && outliner_is_collection_tree_element(te_insert->prev)) {
						name = TIP_("Move between collections");
					}
					else {
						name = TIP_("Move before collection");
					}
					break;
				case TE_INSERT_AFTER:
					if (te_insert->next && outliner_is_collection_tree_element(te_insert->next)) {
						name = TIP_("Move between collections");
					}
					else {
						name = TIP_("Move after collection");
					}
					break;
				case TE_INSERT_INTO:
					name = TIP_("Move inside collection");
					break;
			}
		}
	}
	else if ((TREESTORE(te_float)->type == 0) && (te_float->idcode == ID_OB)) {
		name = TIP_("Move to collection (Ctrl to link)");
	}

	return name;
}

static void outliner_drag_drop_tooltip_cb(const wmWindow *win, void *vdata)
{
	OutlinerDragDropTooltip *data = vdata;
	const char *tooltip;

	int cursorx, cursory;
	int x, y;

	tooltip = outliner_drag_drop_tooltip_get(data->te);
	if (tooltip == NULL) {
		return;
	}

	cursorx = win->eventstate->x;
	cursory = win->eventstate->y;

	x = cursorx + U.widget_unit;
	y = cursory - U.widget_unit;

	/* Drawing. */
	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;

	const float col_fg[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	const float col_bg[4] = {0.0f, 0.0f, 0.0f, 0.2f};

	GPU_blend(true);
	UI_fontstyle_draw_simple_backdrop(fstyle, x, y, tooltip, col_fg, col_bg);
	GPU_blend(false);
}

static int outliner_item_drag_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te_dragged = outliner_item_drag_element_find(soops, ar, event);

	if (!te_dragged) {
		return (OPERATOR_FINISHED | OPERATOR_PASS_THROUGH);
	}

	OutlinerDragDropTooltip *data = MEM_mallocN(sizeof(OutlinerDragDropTooltip), __func__);
	data->te = te_dragged;

	op->customdata = data;
	te_dragged->drag_data = MEM_callocN(sizeof(*te_dragged->drag_data), __func__);
	/* by default we don't change the item position */
	te_dragged->drag_data->insert_handle = te_dragged;
	/* unset highlighted tree element, dragged one will be highlighted instead */
	outliner_flag_set(&soops->tree, TSE_HIGHLIGHTED, false);

	ED_region_tag_redraw_no_rebuild(ar);

	WM_event_add_modal_handler(C, op);

	data->handle = WM_draw_cb_activate(CTX_wm_window(C), outliner_drag_drop_tooltip_cb, data);

	return OPERATOR_RUNNING_MODAL;
}

/**
 * Notes about Outliner Item Drag 'n Drop:
 * Right now only collections display mode is supported. But ideally all/most modes would support this. There are
 * just some open design questions that have to be answered: do we want to allow mixing order of different data types
 * (like render-layers and objects)? Would that be a purely visual change or would that have any other effect? ...
 */
void OUTLINER_OT_item_drag_drop(wmOperatorType *ot)
{
	ot->name = "Drag and Drop";
	ot->idname = "OUTLINER_OT_item_drag_drop";
	ot->description = "Change the hierarchical position of an item by repositioning it using drag and drop";

	ot->invoke = outliner_item_drag_drop_invoke;
	ot->modal = outliner_item_drag_drop_modal;

	ot->poll = outliner_item_drag_drop_poll;

	ot->flag = OPTYPE_UNDO;
}

/* *************************** Drop Boxes ************************** */

/* region dropbox definition */
void outliner_dropboxes(void)
{
	ListBase *lb = WM_dropboxmap_find("Outliner", SPACE_OUTLINER, RGN_TYPE_WINDOW);

	WM_dropbox_add(lb, "OUTLINER_OT_parent_drop", parent_drop_poll, NULL);
	WM_dropbox_add(lb, "OUTLINER_OT_parent_clear", parent_clear_poll, NULL);
	WM_dropbox_add(lb, "OUTLINER_OT_scene_drop", scene_drop_poll, NULL);
	WM_dropbox_add(lb, "OUTLINER_OT_material_drop", material_drop_poll, NULL);
	WM_dropbox_add(lb, "OUTLINER_OT_collection_drop", collection_drop_poll, NULL);
}
