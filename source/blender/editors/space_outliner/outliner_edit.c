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

/** \file blender/editors/space_outliner/outliner_edit.c
 *  \ingroup spoutliner
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_group_types.h"
#include "DNA_ID.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_material_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_mempool.h"
#include "BLI_stack.h"
#include "BLI_string.h"

#include "BLT_translation.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_idcode.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_outliner_treehash.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_material.h"
#include "BKE_group.h"

#include "../blenloader/BLO_readfile.h"

#include "ED_object.h"
#include "ED_outliner.h"
#include "ED_screen.h"
#include "ED_keyframing.h"
#include "ED_armature.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "GPU_material.h"

#include "outliner_intern.h"

/* ************************************************************** */
/* Unused Utilities */
// XXX: where to place these?

/* This is not used anywhere at the moment */
#if 0
static void outliner_open_reveal(SpaceOops *soops, ListBase *lb, TreeElement *teFind, int *found)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		/* check if this tree-element was the one we're seeking */
		if (te == teFind) {
			*found = 1;
			return;
		}
		
		/* try to see if sub-tree contains it then */
		outliner_open_reveal(soops, &te->subtree, teFind, found);
		if (*found) {
			tselem = TREESTORE(te);
			if (tselem->flag & TSE_CLOSED) 
				tselem->flag &= ~TSE_CLOSED;
			return;
		}
	}
}
#endif

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

/* Used for drag and drop parenting */
TreeElement *outliner_dropzone_find(const SpaceOops *soops, const float fmval[2], const bool children)
{
	TreeElement *te;

	for (te = soops->tree.first; te; te = te->next) {
		TreeElement *te_valid = outliner_dropzone_element(te, fmval, children);
		if (te_valid)
			return te_valid;
	}
	return NULL;
}

/* ************************************************************** */
/* Click Activated */

/* Toggle Open/Closed ------------------------------------------- */

static int do_outliner_item_openclose(bContext *C, SpaceOops *soops, TreeElement *te, const bool all, const float mval[2])
{
	
	if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
		TreeStoreElem *tselem = TREESTORE(te);
		
		/* all below close/open? */
		if (all) {
			tselem->flag &= ~TSE_CLOSED;
			outliner_set_flag(&te->subtree, TSE_CLOSED, !outliner_has_one_flag(&te->subtree, TSE_CLOSED, 1));
		}
		else {
			if (tselem->flag & TSE_CLOSED) tselem->flag &= ~TSE_CLOSED;
			else tselem->flag |= TSE_CLOSED;
		}
		
		return 1;
	}
	
	for (te = te->subtree.first; te; te = te->next) {
		if (do_outliner_item_openclose(C, soops, te, all, mval)) 
			return 1;
	}
	return 0;
	
}

/* event can enterkey, then it opens/closes */
static int outliner_item_openclose(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	const bool all = RNA_boolean_get(op->ptr, "all");
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);
	
	for (te = soops->tree.first; te; te = te->next) {
		if (do_outliner_item_openclose(C, soops, te, all, fmval)) 
			break;
	}

	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_item_openclose(wmOperatorType *ot)
{
	ot->name = "Open/Close Item";
	ot->idname = "OUTLINER_OT_item_openclose";
	ot->description = "Toggle whether item under cursor is enabled or closed";
	
	ot->invoke = outliner_item_openclose;
	
	ot->poll = ED_operator_outliner_active;
	
	RNA_def_boolean(ot->srna, "all", 1, "All", "Close or open all items");
}

/* Rename --------------------------------------------------- */

static void do_item_rename(ARegion *ar, TreeElement *te, TreeStoreElem *tselem, ReportList *reports)
{
	/* can't rename rna datablocks entries or listbases */
	if (ELEM(tselem->type, TSE_RNA_STRUCT, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM, TSE_ID_BASE)) {
		/* do nothing */;
	}
	else if (ELEM(tselem->type, TSE_ANIM_DATA, TSE_NLA, TSE_DEFGROUP_BASE, TSE_CONSTRAINT_BASE, TSE_MODIFIER_BASE,
	              TSE_DRIVER_BASE, TSE_POSE_BASE, TSE_POSEGRP_BASE, TSE_R_LAYER_BASE, TSE_R_PASS))
	{
		BKE_report(reports, RPT_WARNING, "Cannot edit builtin name");
	}
	else if (ELEM(tselem->type, TSE_SEQUENCE, TSE_SEQ_STRIP, TSE_SEQUENCE_DUP)) {
		BKE_report(reports, RPT_WARNING, "Cannot edit sequence name");
	}
	else if (ID_IS_LINKED_DATABLOCK(tselem->id)) {
		BKE_report(reports, RPT_WARNING, "Cannot edit external libdata");
	}
	else if (te->idcode == ID_LI && ((Library *)tselem->id)->parent) {
		BKE_report(reports, RPT_WARNING, "Cannot edit the path of an indirectly linked library");
	}
	else {
		tselem->flag |= TSE_TEXTBUT;
		ED_region_tag_redraw(ar);
	}
}

void item_rename_cb(
        bContext *C, ReportList *reports, Scene *UNUSED(scene), TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	ARegion *ar = CTX_wm_region(C);
	do_item_rename(ar, te, tselem, reports);
}

static int do_outliner_item_rename(ReportList *reports, ARegion *ar, TreeElement *te, const float mval[2])
{
	if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
		TreeStoreElem *tselem = TREESTORE(te);
		
		/* click on name */
		if (mval[0] > te->xs + UI_UNIT_X * 2 && mval[0] < te->xend) {
			do_item_rename(ar, te, tselem, reports);
			return 1;
		}
		return 0;
	}
	
	for (te = te->subtree.first; te; te = te->next) {
		if (do_outliner_item_rename(reports, ar, te, mval)) return 1;
	}
	return 0;
}

static int outliner_item_rename(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	bool changed = false;
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);
	
	for (te = soops->tree.first; te; te = te->next) {
		if (do_outliner_item_rename(op->reports, ar, te, fmval)) {
			changed = true;
			break;
		}
	}
	
	return changed ? OPERATOR_FINISHED : OPERATOR_PASS_THROUGH;
}


void OUTLINER_OT_item_rename(wmOperatorType *ot)
{
	ot->name = "Rename Item";
	ot->idname = "OUTLINER_OT_item_rename";
	ot->description = "Rename item under cursor";
	
	ot->invoke = outliner_item_rename;
	
	ot->poll = ED_operator_outliner_active;
}

/* ID delete --------------------------------------------------- */

static void id_delete(bContext *C, ReportList *reports, TreeElement *te, TreeStoreElem *tselem)
{
	Main *bmain = CTX_data_main(C);
	ID *id = tselem->id;

	BLI_assert(te->idcode != 0 && id != NULL);
	UNUSED_VARS_NDEBUG(te);

	if (te->idcode == ID_LI && ((Library *)id)->parent != NULL) {
		BKE_reportf(reports, RPT_WARNING, "Cannot delete indirectly linked library '%s'", id->name);
		return;
	}
	if (id->tag & LIB_TAG_INDIRECT) {
		BKE_reportf(reports, RPT_WARNING, "Cannot delete indirectly linked id '%s'", id->name);
		return;
	}
	else if (BKE_library_ID_is_indirectly_used(bmain, id) && ID_REAL_USERS(id) <= 1) {
		BKE_reportf(reports, RPT_WARNING,
		            "Cannot delete id '%s', indirectly used data-blocks need at least one user",
		            id->name);
		return;
	}


	BKE_libblock_delete(bmain, id);

	WM_event_add_notifier(C, NC_WINDOW, NULL);
}

void id_delete_cb(
        bContext *C, ReportList *reports, Scene *UNUSED(scene),
        TreeElement *te, TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	id_delete(C, reports, te, tselem);
}

static int outliner_id_delete_invoke_do(bContext *C, ReportList *reports, TreeElement *te, const float mval[2])
{
	if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
		TreeStoreElem *tselem = TREESTORE(te);

		if (te->idcode != 0 && tselem->id) {
			if (te->idcode == ID_LI && ((Library *)tselem->id)->parent) {
				BKE_reportf(reports, RPT_ERROR_INVALID_INPUT,
				            "Cannot delete indirectly linked library '%s'", ((Library *)tselem->id)->filepath);
				return OPERATOR_CANCELLED;
			}
			id_delete(C, reports, te, tselem);
			return OPERATOR_FINISHED;
		}
	}
	else {
		for (te = te->subtree.first; te; te = te->next) {
			int ret;
			if ((ret = outliner_id_delete_invoke_do(C, reports, te, mval))) {
				return ret;
			}
		}
	}

	return 0;
}

static int outliner_id_delete_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];

	BLI_assert(ar && soops);

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	for (te = soops->tree.first; te; te = te->next) {
		int ret;

		if ((ret = outliner_id_delete_invoke_do(C, op->reports, te, fmval))) {
			return ret;
		}
	}

	return OPERATOR_CANCELLED;
}

void OUTLINER_OT_id_delete(wmOperatorType *ot)
{
	ot->name = "Delete Data-Block";
	ot->idname = "OUTLINER_OT_id_delete";
	ot->description = "Delete the ID under cursor";

	ot->invoke = outliner_id_delete_invoke;
	ot->poll = ED_operator_outliner_active;
}

/* ID remap --------------------------------------------------- */

static int outliner_id_remap_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);

	const short id_type = (short)RNA_enum_get(op->ptr, "id_type");
	ID *old_id = BLI_findlink(which_libbase(CTX_data_main(C), id_type), RNA_enum_get(op->ptr, "old_id"));
	ID *new_id = BLI_findlink(which_libbase(CTX_data_main(C), id_type), RNA_enum_get(op->ptr, "new_id"));

	/* check for invalid states */
	if (soops == NULL) {
		return OPERATOR_CANCELLED;
	}

	if (!(old_id && new_id && (old_id != new_id) && (GS(old_id->name) == GS(new_id->name)))) {
		BKE_reportf(op->reports, RPT_ERROR_INVALID_INPUT, "Invalid old/new ID pair ('%s' / '%s')",
		            old_id ? old_id->name : "Invalid ID", new_id ? new_id->name : "Invalid ID");
		return OPERATOR_CANCELLED;
	}

	if (ID_IS_LINKED_DATABLOCK(old_id)) {
		BKE_reportf(op->reports, RPT_WARNING,
		            "Old ID '%s' is linked from a library, indirect usages of this data-block will not be remapped",
		            old_id->name);
	}

	BKE_libblock_remap(bmain, old_id, new_id,
	                   ID_REMAP_SKIP_INDIRECT_USAGE | ID_REMAP_SKIP_NEVER_NULL_USAGE);

	BKE_main_lib_objects_recalc_all(bmain);

	/* recreate dependency graph to include new objects */
	DAG_scene_relations_rebuild(bmain, scene);

	/* free gpu materials, some materials depend on existing objects, such as lamps so freeing correctly refreshes */
	GPU_materials_free();

	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

static bool outliner_id_remap_find_tree_element(bContext *C, wmOperator *op, ListBase *tree, const float y)
{
	TreeElement *te;

	for (te = tree->first; te; te = te->next) {
		if (y > te->ys && y < te->ys + UI_UNIT_Y) {
			TreeStoreElem *tselem = TREESTORE(te);

			if (tselem->type == 0 && tselem->id) {
				printf("found id %s (%p)!\n", tselem->id->name, tselem->id);

				RNA_enum_set(op->ptr, "id_type", GS(tselem->id->name));
				RNA_enum_set_identifier(C, op->ptr, "new_id", tselem->id->name + 2);
				RNA_enum_set_identifier(C, op->ptr, "old_id", tselem->id->name + 2);
				return true;
			}
		}
		if (outliner_id_remap_find_tree_element(C, op, &te->subtree, y)) {
			return true;
		}
	}
	return false;
}

static int outliner_id_remap_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	float fmval[2];

	if (!RNA_property_is_set(op->ptr, RNA_struct_find_property(op->ptr, "id_type"))) {
		UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

		outliner_id_remap_find_tree_element(C, op, &soops->tree, fmval[1]);
	}

	return WM_operator_props_dialog_popup(C, op, 200, 100);
}

static EnumPropertyItem *outliner_id_itemf(bContext *C, PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem item_tmp = {0}, *item = NULL;
	int totitem = 0;
	int i = 0;

	short id_type = (short)RNA_enum_get(ptr, "id_type");
	ID *id = which_libbase(CTX_data_main(C), id_type)->first;

	for (; id; id = id->next) {
		item_tmp.identifier = item_tmp.name = id->name + 2;
		item_tmp.value = i++;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

void OUTLINER_OT_id_remap(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Outliner ID data Remap";
	ot->idname = "OUTLINER_OT_id_remap";
	ot->description = "";

	/* callbacks */
	ot->invoke = outliner_id_remap_invoke;
	ot->exec = outliner_id_remap_exec;
	ot->poll = ED_operator_outliner_active;

	ot->flag = 0;

	prop = RNA_def_enum(ot->srna, "id_type", rna_enum_id_type_items, ID_OB, "ID Type", "");
	RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

	prop = RNA_def_enum(ot->srna, "old_id", DummyRNA_NULL_items, 0, "Old ID", "Old ID to replace");
	RNA_def_property_enum_funcs_runtime(prop, NULL, NULL, outliner_id_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE | PROP_HIDDEN);

	ot->prop = RNA_def_enum(ot->srna, "new_id", DummyRNA_NULL_items, 0,
	                        "New ID", "New ID to remap all selected IDs' users to");
	RNA_def_property_enum_funcs_runtime(ot->prop, NULL, NULL, outliner_id_itemf);
	RNA_def_property_flag(ot->prop, PROP_ENUM_NO_TRANSLATE);
}

void id_remap_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	wmOperatorType *ot = WM_operatortype_find("OUTLINER_OT_id_remap", false);
	PointerRNA op_props;

	BLI_assert(tselem->id != NULL);

	WM_operator_properties_create_ptr(&op_props, ot);

	RNA_enum_set(&op_props, "id_type", GS(tselem->id->name));
	RNA_enum_set_identifier(C, &op_props, "old_id", tselem->id->name + 2);

	WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &op_props);

	WM_operator_properties_free(&op_props);
}

/* Library relocate/reload --------------------------------------------------- */

static int lib_relocate(
        bContext *C, TreeElement *te, TreeStoreElem *tselem, wmOperatorType *ot, const bool reload)
{
	PointerRNA op_props;
	int ret = 0;

	BLI_assert(te->idcode == ID_LI && tselem->id != NULL);
	UNUSED_VARS_NDEBUG(te);

	WM_operator_properties_create_ptr(&op_props, ot);

	RNA_string_set(&op_props, "library", tselem->id->name + 2);

	if (reload) {
		Library *lib = (Library *)tselem->id;
		char dir[FILE_MAXDIR], filename[FILE_MAX];

		BLI_split_dirfile(lib->filepath, dir, filename, sizeof(dir), sizeof(filename));

		printf("%s, %s\n", tselem->id->name, lib->filepath);

		/* We assume if both paths in lib are not the same then lib->name was relative... */
		RNA_boolean_set(&op_props, "relative_path", BLI_path_cmp(lib->filepath, lib->name) != 0);

		RNA_string_set(&op_props, "directory", dir);
		RNA_string_set(&op_props, "filename", filename);

		ret = WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &op_props);
	}
	else {
		ret = WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &op_props);
	}

	WM_operator_properties_free(&op_props);

	return ret;
}

static int outliner_lib_relocate_invoke_do(
        bContext *C, ReportList *reports, TreeElement *te, const float mval[2], const bool reload)
{
	if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
		TreeStoreElem *tselem = TREESTORE(te);

		if (te->idcode == ID_LI && tselem->id) {
			if (((Library *)tselem->id)->parent && !reload) {
				BKE_reportf(reports, RPT_ERROR_INVALID_INPUT,
				            "Cannot relocate indirectly linked library '%s'", ((Library *)tselem->id)->filepath);
				return OPERATOR_CANCELLED;
			}
			else {
				wmOperatorType *ot = WM_operatortype_find(reload ? "WM_OT_lib_reload" : "WM_OT_lib_relocate", false);

				return lib_relocate(C, te, tselem, ot, reload);
			}
		}
	}
	else {
		for (te = te->subtree.first; te; te = te->next) {
			int ret;
			if ((ret = outliner_lib_relocate_invoke_do(C, reports, te, mval, reload))) {
				return ret;
			}
		}
	}

	return 0;
}

static int outliner_lib_relocate_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];

	BLI_assert(ar && soops);

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	for (te = soops->tree.first; te; te = te->next) {
		int ret;

		if ((ret = outliner_lib_relocate_invoke_do(C, op->reports, te, fmval, false))) {
			return ret;
		}
	}

	return OPERATOR_CANCELLED;
}

void OUTLINER_OT_lib_relocate(wmOperatorType *ot)
{
	ot->name = "Relocate Library";
	ot->idname = "OUTLINER_OT_lib_relocate";
	ot->description = "Relocate the library under cursor";

	ot->invoke = outliner_lib_relocate_invoke;
	ot->poll = ED_operator_outliner_active;
}

/* XXX This does not work with several items
 *     (it is only called once in the end, due to the 'deferred' filebrowser invocation through event system...). */
void lib_relocate_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	wmOperatorType *ot = WM_operatortype_find("WM_OT_lib_relocate", false);

	lib_relocate(C, te, tselem, ot, false);
}


static int outliner_lib_reload_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];

	BLI_assert(ar && soops);

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	for (te = soops->tree.first; te; te = te->next) {
		int ret;

		if ((ret = outliner_lib_relocate_invoke_do(C, op->reports, te, fmval, true))) {
			return ret;
		}
	}

	return OPERATOR_CANCELLED;
}

void OUTLINER_OT_lib_reload(wmOperatorType *ot)
{
	ot->name = "Reload Library";
	ot->idname = "OUTLINER_OT_lib_reload";
	ot->description = "Reload the library under cursor";

	ot->invoke = outliner_lib_reload_invoke;
	ot->poll = ED_operator_outliner_active;
}

void lib_reload_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	wmOperatorType *ot = WM_operatortype_find("WM_OT_lib_reload", false);

	lib_relocate(C, te, tselem, ot, true);
}

/* ************************************************************** */
/* Setting Toggling Operators */

/* =============================================== */
/* Toggling Utilities (Exported) */

/* Apply Settings ------------------------------- */

static int outliner_count_levels(ListBase *lb, const int curlevel)
{
	TreeElement *te;
	int level = curlevel, lev;
	
	for (te = lb->first; te; te = te->next) {
		
		lev = outliner_count_levels(&te->subtree, curlevel + 1);
		if (lev > level) level = lev;
	}
	return level;
}

int outliner_has_one_flag(ListBase *lb, short flag, const int curlevel)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	int level;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->flag & flag) return curlevel;
		
		level = outliner_has_one_flag(&te->subtree, flag, curlevel + 1);
		if (level) return level;
	}
	return 0;
}

void outliner_set_flag(ListBase *lb, short flag, short set)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (set == 0) tselem->flag &= ~flag;
		else tselem->flag |= flag;
		outliner_set_flag(&te->subtree, flag, set);
	}
}

/* Restriction Columns ------------------------------- */

/* same check needed for both object operation and restrict column button func
 * return 0 when in edit mode (cannot restrict view or select)
 * otherwise return 1 */
int common_restrict_check(bContext *C, Object *ob)
{
	/* Don't allow hide an object in edit mode,
	 * check the bug #22153 and #21609, #23977
	 */
	Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit == ob) {
		/* found object is hidden, reset */
		if (ob->restrictflag & OB_RESTRICT_VIEW)
			ob->restrictflag &= ~OB_RESTRICT_VIEW;
		/* found object is unselectable, reset */
		if (ob->restrictflag & OB_RESTRICT_SELECT)
			ob->restrictflag &= ~OB_RESTRICT_SELECT;
		return 0;
	}
	
	return 1;
}

/* =============================================== */
/* Restriction toggles */

/* Toggle Visibility ---------------------------------------- */

void object_toggle_visibility_cb(
        bContext *C, ReportList *reports, Scene *scene, TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Base *base = (Base *)te->directdata;
	Object *ob = (Object *)tselem->id;

	if (ID_IS_LINKED_DATABLOCK(tselem->id)) {
		BKE_report(reports, RPT_WARNING, "Cannot edit external libdata");
		return;
	}

	/* add check for edit mode */
	if (!common_restrict_check(C, ob)) return;
	
	if (base || (base = BKE_scene_base_find(scene, ob))) {
		if ((base->object->restrictflag ^= OB_RESTRICT_VIEW)) {
			ED_base_object_select(base, BA_DESELECT);
		}
	}
}

void group_toggle_visibility_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *scene, TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Group *group = (Group *)tselem->id;
	restrictbutton_gr_restrict_flag(scene, group, OB_RESTRICT_VIEW);
}

static int outliner_toggle_visibility_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	
	outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_toggle_visibility_cb);
	
	DAG_id_type_tag(bmain, ID_OB);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_VISIBLE, scene);
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_visibility_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Visibility";
	ot->idname = "OUTLINER_OT_visibility_toggle";
	ot->description = "Toggle the visibility of selected items";
	
	/* callbacks */
	ot->exec = outliner_toggle_visibility_exec;
	ot->poll = ED_operator_outliner_active_no_editobject;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Toggle Selectability ---------------------------------------- */

void object_toggle_selectability_cb(
        bContext *UNUSED(C), ReportList *reports, Scene *scene, TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Base *base = (Base *)te->directdata;

	if (ID_IS_LINKED_DATABLOCK(tselem->id)) {
		BKE_report(reports, RPT_WARNING, "Cannot edit external libdata");
		return;
	}

	if (base == NULL) base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base) {
		base->object->restrictflag ^= OB_RESTRICT_SELECT;
	}
}

void group_toggle_selectability_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *scene, TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Group *group = (Group *)tselem->id;
	restrictbutton_gr_restrict_flag(scene, group, OB_RESTRICT_SELECT);
}

static int outliner_toggle_selectability_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	
	outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_toggle_selectability_cb);
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_selectability_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Selectability";
	ot->idname = "OUTLINER_OT_selectability_toggle";
	ot->description = "Toggle the selectability";
	
	/* callbacks */
	ot->exec = outliner_toggle_selectability_exec;
	ot->poll = ED_operator_outliner_active_no_editobject;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* Toggle Renderability ---------------------------------------- */

void object_toggle_renderability_cb(
        bContext *UNUSED(C), ReportList *reports, Scene *scene, TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Base *base = (Base *)te->directdata;

	if (ID_IS_LINKED_DATABLOCK(tselem->id)) {
		BKE_report(reports, RPT_WARNING, "Cannot edit external libdata");
		return;
	}

	if (base == NULL) base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base) {
		base->object->restrictflag ^= OB_RESTRICT_RENDER;
	}
}

void group_toggle_renderability_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *scene, TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Group *group = (Group *)tselem->id;
	restrictbutton_gr_restrict_flag(scene, group, OB_RESTRICT_RENDER);
}

static int outliner_toggle_renderability_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	
	outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_toggle_renderability_cb);
	
	DAG_id_type_tag(bmain, ID_OB);
	WM_event_add_notifier(C, NC_SCENE | ND_OB_RENDER, scene);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_renderability_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Renderability";
	ot->idname = "OUTLINER_OT_renderability_toggle";
	ot->description = "Toggle the renderability of selected items";
	
	/* callbacks */
	ot->exec = outliner_toggle_renderability_exec;
	ot->poll = ED_operator_outliner_active;
	
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* =============================================== */
/* Outliner setting toggles */

/* Toggle Expanded (Outliner) ---------------------------------------- */

static int outliner_toggle_expanded_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	
	if (outliner_has_one_flag(&soops->tree, TSE_CLOSED, 1))
		outliner_set_flag(&soops->tree, TSE_CLOSED, 0);
	else 
		outliner_set_flag(&soops->tree, TSE_CLOSED, 1);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_expanded_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Expand/Collapse All";
	ot->idname = "OUTLINER_OT_expanded_toggle";
	ot->description = "Expand/Collapse all items";
	
	/* callbacks */
	ot->exec = outliner_toggle_expanded_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* no undo or registry, UI option */
}

/* Toggle Selected (Outliner) ---------------------------------------- */

static int outliner_toggle_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	
	if (outliner_has_one_flag(&soops->tree, TSE_SELECTED, 1))
		outliner_set_flag(&soops->tree, TSE_SELECTED, 0);
	else 
		outliner_set_flag(&soops->tree, TSE_SELECTED, 1);
	
	soops->storeflag |= SO_TREESTORE_REDRAW;
	
	WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_selected_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Toggle Selected";
	ot->idname = "OUTLINER_OT_selected_toggle";
	ot->description = "Toggle the Outliner selection of items";
	
	/* callbacks */
	ot->exec = outliner_toggle_selected_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* no undo or registry, UI option */
}

/* ************************************************************** */
/* Hotkey Only Operators */

/* Show Active --------------------------------------------------- */

static void outliner_set_coordinates_element_recursive(SpaceOops *soops, TreeElement *te, int startx, int *starty)
{
	TreeStoreElem *tselem = TREESTORE(te);

	/* store coord and continue, we need coordinates for elements outside view too */
	te->xs = (float)startx;
	te->ys = (float)(*starty);
	*starty -= UI_UNIT_Y;

	if (TSELEM_OPEN(tselem, soops)) {
		TreeElement *ten;
		for (ten = te->subtree.first; ten; ten = ten->next) {
			outliner_set_coordinates_element_recursive(soops, ten, startx + UI_UNIT_X, starty);
		}
	}
}

/* to retrieve coordinates with redrawing the entire tree */
static void outliner_set_coordinates(ARegion *ar, SpaceOops *soops)
{
	TreeElement *te;
	int starty = (int)(ar->v2d.tot.ymax) - UI_UNIT_Y;

	for (te = soops->tree.first; te; te = te->next) {
		outliner_set_coordinates_element_recursive(soops, te, 0, &starty);
	}
}

/* return 1 when levels were opened */
static int outliner_open_back(TreeElement *te)
{
	TreeStoreElem *tselem;
	int retval = 0;

	for (te = te->parent; te; te = te->parent) {
		tselem = TREESTORE(te);
		if (tselem->flag & TSE_CLOSED) {
			tselem->flag &= ~TSE_CLOSED;
			retval = 1;
		}
	}
	return retval;
}

static int outliner_show_active_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *so = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	View2D *v2d = &ar->v2d;
	
	TreeElement *te;
	int xdelta, ytop;

	Object *obact = OBACT;

	if (!obact)
		return OPERATOR_CANCELLED;


	te = outliner_find_id(so, &so->tree, &obact->id);

	if (te != NULL && obact->type == OB_ARMATURE) {
		/* traverse down the bone hierarchy in case of armature */
		TreeElement *te_obact = te;

		if (obact->mode & OB_MODE_POSE) {
			bPoseChannel *pchan = CTX_data_active_pose_bone(C);
			if (pchan) {
				te = outliner_find_posechannel(&te_obact->subtree, pchan);
			}
		}
		else if (obact->mode & OB_MODE_EDIT) {
			EditBone *ebone = CTX_data_active_bone(C);
			if (ebone) {
				te = outliner_find_editbone(&te_obact->subtree, ebone);
			}
		}
	}

	if (te) {
		/* open up tree to active object/bone */
		if (outliner_open_back(te)) {
			outliner_set_coordinates(ar, so);
		}

		/* make te->ys center of view */
		ytop = te->ys + BLI_rcti_size_y(&v2d->mask) / 2;
		if (ytop > 0) ytop = 0;
		
		v2d->cur.ymax = (float)ytop;
		v2d->cur.ymin = (float)(ytop - BLI_rcti_size_y(&v2d->mask));
		
		/* make te->xs ==> te->xend center of view */
		xdelta = (int)(te->xs - v2d->cur.xmin);
		v2d->cur.xmin += xdelta;
		v2d->cur.xmax += xdelta;
		
		so->storeflag |= SO_TREESTORE_REDRAW;
	}
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_active(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show Active";
	ot->idname = "OUTLINER_OT_show_active";
	ot->description = "Open up the tree and adjust the view so that the active Object is shown centered";
	
	/* callbacks */
	ot->exec = outliner_show_active_exec;
	ot->poll = ED_operator_outliner_active;
}

/* View Panning --------------------------------------------------- */

static int outliner_scroll_page_exec(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);
	int dy = BLI_rcti_size_y(&ar->v2d.mask);
	int up = 0;
	
	if (RNA_boolean_get(op->ptr, "up"))
		up = 1;

	if (up == 0) dy = -dy;
	ar->v2d.cur.ymin += dy;
	ar->v2d.cur.ymax += dy;
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_scroll_page(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Scroll Page";
	ot->idname = "OUTLINER_OT_scroll_page";
	ot->description = "Scroll page up or down";
	
	/* callbacks */
	ot->exec = outliner_scroll_page_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* properties */
	prop = RNA_def_boolean(ot->srna, "up", 0, "Up", "Scroll up one page");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* Search ------------------------------------------------------- */
// TODO: probably obsolete now with filtering?

#if 0

/* find next element that has this name */
static TreeElement *outliner_find_name(SpaceOops *soops, ListBase *lb, char *name, int flags,
                                       TreeElement *prev, int *prevFound)
{
	TreeElement *te, *tes;
	
	for (te = lb->first; te; te = te->next) {
		int found = outliner_filter_has_name(te, name, flags);
		
		if (found) {
			/* name is right, but is element the previous one? */
			if (prev) {
				if ((te != prev) && (*prevFound)) 
					return te;
				if (te == prev) {
					*prevFound = 1;
				}
			}
			else
				return te;
		}
		
		tes = outliner_find_name(soops, &te->subtree, name, flags, prev, prevFound);
		if (tes) return tes;
	}

	/* nothing valid found */
	return NULL;
}

static void outliner_find_panel(Scene *UNUSED(scene), ARegion *ar, SpaceOops *soops, int again, int flags) 
{
	ReportList *reports = NULL; // CTX_wm_reports(C);
	TreeElement *te = NULL;
	TreeElement *last_find;
	TreeStoreElem *tselem;
	int ytop, xdelta, prevFound = 0;
	char name[sizeof(soops->search_string)];
	
	/* get last found tree-element based on stored search_tse */
	last_find = outliner_find_tse(soops, &soops->search_tse);
	
	/* determine which type of search to do */
	if (again && last_find) {
		/* no popup panel - previous + user wanted to search for next after previous */
		BLI_strncpy(name, soops->search_string, sizeof(name));
		flags = soops->search_flags;
		
		/* try to find matching element */
		te = outliner_find_name(soops, &soops->tree, name, flags, last_find, &prevFound);
		if (te == NULL) {
			/* no more matches after previous, start from beginning again */
			prevFound = 1;
			te = outliner_find_name(soops, &soops->tree, name, flags, last_find, &prevFound);
		}
	}
	else {
		/* pop up panel - no previous, or user didn't want search after previous */
		name[0] = '\0';
// XXX		if (sbutton(name, 0, sizeof(name) - 1, "Find: ") && name[0]) {
//			te = outliner_find_name(soops, &soops->tree, name, flags, NULL, &prevFound);
//		}
//		else return; /* XXX RETURN! XXX */
	}

	/* do selection and reveal */
	if (te) {
		tselem = TREESTORE(te);
		if (tselem) {
			/* expand branches so that it will be visible, we need to get correct coordinates */
			if (outliner_open_back(soops, te))
				outliner_set_coordinates(ar, soops);
			
			/* deselect all visible, and select found element */
			outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
			tselem->flag |= TSE_SELECTED;
			
			/* make te->ys center of view */
			ytop = (int)(te->ys + BLI_rctf_size_y(&ar->v2d.mask) / 2);
			if (ytop > 0) ytop = 0;
			ar->v2d.cur.ymax = (float)ytop;
			ar->v2d.cur.ymin = (float)(ytop - BLI_rctf_size_y(&ar->v2d.mask));
			
			/* make te->xs ==> te->xend center of view */
			xdelta = (int)(te->xs - ar->v2d.cur.xmin);
			ar->v2d.cur.xmin += xdelta;
			ar->v2d.cur.xmax += xdelta;
			
			/* store selection */
			soops->search_tse = *tselem;
			
			BLI_strncpy(soops->search_string, name, sizeof(soops->search_string));
			soops->search_flags = flags;
			
			/* redraw */
			soops->storeflag |= SO_TREESTORE_REDRAW;
		}
	}
	else {
		/* no tree-element found */
		BKE_reportf(reports, RPT_WARNING, "Not found: %s", name);
	}
}
#endif

/* Show One Level ----------------------------------------------- */

/* helper function for Show/Hide one level operator */
static void outliner_openclose_level(ListBase *lb, int curlevel, int level, int open)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		
		if (open) {
			if (curlevel <= level) tselem->flag &= ~TSE_CLOSED;
		}
		else {
			if (curlevel >= level) tselem->flag |= TSE_CLOSED;
		}
		
		outliner_openclose_level(&te->subtree, curlevel + 1, level, open);
	}
}

static int outliner_one_level_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	const bool add = RNA_boolean_get(op->ptr, "open");
	int level;
	
	level = outliner_has_one_flag(&soops->tree, TSE_CLOSED, 1);
	if (add == 1) {
		if (level) outliner_openclose_level(&soops->tree, 1, level, 1);
	}
	else {
		if (level == 0) level = outliner_count_levels(&soops->tree, 0);
		if (level) outliner_openclose_level(&soops->tree, 1, level - 1, 0);
	}
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_one_level(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Show/Hide One Level";
	ot->idname = "OUTLINER_OT_show_one_level";
	ot->description = "Expand/collapse all entries by one level";
	
	/* callbacks */
	ot->exec = outliner_one_level_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* no undo or registry, UI option */
	
	/* properties */
	prop = RNA_def_boolean(ot->srna, "open", 1, "Open", "Expand all entries one level deep");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* Show Hierarchy ----------------------------------------------- */

/* helper function for tree_element_shwo_hierarchy() - recursively checks whether subtrees have any objects*/
static int subtree_has_objects(ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->type == 0 && te->idcode == ID_OB) return 1;
		if (subtree_has_objects(&te->subtree)) return 1;
	}
	return 0;
}

/* recursive helper function for Show Hierarchy operator */
static void tree_element_show_hierarchy(Scene *scene, SpaceOops *soops, ListBase *lb)
{
	TreeElement *te;
	TreeStoreElem *tselem;

	/* open all object elems, close others */
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		
		if (tselem->type == 0) {
			if (te->idcode == ID_SCE) {
				if (tselem->id != (ID *)scene) tselem->flag |= TSE_CLOSED;
				else tselem->flag &= ~TSE_CLOSED;
			}
			else if (te->idcode == ID_OB) {
				if (subtree_has_objects(&te->subtree)) tselem->flag &= ~TSE_CLOSED;
				else tselem->flag |= TSE_CLOSED;
			}
		}
		else {
			tselem->flag |= TSE_CLOSED;
		}

		if (TSELEM_OPEN(tselem, soops)) {
			tree_element_show_hierarchy(scene, soops, &te->subtree);
		}
	}
}

/* show entire object level hierarchy */
static int outliner_show_hierarchy_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	
	/* recursively open/close levels */
	tree_element_show_hierarchy(scene, soops, &soops->tree);
	
	ED_region_tag_redraw(ar);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_show_hierarchy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Show Hierarchy";
	ot->idname = "OUTLINER_OT_show_hierarchy";
	ot->description = "Open all object entries and close all others";
	
	/* callbacks */
	ot->exec = outliner_show_hierarchy_exec;
	ot->poll = ED_operator_outliner_active; //  TODO: shouldn't be allowed in RNA views...
	
	/* no undo or registry, UI option */
}

/* ************************************************************** */
/* ANIMATO OPERATIONS */
/* KeyingSet and Driver Creation - Helper functions */

/* specialized poll callback for these operators to work in Datablocks view only */
static int ed_operator_outliner_datablocks_active(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	if ((sa) && (sa->spacetype == SPACE_OUTLINER)) {
		SpaceOops *so = CTX_wm_space_outliner(C);
		return (so->outlinevis == SO_DATABLOCKS);
	}
	return 0;
}


/* Helper func to extract an RNA path from selected tree element 
 * NOTE: the caller must zero-out all values of the pointers that it passes here first, as
 * this function does not do that yet 
 */
static void tree_element_to_path(TreeElement *te, TreeStoreElem *tselem,
                                 ID **id, char **path, int *array_index, short *flag, short *UNUSED(groupmode))
{
	ListBase hierarchy = {NULL, NULL};
	LinkData *ld;
	TreeElement *tem, *temnext, *temsub;
	TreeStoreElem *tse /* , *tsenext */ /* UNUSED */;
	PointerRNA *ptr, *nextptr;
	PropertyRNA *prop;
	char *newpath = NULL;
	
	/* optimize tricks:
	 *	- Don't do anything if the selected item is a 'struct', but arrays are allowed
	 */
	if (tselem->type == TSE_RNA_STRUCT)
		return;
	
	/* Overview of Algorithm:
	 *  1. Go up the chain of parents until we find the 'root', taking note of the
	 *	   levels encountered in reverse-order (i.e. items are added to the start of the list
	 *      for more convenient looping later)
	 *  2. Walk down the chain, adding from the first ID encountered
	 *	   (which will become the 'ID' for the KeyingSet Path), and build a  
	 *      path as we step through the chain
	 */
	 
	/* step 1: flatten out hierarchy of parents into a flat chain */
	for (tem = te->parent; tem; tem = tem->parent) {
		ld = MEM_callocN(sizeof(LinkData), "LinkData for tree_element_to_path()");
		ld->data = tem;
		BLI_addhead(&hierarchy, ld);
	}
	
	/* step 2: step down hierarchy building the path
	 * (NOTE: addhead in previous loop was needed so that we can loop like this) */
	for (ld = hierarchy.first; ld; ld = ld->next) {
		/* get data */
		tem = (TreeElement *)ld->data;
		tse = TREESTORE(tem);
		ptr = &tem->rnaptr;
		prop = tem->directdata;
		
		/* check if we're looking for first ID, or appending to path */
		if (*id) {
			/* just 'append' property to path 
			 * - to prevent memory leaks, we must write to newpath not path, then free old path + swap them
			 */
			if (tse->type == TSE_RNA_PROPERTY) {
				if (RNA_property_type(prop) == PROP_POINTER) {
					/* for pointer we just append property name */
					newpath = RNA_path_append(*path, ptr, prop, 0, NULL);
				}
				else if (RNA_property_type(prop) == PROP_COLLECTION) {
					char buf[128], *name;
					
					temnext = (TreeElement *)(ld->next->data);
					/* tsenext = TREESTORE(temnext); */ /* UNUSED */
					
					nextptr = &temnext->rnaptr;
					name = RNA_struct_name_get_alloc(nextptr, buf, sizeof(buf), NULL);
					
					if (name) {
						/* if possible, use name as a key in the path */
						newpath = RNA_path_append(*path, NULL, prop, 0, name);
						
						if (name != buf)
							MEM_freeN(name);
					}
					else {
						/* otherwise use index */
						int index = 0;
						
						for (temsub = tem->subtree.first; temsub; temsub = temsub->next, index++)
							if (temsub == temnext)
								break;
						
						newpath = RNA_path_append(*path, NULL, prop, index, NULL);
					}
					
					ld = ld->next;
				}
			}
			
			if (newpath) {
				if (*path) MEM_freeN(*path);
				*path = newpath;
				newpath = NULL;
			}
		}
		else {
			/* no ID, so check if entry is RNA-struct, and if that RNA-struct is an ID datablock to extract info from */
			if (tse->type == TSE_RNA_STRUCT) {
				/* ptr->data not ptr->id.data seems to be the one we want,
				 * since ptr->data is sometimes the owner of this ID? */
				if (RNA_struct_is_ID(ptr->type)) {
					*id = (ID *)ptr->data;
					
					/* clear path */
					if (*path) {
						MEM_freeN(*path);
						path = NULL;
					}
				}
			}
		}
	}

	/* step 3: if we've got an ID, add the current item to the path */
	if (*id) {
		/* add the active property to the path */
		ptr = &te->rnaptr;
		prop = te->directdata;
		
		/* array checks */
		if (tselem->type == TSE_RNA_ARRAY_ELEM) {
			/* item is part of an array, so must set the array_index */
			*array_index = te->index;
		}
		else if (RNA_property_array_check(prop)) {
			/* entire array was selected, so keyframe all */
			*flag |= KSP_FLAG_WHOLE_ARRAY;
		}
		
		/* path */
		newpath = RNA_path_append(*path, NULL, prop, 0, NULL);
		if (*path) MEM_freeN(*path);
		*path = newpath;
	}

	/* free temp data */
	BLI_freelistN(&hierarchy);
}

/* =============================================== */
/* Driver Operations */

/* These operators are only available in databrowser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
	DRIVERS_EDITMODE_ADD    = 0,
	DRIVERS_EDITMODE_REMOVE,
} /*eDrivers_EditModes*/;

/* Utilities ---------------------------------- */ 

/* Recursively iterate over tree, finding and working on selected items */
static void do_outliner_drivers_editop(SpaceOops *soops, ListBase *tree, ReportList *reports, short mode)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = tree->first; te; te = te->next) {
		tselem = TREESTORE(te);
		
		/* if item is selected, perform operation */
		if (tselem->flag & TSE_SELECTED) {
			ID *id = NULL;
			char *path = NULL;
			int array_index = 0;
			short flag = 0;
			short groupmode = KSP_GROUP_KSNAME;
			
			/* check if RNA-property described by this selected element is an animatable prop */
			if (ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM) &&
			    RNA_property_animateable(&te->rnaptr, te->directdata))
			{
				/* get id + path + index info from the selected element */
				tree_element_to_path(te, tselem,
				                     &id, &path, &array_index, &flag, &groupmode);
			}
			
			/* only if ID and path were set, should we perform any actions */
			if (id && path) {
				short dflags = CREATEDRIVER_WITH_DEFAULT_DVAR;
				int arraylen = 1;
				
				/* array checks */
				if (flag & KSP_FLAG_WHOLE_ARRAY) {
					/* entire array was selected, so add drivers for all */
					arraylen = RNA_property_array_length(&te->rnaptr, te->directdata);
				}
				else
					arraylen = array_index;
				
				/* we should do at least one step */
				if (arraylen == array_index)
					arraylen++;
				
				/* for each array element we should affect, add driver */
				for (; array_index < arraylen; array_index++) {
					/* action depends on mode */
					switch (mode) {
						case DRIVERS_EDITMODE_ADD:
						{
							/* add a new driver with the information obtained (only if valid) */
							ANIM_add_driver(reports, id, path, array_index, dflags, DRIVER_TYPE_PYTHON);
							break;
						}
						case DRIVERS_EDITMODE_REMOVE:
						{
							/* remove driver matching the information obtained (only if valid) */
							ANIM_remove_driver(reports, id, path, array_index, dflags);
							break;
						}
					}
				}
				
				/* free path, since it had to be generated */
				MEM_freeN(path);
			}
			
			
		}
		
		/* go over sub-tree */
		if (TSELEM_OPEN(tselem, soops))
			do_outliner_drivers_editop(soops, &te->subtree, reports, mode);
	}
}

/* Add Operator ---------------------------------- */

static int outliner_drivers_addsel_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner = CTX_wm_space_outliner(C);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_drivers_editop(soutliner, &soutliner->tree, op->reports, DRIVERS_EDITMODE_ADD);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, NULL); // XXX
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_add_selected(wmOperatorType *ot)
{
	/* api callbacks */
	ot->idname = "OUTLINER_OT_drivers_add_selected";
	ot->name = "Add Drivers for Selected";
	ot->description = "Add drivers to selected items";
	
	/* api callbacks */
	ot->exec = outliner_drivers_addsel_exec;
	ot->poll = ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* Remove Operator ---------------------------------- */

static int outliner_drivers_deletesel_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner = CTX_wm_space_outliner(C);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_drivers_editop(soutliner, &soutliner->tree, op->reports, DRIVERS_EDITMODE_REMOVE);
	
	/* send notifiers */
	WM_event_add_notifier(C, ND_KEYS, NULL); // XXX
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_drivers_delete_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "OUTLINER_OT_drivers_delete_selected";
	ot->name = "Delete Drivers for Selected";
	ot->description = "Delete drivers assigned to selected items";
	
	/* api callbacks */
	ot->exec = outliner_drivers_deletesel_exec;
	ot->poll = ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* =============================================== */
/* Keying Set Operations */

/* These operators are only available in databrowser mode for now, as
 * they depend on having RNA paths and/or hierarchies available.
 */
enum {
	KEYINGSET_EDITMODE_ADD  = 0,
	KEYINGSET_EDITMODE_REMOVE,
} /*eKeyingSet_EditModes*/;

/* Utilities ---------------------------------- */ 
 
/* find the 'active' KeyingSet, and add if not found (if adding is allowed) */
// TODO: should this be an API func?
static KeyingSet *verify_active_keyingset(Scene *scene, short add)
{
	KeyingSet *ks = NULL;
	
	/* sanity check */
	if (scene == NULL)
		return NULL;
	
	/* try to find one from scene */
	if (scene->active_keyingset > 0)
		ks = BLI_findlink(&scene->keyingsets, scene->active_keyingset - 1);
		
	/* add if none found */
	// XXX the default settings have yet to evolve
	if ((add) && (ks == NULL)) {
		ks = BKE_keyingset_add(&scene->keyingsets, NULL, NULL, KEYINGSET_ABSOLUTE, 0);
		scene->active_keyingset = BLI_listbase_count(&scene->keyingsets);
	}
	
	return ks;
}

/* Recursively iterate over tree, finding and working on selected items */
static void do_outliner_keyingset_editop(SpaceOops *soops, KeyingSet *ks, ListBase *tree, short mode)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = tree->first; te; te = te->next) {
		tselem = TREESTORE(te);
		
		/* if item is selected, perform operation */
		if (tselem->flag & TSE_SELECTED) {
			ID *id = NULL;
			char *path = NULL;
			int array_index = 0;
			short flag = 0;
			short groupmode = KSP_GROUP_KSNAME;
			
			/* check if RNA-property described by this selected element is an animatable prop */
			if (ELEM(tselem->type, TSE_RNA_PROPERTY, TSE_RNA_ARRAY_ELEM) &&
			    RNA_property_animateable(&te->rnaptr, te->directdata))
			{
				/* get id + path + index info from the selected element */
				tree_element_to_path(te, tselem,
				                     &id, &path, &array_index, &flag, &groupmode);
			}
			
			/* only if ID and path were set, should we perform any actions */
			if (id && path) {
				/* action depends on mode */
				switch (mode) {
					case KEYINGSET_EDITMODE_ADD:
					{
						/* add a new path with the information obtained (only if valid) */
						/* TODO: what do we do with group name?
						 * for now, we don't supply one, and just let this use the KeyingSet name */
						BKE_keyingset_add_path(ks, id, NULL, path, array_index, flag, groupmode);
						ks->active_path = BLI_listbase_count(&ks->paths);
						break;
					}
					case KEYINGSET_EDITMODE_REMOVE:
					{
						/* find the relevant path, then remove it from the KeyingSet */
						KS_Path *ksp = BKE_keyingset_find_path(ks, id, NULL, path, array_index, groupmode);
						
						if (ksp) {
							/* free path's data */
							BKE_keyingset_free_path(ks, ksp);

							ks->active_path = 0;
						}
						break;
					}
				}
				
				/* free path, since it had to be generated */
				MEM_freeN(path);
			}
		}
		
		/* go over sub-tree */
		if (TSELEM_OPEN(tselem, soops))
			do_outliner_keyingset_editop(soops, ks, &te->subtree, mode);
	}
}

/* Add Operator ---------------------------------- */

static int outliner_keyingset_additems_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soutliner = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks = verify_active_keyingset(scene, 1);
	
	/* check for invalid states */
	if (ks == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Operation requires an active keying set");
		return OPERATOR_CANCELLED;
	}
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_keyingset_editop(soutliner, ks, &soutliner->tree, KEYINGSET_EDITMODE_ADD);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_add_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "OUTLINER_OT_keyingset_add_selected";
	ot->name = "Keying Set Add Selected";
	ot->description = "Add selected items (blue-gray rows) to active Keying Set";
	
	/* api callbacks */
	ot->exec = outliner_keyingset_additems_exec;
	ot->poll = ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* Remove Operator ---------------------------------- */

static int outliner_keyingset_removeitems_exec(bContext *C, wmOperator *UNUSED(op))
{
	SpaceOops *soutliner = CTX_wm_space_outliner(C);
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks = verify_active_keyingset(scene, 1);
	
	/* check for invalid states */
	if (soutliner == NULL)
		return OPERATOR_CANCELLED;
	
	/* recursively go into tree, adding selected items */
	do_outliner_keyingset_editop(soutliner, ks, &soutliner->tree, KEYINGSET_EDITMODE_REMOVE);
	
	/* send notifiers */
	WM_event_add_notifier(C, NC_SCENE | ND_KEYINGSET, NULL);
	
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_keyingset_remove_selected(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "OUTLINER_OT_keyingset_remove_selected";
	ot->name = "Keying Set Remove Selected";
	ot->description = "Remove selected items (blue-gray rows) from active Keying Set";
	
	/* api callbacks */
	ot->exec = outliner_keyingset_removeitems_exec;
	ot->poll = ed_operator_outliner_datablocks_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/* ************************************************************** */
/* ORPHANED DATABLOCKS */

static int ed_operator_outliner_id_orphans_active(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	if ((sa) && (sa->spacetype == SPACE_OUTLINER)) {
		SpaceOops *so = CTX_wm_space_outliner(C);
		return (so->outlinevis == SO_ID_ORPHANS);
	}
	return 0;
}

/* Purge Orphans Operator --------------------------------------- */

static int outliner_orphans_purge_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(evt))
{
	/* present a prompt to informing users that this change is irreversible */
	return WM_operator_confirm_message(C, op,
	                                   "Purging unused data-blocks cannot be undone and saves to current .blend file. "
	                                   "Click here to proceed...");
}

static int outliner_orphans_purge_exec(bContext *C, wmOperator *UNUSED(op))
{
	/* Firstly, ensure that the file has been saved,
	 * so that the latest changes since the last save
	 * are retained...
	 */
	WM_operator_name_call(C, "WM_OT_save_mainfile", WM_OP_EXEC_DEFAULT, NULL);
	
	/* Now, reload the file to get rid of the orphans... */
	WM_operator_name_call(C, "WM_OT_revert_mainfile", WM_OP_EXEC_DEFAULT, NULL);
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_orphans_purge(wmOperatorType *ot)
{
	/* identifiers */
	ot->idname = "OUTLINER_OT_orphans_purge";
	ot->name = "Purge All";
	ot->description = "Clear all orphaned data-blocks without any users from the file "
	                  "(cannot be undone, saves to current .blend file)";
	
	/* callbacks */
	ot->invoke = outliner_orphans_purge_invoke;
	ot->exec = outliner_orphans_purge_exec;
	ot->poll = ed_operator_outliner_id_orphans_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ************************************************************** */
/* DRAG AND DROP OPERATORS */

/* ******************** Parent Drop Operator *********************** */

static int parent_drop_exec(bContext *C, wmOperator *op)
{
	Object *par = NULL, *ob = NULL;
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	int partype = -1;
	char parname[MAX_ID_NAME], childname[MAX_ID_NAME];

	partype = RNA_enum_get(op->ptr, "type");
	RNA_string_get(op->ptr, "parent", parname);
	par = (Object *)BKE_libblock_find_name(ID_OB, parname);
	RNA_string_get(op->ptr, "child", childname);
	ob = (Object *)BKE_libblock_find_name(ID_OB, childname);

	if (ID_IS_LINKED_DATABLOCK(ob)) {
		BKE_report(op->reports, RPT_INFO, "Can't edit library linked object");
		return OPERATOR_CANCELLED;
	}

	ED_object_parent_set(op->reports, bmain, scene, ob, par, partype, false, false, NULL);

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);

	return OPERATOR_FINISHED;
}

static int parent_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *par = NULL;
	Object *ob = NULL;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Main *bmain = CTX_data_main(C);
	Scene *scene = NULL;
	TreeElement *te = NULL;
	char childname[MAX_ID_NAME];
	char parname[MAX_ID_NAME];
	int partype = 0;
	float fmval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	/* Find object hovered over */
	te = outliner_dropzone_find(soops, fmval, true);

	if (te) {
		RNA_string_set(op->ptr, "parent", te->name);
		/* Identify parent and child */
		RNA_string_get(op->ptr, "child", childname);
		ob = (Object *)BKE_libblock_find_name(ID_OB, childname);
		RNA_string_get(op->ptr, "parent", parname);
		par = (Object *)BKE_libblock_find_name(ID_OB, parname);
		
		if (ELEM(NULL, ob, par)) {
			if (par == NULL) printf("par==NULL\n");
			return OPERATOR_CANCELLED;
		}
		if (ob == par) {
			return OPERATOR_CANCELLED;
		}
		if (ID_IS_LINKED_DATABLOCK(ob)) {
			BKE_report(op->reports, RPT_INFO, "Can't edit library linked object");
			return OPERATOR_CANCELLED;
		}
		
		scene = (Scene *)outliner_search_back(soops, te, ID_SCE);

		if (scene == NULL) {
			/* currently outlier organized in a way, that if there's no parent scene
			 * element for object it means that all displayed objects belong to
			 * active scene and parenting them is allowed (sergey)
			 */

			scene = CTX_data_scene(C);
		}

		if ((par->type != OB_ARMATURE) && (par->type != OB_CURVE) && (par->type != OB_LATTICE)) {
			if (ED_object_parent_set(op->reports, bmain, scene, ob, par, partype, false, false, NULL)) {
				DAG_relations_tag_update(bmain);
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
			ptr = uiItemFullO_ptr(layout, ot, IFACE_("Object"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
			RNA_string_set(&ptr, "parent", parname);
			RNA_string_set(&ptr, "child", childname);
			RNA_enum_set(&ptr, "type", PAR_OBJECT);

			/* par becomes parent, make the associated menus */
			if (par->type == OB_ARMATURE) {
				ptr = uiItemFullO_ptr(layout, ot, IFACE_("Armature Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_ARMATURE);

				ptr = uiItemFullO_ptr(layout, ot, IFACE_("   With Empty Groups"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_ARMATURE_NAME);

				ptr = uiItemFullO_ptr(layout, ot, IFACE_("   With Envelope Weights"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_ARMATURE_ENVELOPE);

				ptr = uiItemFullO_ptr(layout, ot, IFACE_("   With Automatic Weights"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_ARMATURE_AUTO);

				ptr = uiItemFullO_ptr(layout, ot, IFACE_("Bone"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_BONE);
			}
			else if (par->type == OB_CURVE) {
				ptr = uiItemFullO_ptr(layout, ot, IFACE_("Curve Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_CURVE);

				ptr = uiItemFullO_ptr(layout, ot, IFACE_("Follow Path"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_FOLLOW);

				ptr = uiItemFullO_ptr(layout, ot, IFACE_("Path Constraint"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_PATH_CONST);
			}
			else if (par->type == OB_LATTICE) {
				ptr = uiItemFullO_ptr(layout, ot, IFACE_("Lattice Deform"), 0, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
				RNA_string_set(&ptr, "parent", parname);
				RNA_string_set(&ptr, "child", childname);
				RNA_enum_set(&ptr, "type", PAR_LATTICE);
			}
			
			UI_popup_menu_end(C, pup);
			
			return OPERATOR_INTERFACE;
		}
	}
	else {
		return OPERATOR_CANCELLED;
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

static int outliner_parenting_poll(bContext *C)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);

	if (soops) {
		return ELEM(soops->outlinevis, SO_ALL_SCENES, SO_CUR_SCENE, SO_VISIBLE, SO_GROUPS);
	}

	return false;
}

static int parent_clear_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Main *bmain = CTX_data_main(C);
	Object *ob = NULL;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	char obname[MAX_ID_NAME];

	RNA_string_get(op->ptr, "dragged_obj", obname);
	ob = (Object *)BKE_libblock_find_name(ID_OB, obname);

	/* search forwards to find the object */
	outliner_find_id(soops, &soops->tree, (ID *)ob);

	ED_object_parent_clear(ob, RNA_enum_get(op->ptr, "type"));

	DAG_relations_tag_update(bmain);
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

	ot->poll = outliner_parenting_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_string(ot->srna, "dragged_obj", "Object", MAX_ID_NAME, "Child", "Child Object");
	RNA_def_enum(ot->srna, "type", prop_clear_parent_types, 0, "Type", "");
}

static int scene_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Scene *scene = NULL;
	Object *ob = NULL;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	Main *bmain = CTX_data_main(C);
	TreeElement *te = NULL;
	char obname[MAX_ID_NAME];
	float fmval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	/* Find object hovered over */
	te = outliner_dropzone_find(soops, fmval, false);

	if (te) {
		Base *base;

		RNA_string_set(op->ptr, "scene", te->name);
		scene = (Scene *)BKE_libblock_find_name(ID_SCE, te->name);

		RNA_string_get(op->ptr, "object", obname);
		ob = (Object *)BKE_libblock_find_name(ID_OB, obname);

		if (ELEM(NULL, ob, scene) || ID_IS_LINKED_DATABLOCK(scene)) {
			return OPERATOR_CANCELLED;
		}

		base = ED_object_scene_link(scene, ob);

		if (base == NULL) {
			return OPERATOR_CANCELLED;
		}

		if (scene == CTX_data_scene(C)) {
			/* when linking to an inactive scene don't touch the layer */
			ob->lay = base->lay;
			ED_base_object_select(base, BA_SELECT);
		}

		DAG_relations_tag_update(bmain);

		WM_main_add_notifier(NC_SCENE | ND_OB_SELECT, scene);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
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

	/* properties */
	RNA_def_string(ot->srna, "object", "Object", MAX_ID_NAME, "Object", "Target Object");
	RNA_def_string(ot->srna, "scene", "Scene", MAX_ID_NAME, "Scene", "Target Scene");
}

static int material_drop_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Material *ma = NULL;
	Object *ob = NULL;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	TreeElement *te = NULL;
	char mat_name[MAX_ID_NAME - 2];
	float fmval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	/* Find object hovered over */
	te = outliner_dropzone_find(soops, fmval, true);

	if (te) {
		RNA_string_set(op->ptr, "object", te->name);
		ob = (Object *)BKE_libblock_find_name(ID_OB, te->name);

		RNA_string_get(op->ptr, "material", mat_name);
		ma = (Material *)BKE_libblock_find_name(ID_MA, mat_name);

		if (ELEM(NULL, ob, ma)) {
			return OPERATOR_CANCELLED;
		}

		assign_material(ob, ma, ob->totcol + 1, BKE_MAT_ASSIGN_USERPREF);

		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
		WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
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

	/* properties */
	RNA_def_string(ot->srna, "object", "Object", MAX_ID_NAME, "Object", "Target Object");
	RNA_def_string(ot->srna, "material", "Material", MAX_ID_NAME, "Material", "Target Material");
}

static int group_link_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Main *bmain = CTX_data_main(C);
	Group *group = NULL;
	Object *ob = NULL;
	Scene *scene = CTX_data_scene(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ARegion *ar = CTX_wm_region(C);
	TreeElement *te = NULL;
	char ob_name[MAX_ID_NAME - 2];
	float fmval[2];

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);

	/* Find object hovered over */
	te = outliner_dropzone_find(soops, fmval, true);

	if (te) {
		group = (Group *)BKE_libblock_find_name(ID_GR, te->name);

		RNA_string_get(op->ptr, "object", ob_name);
		ob = (Object *)BKE_libblock_find_name(ID_OB, ob_name);

		if (ELEM(NULL, group, ob)) {
			return OPERATOR_CANCELLED;
		}
		if (BKE_group_object_exists(group, ob)) {
			return OPERATOR_FINISHED;
		}

		if (BKE_group_object_cyclic_check(bmain, ob, group)) {
			BKE_report(op->reports, RPT_ERROR, "Could not add the group because of dependency cycle detected");
			return OPERATOR_CANCELLED;
		}

		BKE_group_object_add(group, ob, scene, NULL);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

		return OPERATOR_FINISHED;
	}

	return OPERATOR_CANCELLED;
}

void OUTLINER_OT_group_link(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Link Object to Group";
	ot->description = "Link Object to Group in Outliner";
	ot->idname = "OUTLINER_OT_group_link";

	/* api callbacks */
	ot->invoke = group_link_invoke;

	ot->poll = ED_operator_outliner_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_string(ot->srna, "object", "Object", MAX_ID_NAME, "Object", "Target Object");
}
