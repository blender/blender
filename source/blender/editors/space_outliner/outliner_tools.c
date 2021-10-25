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

/** \file blender/editors/space_outliner/outliner_tools.c
 *  \ingroup spoutliner
 */

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_linestyle_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"
#include "DNA_constraint_types.h"
#include "DNA_modifier_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_group.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_library_remap.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_sequencer.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sequencer.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_view2d.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "outliner_intern.h"


/* ****************************************************** */

/* ************ SELECTION OPERATIONS ********* */

static void set_operation_types(SpaceOops *soops, ListBase *lb,
                                int *scenelevel,
                                int *objectlevel,
                                int *idlevel,
                                int *datalevel)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->flag & TSE_SELECTED) {
			if (tselem->type) {
				if (*datalevel == 0)
					*datalevel = tselem->type;
				else if (*datalevel != tselem->type)
					*datalevel = -1;
			}
			else {
				int idcode = GS(tselem->id->name);
				switch (idcode) {
					case ID_SCE:
						*scenelevel = 1;
						break;
					case ID_OB:
						*objectlevel = 1;
						break;
						
					case ID_ME: case ID_CU: case ID_MB: case ID_LT:
					case ID_LA: case ID_AR: case ID_CA: case ID_SPK:
					case ID_MA: case ID_TE: case ID_IP: case ID_IM:
					case ID_SO: case ID_KE: case ID_WO: case ID_AC:
					case ID_NLA: case ID_TXT: case ID_GR: case ID_LS:
					case ID_LI:
						if (*idlevel == 0) *idlevel = idcode;
						else if (*idlevel != idcode) *idlevel = -1;
						break;
				}
			}
		}
		if (TSELEM_OPEN(tselem, soops)) {
			set_operation_types(soops, &te->subtree,
			                    scenelevel, objectlevel, idlevel, datalevel);
		}
	}
}

static void unlink_action_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *tsep, TreeStoreElem *UNUSED(tselem), void *UNUSED(user_data))
{
	/* just set action to NULL */
	BKE_animdata_set_action(CTX_wm_reports(C), tsep->id, NULL);
}

static void unlink_material_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *te,
        TreeStoreElem *tsep, TreeStoreElem *UNUSED(tselem), void *UNUSED(user_data))
{
	Material **matar = NULL;
	int a, totcol = 0;
	
	if (GS(tsep->id->name) == ID_OB) {
		Object *ob = (Object *)tsep->id;
		totcol = ob->totcol;
		matar = ob->mat;
	}
	else if (GS(tsep->id->name) == ID_ME) {
		Mesh *me = (Mesh *)tsep->id;
		totcol = me->totcol;
		matar = me->mat;
	}
	else if (GS(tsep->id->name) == ID_CU) {
		Curve *cu = (Curve *)tsep->id;
		totcol = cu->totcol;
		matar = cu->mat;
	}
	else if (GS(tsep->id->name) == ID_MB) {
		MetaBall *mb = (MetaBall *)tsep->id;
		totcol = mb->totcol;
		matar = mb->mat;
	}
	else {
		BLI_assert(0);
	}

	if (LIKELY(matar != NULL)) {
		for (a = 0; a < totcol; a++) {
			if (a == te->index && matar[a]) {
				id_us_min(&matar[a]->id);
				matar[a] = NULL;
			}
		}
	}
}

static void unlink_texture_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *te,
        TreeStoreElem *tsep, TreeStoreElem *UNUSED(tselem), void *UNUSED(user_data))
{
	MTex **mtex = NULL;
	int a;
	
	if (GS(tsep->id->name) == ID_MA) {
		Material *ma = (Material *)tsep->id;
		mtex = ma->mtex;
	}
	else if (GS(tsep->id->name) == ID_LA) {
		Lamp *la = (Lamp *)tsep->id;
		mtex = la->mtex;
	}
	else if (GS(tsep->id->name) == ID_WO) {
		World *wrld = (World *)tsep->id;
		mtex = wrld->mtex;
	}
	else if (GS(tsep->id->name) == ID_LS) {
		FreestyleLineStyle *ls = (FreestyleLineStyle *)tsep->id;
		mtex = ls->mtex;
	}
	else {
		return;
	}

	for (a = 0; a < MAX_MTEX; a++) {
		if (a == te->index && mtex[a]) {
			if (mtex[a]->tex) {
				id_us_min(&mtex[a]->tex->id);
				mtex[a]->tex = NULL;
			}
		}
	}
}

static void unlink_group_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *tsep, TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Group *group = (Group *)tselem->id;
	
	if (tsep) {
		if (GS(tsep->id->name) == ID_OB) {
			Object *ob = (Object *)tsep->id;
			ob->dup_group = NULL;
		}
	}
	else {
		Main *bmain = CTX_data_main(C);
		BKE_libblock_delete(bmain, group);
	}
}

static void unlink_world_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *tsep, TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Scene *parscene = (Scene *)tsep->id;
	World *wo = (World *)tselem->id;
	
	/* need to use parent scene not just scene, otherwise may end up getting wrong one */
	id_us_min(&wo->id);
	parscene->world = NULL;
}

static void outliner_do_libdata_operation(
        bContext *C, ReportList *reports, Scene *scene, SpaceOops *soops, ListBase *lb,
        outliner_operation_cb operation_cb,
        void *user_data)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->flag & TSE_SELECTED) {
			if (tselem->type == 0) {
				TreeStoreElem *tsep = te->parent ? TREESTORE(te->parent) : NULL;
				operation_cb(C, reports, scene, te, tsep, tselem, user_data);
			}
		}
		if (TSELEM_OPEN(tselem, soops)) {
			outliner_do_libdata_operation(C, reports, scene, soops, &te->subtree, operation_cb, user_data);
		}
	}
}

/* ******************************************** */
typedef enum eOutliner_PropSceneOps {
	OL_SCENE_OP_DELETE = 1
} eOutliner_PropSceneOps;

static EnumPropertyItem prop_scene_op_types[] = {
	{OL_SCENE_OP_DELETE, "DELETE", ICON_X, "Delete", ""},
	{0, NULL, 0, NULL, NULL}
};

static bool outliner_do_scene_operation(
        bContext *C, eOutliner_PropSceneOps event, ListBase *lb,
        bool (*operation_cb)(bContext *, eOutliner_PropSceneOps, TreeElement *, TreeStoreElem *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	bool success = false;

	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->flag & TSE_SELECTED) {
			if (operation_cb(C, event, te, tselem)) {
				success = true;
			}
		}
	}

	return success;
}

static bool scene_cb(bContext *C, eOutliner_PropSceneOps event, TreeElement *UNUSED(te), TreeStoreElem *tselem)
{
	Scene *scene = (Scene *)tselem->id;

	if (event == OL_SCENE_OP_DELETE) {
		if (ED_screen_delete_scene(C, scene)) {
			WM_event_add_notifier(C, NC_SCENE | NA_REMOVED, scene);
		}
		else {
			return false;
		}
	}

	return true;
}

static int outliner_scene_operation_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	const eOutliner_PropSceneOps event = RNA_enum_get(op->ptr, "type");

	if (outliner_do_scene_operation(C, event, &soops->tree, scene_cb) == false) {
		return OPERATOR_CANCELLED;
	}

	if (event == OL_SCENE_OP_DELETE) {
		outliner_cleanup_tree(soops);
		ED_undo_push(C, "Delete Scene(s)");
	}
	else {
		BLI_assert(0);
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_scene_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner Scene Operation";
	ot->idname = "OUTLINER_OT_scene_operation";
	ot->description = "Context menu for scene operations";

	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_scene_operation_exec;
	ot->poll = ED_operator_outliner_active;

	ot->flag = 0;

	ot->prop = RNA_def_enum(ot->srna, "type", prop_scene_op_types, 0, "Scene Operation", "");
}
/* ******************************************** */

static void object_select_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *scene, TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Base *base = (Base *)te->directdata;
	
	if (base == NULL) base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base && ((base->object->restrictflag & OB_RESTRICT_VIEW) == 0)) {
		base->flag |= SELECT;
		base->object->flag |= SELECT;
	}
}

static void object_select_hierarchy_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	/* Don't extend because this toggles, which is nice for Ctrl-Click but not for a menu item.
	 * it's especially confusing when multiple items are selected since some toggle on/off. */
	outliner_item_do_activate_from_tree_element(C, te, tselem, false, true);
}

static void object_deselect_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *scene, TreeElement *te,
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Base *base = (Base *)te->directdata;
	
	if (base == NULL) base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base) {
		base->flag &= ~SELECT;
		base->object->flag &= ~SELECT;
	}
}

static void object_delete_cb(
        bContext *C, ReportList *reports, Scene *scene, TreeElement *te,
        TreeStoreElem *tsep, TreeStoreElem *tselem, void *user_data)
{
	Base *base = (Base *)te->directdata;
	
	if (base == NULL)
		base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base) {
		Main *bmain = CTX_data_main(C);
		if (base->object->id.tag & LIB_TAG_INDIRECT) {
			BKE_reportf(reports, RPT_WARNING, "Cannot delete indirectly linked object '%s'", base->object->id.name + 2);
			return;
		}
		else if (BKE_library_ID_is_indirectly_used(bmain, base->object) &&
		         ID_REAL_USERS(base->object) <= 1 && ID_EXTRA_USERS(base->object) == 0)
		{
			BKE_reportf(reports, RPT_WARNING,
			            "Cannot delete object '%s' from scene '%s', indirectly used objects need at least one user",
			            base->object->id.name + 2, scene->id.name + 2);
			return;
		}

		// check also library later
		if (scene->obedit == base->object)
			ED_object_editmode_exit(C, EM_FREEDATA | EM_FREEUNDO | EM_WAITCURSOR | EM_DO_UNDO);
		
		ED_base_object_free_and_unlink(CTX_data_main(C), scene, base);
		/* leave for ED_outliner_id_unref to handle */
#if 0
		te->directdata = NULL;
		tselem->id = NULL;
#endif
	}
	else {
		/* No base, means object is no more instantiated in any scene.
		 * Should not happen ideally, but does happens, see T51625.
		 * Rather than twisting in all kind of ways to address all possible cases leading to that situation, simpler
		 * to allow deleting such object as a mere generic data-block. */
		id_delete_cb(C, reports, scene, te, tsep, tselem, user_data);
	}
}

static void id_local_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	if (ID_IS_LINKED_DATABLOCK(tselem->id) && (tselem->id->tag & LIB_TAG_EXTERN)) {
		Main *bmain = CTX_data_main(C);
		/* if the ID type has no special local function,
		 * just clear the lib */
		if (id_make_local(bmain, tselem->id, false, false) == false) {
			id_clear_lib_data(bmain, tselem->id);
		}
		else {
			BKE_main_id_clear_newpoins(bmain);
		}
	}
}

static void id_fake_user_set_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	ID *id = tselem->id;
	
	id_fake_user_set(id);
}

static void id_fake_user_clear_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	ID *id = tselem->id;
	
	id_fake_user_clear(id);
}

static void id_select_linked_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	ID *id = tselem->id;

	ED_object_select_linked_by_id(C, id);
}

static void singleuser_action_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *tsep, TreeStoreElem *tselem, void *UNUSED(user_data))
{
	ID *id = tselem->id;
	
	if (id) {
		IdAdtTemplate *iat = (IdAdtTemplate *)tsep->id;
		PointerRNA ptr = {{NULL}};
		PropertyRNA *prop;
		
		RNA_pointer_create(&iat->id, &RNA_AnimData, iat->adt, &ptr);
		prop = RNA_struct_find_property(&ptr, "action");
		
		id_single_user(C, id, &ptr, prop);
	}
}

static void singleuser_world_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *UNUSED(scene), TreeElement *UNUSED(te),
        TreeStoreElem *tsep, TreeStoreElem *tselem, void *UNUSED(user_data))
{
	ID *id = tselem->id;
	
	/* need to use parent scene not just scene, otherwise may end up getting wrong one */
	if (id) {
		Scene *parscene = (Scene *)tsep->id;
		PointerRNA ptr = {{NULL}};
		PropertyRNA *prop;
		
		RNA_id_pointer_create(&parscene->id, &ptr);
		prop = RNA_struct_find_property(&ptr, "world");
		
		id_single_user(C, id, &ptr, prop);
	}
}

static void group_linkobs2scene_cb(
        bContext *UNUSED(C), ReportList *UNUSED(reports), Scene *scene, TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Group *group = (Group *)tselem->id;
	GroupObject *gob;
	Base *base;

	for (gob = group->gobject.first; gob; gob = gob->next) {
		base = BKE_scene_base_find(scene, gob->ob);
		if (!base) {
			/* link to scene */
			base = BKE_scene_base_add(scene, gob->ob);
			id_us_plus(&gob->ob->id);
		}
		base->object->flag |= SELECT;
		base->flag |= SELECT;
	}
}

static void group_instance_cb(
        bContext *C, ReportList *UNUSED(reports), Scene *scene, TreeElement *UNUSED(te),
        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Group *group = (Group *)tselem->id;

	Object *ob = ED_object_add_type(C, OB_EMPTY, group->id.name + 2, scene->cursor, NULL, false, scene->layact);
	ob->dup_group = group;
	ob->transflag |= OB_DUPLIGROUP;
	id_lib_extern(&group->id);
}

/**
 * \param select_recurse: Set to false for operations which are already recursively operating on their children.
 */
void outliner_do_object_operation_ex(
        bContext *C, ReportList *reports, Scene *scene_act, SpaceOops *soops, ListBase *lb,
        outliner_operation_cb operation_cb, bool select_recurse)
{
	TreeElement *te;
	
	for (te = lb->first; te; te = te->next) {
		TreeStoreElem *tselem = TREESTORE(te);
		bool select_handled = false;
		if (tselem->flag & TSE_SELECTED) {
			if (tselem->type == 0 && te->idcode == ID_OB) {
				// when objects selected in other scenes... dunno if that should be allowed
				Scene *scene_owner = (Scene *)outliner_search_back(soops, te, ID_SCE);
				if (scene_owner && scene_act != scene_owner) {
					ED_screen_set_scene(C, CTX_wm_screen(C), scene_owner);
				}
				/* important to use 'scene_owner' not scene_act else deleting objects can crash.
				 * only use 'scene_act' when 'scene_owner' is NULL, which can happen when the
				 * outliner isn't showing scenes: Visible Layer draw mode for eg. */
				operation_cb(C, reports, scene_owner ? scene_owner : scene_act, te, NULL, tselem, NULL);
				select_handled = true;
			}
		}
		if (TSELEM_OPEN(tselem, soops)) {
			if ((select_handled == false) || select_recurse) {
				outliner_do_object_operation_ex(
				            C, reports, scene_act, soops, &te->subtree, operation_cb, select_recurse);
			}
		}
	}
}

void outliner_do_object_operation(
        bContext *C, ReportList *reports, Scene *scene_act, SpaceOops *soops, ListBase *lb,
        outliner_operation_cb operation_cb)
{
	outliner_do_object_operation_ex(C, reports, scene_act, soops, lb, operation_cb, true);
}

/* ******************************************** */

static void clear_animdata_cb(int UNUSED(event), TreeElement *UNUSED(te),
                              TreeStoreElem *tselem, void *UNUSED(arg))
{
	BKE_animdata_free(tselem->id, true);
}


static void unlinkact_animdata_cb(int UNUSED(event), TreeElement *UNUSED(te),
                                  TreeStoreElem *tselem, void *UNUSED(arg))
{
	/* just set action to NULL */
	BKE_animdata_set_action(NULL, tselem->id, NULL);
}

static void cleardrivers_animdata_cb(int UNUSED(event), TreeElement *UNUSED(te),
                                     TreeStoreElem *tselem, void *UNUSED(arg))
{
	IdAdtTemplate *iat = (IdAdtTemplate *)tselem->id;
	
	/* just free drivers - stored as a list of F-Curves */
	free_fcurves(&iat->adt->drivers);
}

static void refreshdrivers_animdata_cb(int UNUSED(event), TreeElement *UNUSED(te),
                                       TreeStoreElem *tselem, void *UNUSED(arg))
{
	IdAdtTemplate *iat = (IdAdtTemplate *)tselem->id;
	FCurve *fcu;
	
	/* loop over drivers, performing refresh (i.e. check graph_buttons.c and rna_fcurve.c for details) */
	for (fcu = iat->adt->drivers.first; fcu; fcu = fcu->next) {
		fcu->flag &= ~FCURVE_DISABLED;
		
		if (fcu->driver)
			fcu->driver->flag &= ~DRIVER_FLAG_INVALID;
	}
}

/* --------------------------------- */

typedef enum eOutliner_PropDataOps {
	OL_DOP_SELECT = 1,
	OL_DOP_DESELECT,
	OL_DOP_HIDE,
	OL_DOP_UNHIDE,
	OL_DOP_SELECT_LINKED,
} eOutliner_PropDataOps;

typedef enum eOutliner_PropConstraintOps {
	OL_CONSTRAINTOP_ENABLE = 1,
	OL_CONSTRAINTOP_DISABLE,
	OL_CONSTRAINTOP_DELETE
} eOutliner_PropConstraintOps;

typedef enum eOutliner_PropModifierOps {
	OL_MODIFIER_OP_TOGVIS = 1,
	OL_MODIFIER_OP_TOGREN,
	OL_MODIFIER_OP_DELETE
} eOutliner_PropModifierOps;

static void pchan_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
	bPoseChannel *pchan = (bPoseChannel *)te->directdata;
	
	if (event == OL_DOP_SELECT)
		pchan->bone->flag |= BONE_SELECTED;
	else if (event == OL_DOP_DESELECT)
		pchan->bone->flag &= ~BONE_SELECTED;
	else if (event == OL_DOP_HIDE) {
		pchan->bone->flag |= BONE_HIDDEN_P;
		pchan->bone->flag &= ~BONE_SELECTED;
	}
	else if (event == OL_DOP_UNHIDE)
		pchan->bone->flag &= ~BONE_HIDDEN_P;
}

static void bone_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
	Bone *bone = (Bone *)te->directdata;
	
	if (event == OL_DOP_SELECT)
		bone->flag |= BONE_SELECTED;
	else if (event == OL_DOP_DESELECT)
		bone->flag &= ~BONE_SELECTED;
	else if (event == OL_DOP_HIDE) {
		bone->flag |= BONE_HIDDEN_P;
		bone->flag &= ~BONE_SELECTED;
	}
	else if (event == OL_DOP_UNHIDE)
		bone->flag &= ~BONE_HIDDEN_P;
}

static void ebone_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
	EditBone *ebone = (EditBone *)te->directdata;
	
	if (event == OL_DOP_SELECT)
		ebone->flag |= BONE_SELECTED;
	else if (event == OL_DOP_DESELECT)
		ebone->flag &= ~BONE_SELECTED;
	else if (event == OL_DOP_HIDE) {
		ebone->flag |= BONE_HIDDEN_A;
		ebone->flag &= ~BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL;
	}
	else if (event == OL_DOP_UNHIDE)
		ebone->flag &= ~BONE_HIDDEN_A;
}

static void sequence_cb(int event, TreeElement *te, TreeStoreElem *tselem, void *scene_ptr)
{
	Sequence *seq = (Sequence *)te->directdata;
	if (event == OL_DOP_SELECT) {
		Scene *scene = (Scene *)scene_ptr;
		Editing *ed = BKE_sequencer_editing_get(scene, false);
		if (BLI_findindex(ed->seqbasep, seq) != -1) {
			ED_sequencer_select_sequence_single(scene, seq, true);
		}
	}

	(void)tselem;
}

static void gp_layer_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
	bGPDlayer *gpl = (bGPDlayer *)te->directdata;
	
	if (event == OL_DOP_SELECT)
		gpl->flag |= GP_LAYER_SELECT;
	else if (event == OL_DOP_DESELECT)
		gpl->flag &= ~GP_LAYER_SELECT;
	else if (event == OL_DOP_HIDE)
		gpl->flag |= GP_LAYER_HIDE;
	else if (event == OL_DOP_UNHIDE)
		gpl->flag &= ~GP_LAYER_HIDE;
}

static void data_select_linked_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *C_v)
{
	if (event == OL_DOP_SELECT_LINKED) {
		if (RNA_struct_is_ID(te->rnaptr.type)) {
			bContext *C = (bContext *) C_v;
			ID *id = te->rnaptr.data;

			ED_object_select_linked_by_id(C, id);
		}
	}
}

static void constraint_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *C_v)
{
	bContext *C = C_v;
	SpaceOops *soops = CTX_wm_space_outliner(C);
	bConstraint *constraint = (bConstraint *)te->directdata;
	Object *ob = (Object *)outliner_search_back(soops, te, ID_OB);

	if (event == OL_CONSTRAINTOP_ENABLE) {
		constraint->flag &= ~CONSTRAINT_OFF;
		ED_object_constraint_update(ob);
		WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
	}
	else if (event == OL_CONSTRAINTOP_DISABLE) {
		constraint->flag = CONSTRAINT_OFF;
		ED_object_constraint_update(ob);
		WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT, ob);
	}
	else if (event == OL_CONSTRAINTOP_DELETE) {
		ListBase *lb = NULL;

		if (TREESTORE(te->parent->parent)->type == TSE_POSE_CHANNEL) {
			lb = &((bPoseChannel *)te->parent->parent->directdata)->constraints;
		}
		else {
			lb = &ob->constraints;
		}

		if (BKE_constraint_remove_ex(lb, ob, constraint, true)) {
			/* there's no active constraint now, so make sure this is the case */
			BKE_constraints_active_set(&ob->constraints, NULL);
			ED_object_constraint_update(ob); /* needed to set the flags on posebones correctly */
			WM_event_add_notifier(C, NC_OBJECT | ND_CONSTRAINT | NA_REMOVED, ob);
			te->store_elem->flag &= ~TSE_SELECTED;
		}
	}
}

static void modifier_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *Carg)
{
	bContext *C = (bContext *)Carg;
	Main *bmain = CTX_data_main(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	ModifierData *md = (ModifierData *)te->directdata;
	Object *ob = (Object *)outliner_search_back(soops, te, ID_OB);

	if (event == OL_MODIFIER_OP_TOGVIS) {
		md->mode ^= eModifierMode_Realtime;
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	}
	else if (event == OL_MODIFIER_OP_TOGREN) {
		md->mode ^= eModifierMode_Render;
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
		WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
	}
	else if (event == OL_MODIFIER_OP_DELETE) {
		ED_object_modifier_remove(NULL, bmain, ob, md);
		WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER | NA_REMOVED, ob);
		te->store_elem->flag &= ~TSE_SELECTED;
	}
}

static void outliner_do_data_operation(SpaceOops *soops, int type, int event, ListBase *lb,
                                       void (*operation_cb)(int, TreeElement *, TreeStoreElem *, void *),
                                       void *arg)
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->flag & TSE_SELECTED) {
			if (tselem->type == type) {
				operation_cb(event, te, tselem, arg);
			}
		}
		if (TSELEM_OPEN(tselem, soops)) {
			outliner_do_data_operation(soops, type, event, &te->subtree, operation_cb, arg);
		}
	}
}

static Base *outline_delete_hierarchy(bContext *C, ReportList *reports, Scene *scene, Base *base)
{
	Base *child_base, *base_next;
	Object *parent;

	if (!base) {
		return NULL;
	}

	for (child_base = scene->base.first; child_base; child_base = base_next) {
		base_next = child_base->next;
		for (parent = child_base->object->parent; parent && (parent != base->object); parent = parent->parent);
		if (parent) {
			base_next = outline_delete_hierarchy(C, reports, scene, child_base);
		}
	}

	base_next = base->next;

	Main *bmain = CTX_data_main(C);
	if (base->object->id.tag & LIB_TAG_INDIRECT) {
		BKE_reportf(reports, RPT_WARNING, "Cannot delete indirectly linked object '%s'", base->object->id.name + 2);
		return base_next;
	}
	else if (BKE_library_ID_is_indirectly_used(bmain, base->object) &&
	         ID_REAL_USERS(base->object) <= 1 && ID_EXTRA_USERS(base->object) == 0)
	{
		BKE_reportf(reports, RPT_WARNING,
		            "Cannot delete object '%s' from scene '%s', indirectly used objects need at least one user",
		            base->object->id.name + 2, scene->id.name + 2);
		return base_next;
	}
	ED_base_object_free_and_unlink(CTX_data_main(C), scene, base);
	return base_next;
}

static void object_delete_hierarchy_cb(
        bContext *C, ReportList *reports, Scene *scene,
        TreeElement *te, TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem, void *UNUSED(user_data))
{
	Base *base = (Base *)te->directdata;
	Object *obedit = scene->obedit;

	if (!base) {
		base = BKE_scene_base_find(scene, (Object *)tselem->id);
	}
	if (base) {
		/* Check also library later. */
		for (; obedit && (obedit != base->object); obedit = obedit->parent);
		if (obedit == base->object) {
			ED_object_editmode_exit(C, EM_FREEDATA | EM_FREEUNDO | EM_WAITCURSOR | EM_DO_UNDO);
		}

		outline_delete_hierarchy(C, reports, scene, base);
		/* leave for ED_outliner_id_unref to handle */
#if 0
		te->directdata = NULL;
		tselem->id = NULL;
#endif
	}

	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
}

/* **************************************** */

enum {
	OL_OP_SELECT = 1,
	OL_OP_DESELECT,
	OL_OP_SELECT_HIERARCHY,
	OL_OP_DELETE,
	OL_OP_DELETE_HIERARCHY,
	OL_OP_REMAP,
	OL_OP_LOCALIZED,  /* disabled, see below */
	OL_OP_TOGVIS,
	OL_OP_TOGSEL,
	OL_OP_TOGREN,
	OL_OP_RENAME,
};

static EnumPropertyItem prop_object_op_types[] = {
	{OL_OP_SELECT, "SELECT", 0, "Select", ""},
	{OL_OP_DESELECT, "DESELECT", 0, "Deselect", ""},
	{OL_OP_SELECT_HIERARCHY, "SELECT_HIERARCHY", 0, "Select Hierarchy", ""},
	{OL_OP_DELETE, "DELETE", 0, "Delete", ""},
	{OL_OP_DELETE_HIERARCHY, "DELETE_HIERARCHY", 0, "Delete Hierarchy", ""},
	{OL_OP_REMAP, "REMAP",   0, "Remap Users",
	 "Make all users of selected data-blocks to use instead a new chosen one"},
	{OL_OP_TOGVIS, "TOGVIS", 0, "Toggle Visible", ""},
	{OL_OP_TOGSEL, "TOGSEL", 0, "Toggle Selectable", ""},
	{OL_OP_TOGREN, "TOGREN", 0, "Toggle Renderable", ""},
	{OL_OP_RENAME, "RENAME", 0, "Rename", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_object_operation_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int event;
	const char *str = NULL;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	event = RNA_enum_get(op->ptr, "type");

	if (event == OL_OP_SELECT) {
		Scene *sce = scene;  // to be able to delete, scenes are set...
		outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_select_cb);
		if (scene != sce) {
			ED_screen_set_scene(C, CTX_wm_screen(C), sce);
		}
		
		str = "Select Objects";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}
	else if (event == OL_OP_SELECT_HIERARCHY) {
		Scene *sce = scene;  // to be able to delete, scenes are set...
		outliner_do_object_operation_ex(C, op->reports, scene, soops, &soops->tree, object_select_hierarchy_cb, false);
		if (scene != sce) {
			ED_screen_set_scene(C, CTX_wm_screen(C), sce);
		}	
		str = "Select Object Hierarchy";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}
	else if (event == OL_OP_DESELECT) {
		outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_deselect_cb);
		str = "Deselect Objects";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}
	else if (event == OL_OP_DELETE) {
		outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_delete_cb);

		/* XXX: tree management normally happens from draw_outliner(), but when
		 *      you're clicking to fast on Delete object from context menu in
		 *      outliner several mouse events can be handled in one cycle without
		 *      handling notifiers/redraw which leads to deleting the same object twice.
		 *      cleanup tree here to prevent such cases. */
		outliner_cleanup_tree(soops);

		DAG_relations_tag_update(bmain);
		str = "Delete Objects";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	}
	else if (event == OL_OP_DELETE_HIERARCHY) {
		outliner_do_object_operation_ex(C, op->reports, scene, soops, &soops->tree, object_delete_hierarchy_cb, false);

		/* XXX: See OL_OP_DELETE comment above. */
		outliner_cleanup_tree(soops);

		DAG_relations_tag_update(bmain);
		str = "Delete Object Hierarchy";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	}
	else if (event == OL_OP_REMAP) {
		outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_remap_cb, NULL);
		str = "Remap ID";
	}
	else if (event == OL_OP_LOCALIZED) {    /* disabled, see above enum (ton) */
		outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, id_local_cb);
		str = "Localized Objects";
	}
	else if (event == OL_OP_TOGVIS) {
		outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_toggle_visibility_cb);
		str = "Toggle Visibility";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_VISIBLE, scene);
	}
	else if (event == OL_OP_TOGSEL) {
		outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_toggle_selectability_cb);
		str = "Toggle Selectability";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}
	else if (event == OL_OP_TOGREN) {
		outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, object_toggle_renderability_cb);
		str = "Toggle Renderability";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_RENDER, scene);
	}
	else if (event == OL_OP_RENAME) {
		outliner_do_object_operation(C, op->reports, scene, soops, &soops->tree, item_rename_cb);
		str = "Rename Object";
	}
	else {
		BLI_assert(0);
		return OPERATOR_CANCELLED;
	}

	ED_undo_push(C, str);
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_object_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner Object Operation";
	ot->idname = "OUTLINER_OT_object_operation";
	ot->description = "";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_object_operation_exec;
	ot->poll = ED_operator_outliner_active;
	
	ot->flag = 0;

	ot->prop = RNA_def_enum(ot->srna, "type", prop_object_op_types, 0, "Object Operation", "");
}

/* **************************************** */

typedef enum eOutliner_PropGroupOps {
	OL_GROUPOP_UNLINK = 1,
	OL_GROUPOP_LOCAL,
	OL_GROUPOP_LINK,
	OL_GROUPOP_DELETE,
	OL_GROUPOP_REMAP,
	OL_GROUPOP_INSTANCE,
	OL_GROUPOP_TOGVIS,
	OL_GROUPOP_TOGSEL,
	OL_GROUPOP_TOGREN,
	OL_GROUPOP_RENAME,
} eOutliner_PropGroupOps;

static EnumPropertyItem prop_group_op_types[] = {
	{OL_GROUPOP_UNLINK, "UNLINK",     0, "Unlink Group", ""},
	{OL_GROUPOP_LOCAL, "LOCAL",       0, "Make Local Group", ""},
	{OL_GROUPOP_LINK, "LINK",         0, "Link Group Objects to Scene", ""},
	{OL_GROUPOP_DELETE, "DELETE",     0, "Delete Group", ""},
	{OL_GROUPOP_REMAP, "REMAP",       0, "Remap Users",
	 "Make all users of selected data-blocks to use instead current (clicked) one"},
	{OL_GROUPOP_INSTANCE, "INSTANCE", 0, "Instance Groups in Scene", ""},
	{OL_GROUPOP_TOGVIS, "TOGVIS",     0, "Toggle Visible Group", ""},
	{OL_GROUPOP_TOGSEL, "TOGSEL",     0, "Toggle Selectable", ""},
	{OL_GROUPOP_TOGREN, "TOGREN",     0, "Toggle Renderable", ""},
	{OL_GROUPOP_RENAME, "RENAME",     0, "Rename", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_group_operation_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int event;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	event = RNA_enum_get(op->ptr, "type");

	switch (event) {
		case OL_GROUPOP_UNLINK:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, unlink_group_cb, NULL);
			break;
		case OL_GROUPOP_LOCAL:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_local_cb, NULL);
			break;
		case OL_GROUPOP_LINK:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, group_linkobs2scene_cb, NULL);
			break;
		case OL_GROUPOP_INSTANCE:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, group_instance_cb, NULL);
			/* works without this except if you try render right after, see: 22027 */
			DAG_relations_tag_update(CTX_data_main(C));
			break;
		case OL_GROUPOP_DELETE:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_delete_cb, NULL);
			break;
		case OL_GROUPOP_REMAP:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_remap_cb, NULL);
			break;
		case OL_GROUPOP_TOGVIS:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, group_toggle_visibility_cb, NULL);
			break;
		case OL_GROUPOP_TOGSEL:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, group_toggle_selectability_cb, NULL);
			break;
		case OL_GROUPOP_TOGREN:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, group_toggle_renderability_cb, NULL);
			break;
		case OL_GROUPOP_RENAME:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, item_rename_cb, NULL);
			break;
		default:
			BLI_assert(0);
	}

	ED_undo_push(C, prop_group_op_types[event - 1].name);
	WM_event_add_notifier(C, NC_GROUP, NULL);

	return OPERATOR_FINISHED;
}


void OUTLINER_OT_group_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner Group Operation";
	ot->idname = "OUTLINER_OT_group_operation";
	ot->description = "";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_group_operation_exec;
	ot->poll = ED_operator_outliner_active;
	
	ot->flag = 0;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_group_op_types, 0, "Group Operation", "");
}

/* **************************************** */

typedef enum eOutlinerIdOpTypes {
	OUTLINER_IDOP_INVALID = 0,
	
	OUTLINER_IDOP_UNLINK,
	OUTLINER_IDOP_LOCAL,
	OUTLINER_IDOP_SINGLE,
	OUTLINER_IDOP_DELETE,
	OUTLINER_IDOP_REMAP,
	
	OUTLINER_IDOP_FAKE_ADD,
	OUTLINER_IDOP_FAKE_CLEAR,
	OUTLINER_IDOP_RENAME,

	OUTLINER_IDOP_SELECT_LINKED
} eOutlinerIdOpTypes;

// TODO: implement support for changing the ID-block used
static EnumPropertyItem prop_id_op_types[] = {
	{OUTLINER_IDOP_UNLINK, "UNLINK", 0, "Unlink", ""},
	{OUTLINER_IDOP_LOCAL, "LOCAL", 0, "Make Local", ""},
	{OUTLINER_IDOP_SINGLE, "SINGLE", 0, "Make Single User", ""},
	{OUTLINER_IDOP_DELETE, "DELETE", 0, "Delete", "WARNING: no undo"},
	{OUTLINER_IDOP_REMAP, "REMAP", 0, "Remap Users",
	 "Make all users of selected data-blocks to use instead current (clicked) one"},
	{OUTLINER_IDOP_FAKE_ADD, "ADD_FAKE", 0, "Add Fake User",
	 "Ensure data-block gets saved even if it isn't in use (e.g. for motion and material libraries)"},
	{OUTLINER_IDOP_FAKE_CLEAR, "CLEAR_FAKE", 0, "Clear Fake User", ""},
	{OUTLINER_IDOP_RENAME, "RENAME", 0, "Rename", ""},
	{OUTLINER_IDOP_SELECT_LINKED, "SELECT_LINKED", 0, "Select Linked", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_id_operation_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
	eOutlinerIdOpTypes event;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
	
	event = RNA_enum_get(op->ptr, "type");
	
	switch (event) {
		case OUTLINER_IDOP_UNLINK:
		{
			/* unlink datablock from its parent */
			switch (idlevel) {
				case ID_AC:
					outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, unlink_action_cb, NULL);
					
					WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
					ED_undo_push(C, "Unlink action");
					break;
				case ID_MA:
					outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, unlink_material_cb, NULL);
					
					WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, NULL);
					ED_undo_push(C, "Unlink material");
					break;
				case ID_TE:
					outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, unlink_texture_cb, NULL);
					
					WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, NULL);
					ED_undo_push(C, "Unlink texture");
					break;
				case ID_WO:
					outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, unlink_world_cb, NULL);
					
					WM_event_add_notifier(C, NC_SCENE | ND_WORLD, NULL);
					ED_undo_push(C, "Unlink world");
					break;
				default:
					BKE_report(op->reports, RPT_WARNING, "Not yet implemented");
					break;
			}
			break;
		}
		case OUTLINER_IDOP_LOCAL:
		{
			/* make local */
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_local_cb, NULL);
			ED_undo_push(C, "Localized Data");
			break;
		}
		case OUTLINER_IDOP_SINGLE:
		{
			/* make single user */
			switch (idlevel) {
				case ID_AC:
					outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, singleuser_action_cb, NULL);
					
					WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
					ED_undo_push(C, "Single-User Action");
					break;
					
				case ID_WO:
					outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, singleuser_world_cb, NULL);
					
					WM_event_add_notifier(C, NC_SCENE | ND_WORLD, NULL);
					ED_undo_push(C, "Single-User World");
					break;
					
				default:
					BKE_report(op->reports, RPT_WARNING, "Not yet implemented");
					break;
			}
			break;
		}
		case OUTLINER_IDOP_DELETE:
		{
			if (idlevel > 0) {
				outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_delete_cb, NULL);
				ED_undo_push(C, "Delete");
			}
			break;
		}
		case OUTLINER_IDOP_REMAP:
		{
			if (idlevel > 0) {
				outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_remap_cb, NULL);
				ED_undo_push(C, "Remap");
			}
			break;
		}
		case OUTLINER_IDOP_FAKE_ADD:
		{
			/* set fake user */
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_fake_user_set_cb, NULL);
			
			WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
			ED_undo_push(C, "Add Fake User");
			break;
		}
		case OUTLINER_IDOP_FAKE_CLEAR:
		{
			/* clear fake user */
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_fake_user_clear_cb, NULL);
			
			WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
			ED_undo_push(C, "Clear Fake User");
			break;
		}
		case OUTLINER_IDOP_RENAME:
		{
			/* rename */
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, item_rename_cb, NULL);
			
			WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
			ED_undo_push(C, "Rename");
			break;
		}
		case OUTLINER_IDOP_SELECT_LINKED:
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_select_linked_cb, NULL);
			ED_undo_push(C, "Select");
			break;
			
		default:
			// invalid - unhandled
			break;
	}
	
	/* wrong notifier still... */
	WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
	
	// XXX: this is just so that outliner is always up to date 
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, NULL);
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_id_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner ID data Operation";
	ot->idname = "OUTLINER_OT_id_operation";
	ot->description = "";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_id_operation_exec;
	ot->poll = ED_operator_outliner_active;
	
	ot->flag = 0;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_id_op_types, 0, "ID data Operation", "");
}

/* **************************************** */

typedef enum eOutlinerLibOpTypes {
	OL_LIB_INVALID = 0,

	OL_LIB_RENAME,
	OL_LIB_DELETE,
	OL_LIB_RELOCATE,
	OL_LIB_RELOAD,
} eOutlinerLibOpTypes;

static EnumPropertyItem outliner_lib_op_type_items[] = {
	{OL_LIB_RENAME, "RENAME", 0, "Rename", ""},
	{OL_LIB_DELETE, "DELETE", 0, "Delete", "Delete this library and all its item from Blender - WARNING: no undo"},
	{OL_LIB_RELOCATE, "RELOCATE", 0, "Relocate", "Select a new path for this library, and reload all its data"},
	{OL_LIB_RELOAD, "RELOAD", 0, "Reload", "Reload all data from this library"},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_lib_operation_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
	eOutlinerLibOpTypes event;

	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;

	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

	event = RNA_enum_get(op->ptr, "type");

	switch (event) {
		case OL_LIB_RENAME:
		{
			/* rename */
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, item_rename_cb, NULL);

			WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
			ED_undo_push(C, "Rename Library");
			break;
		}
		case OL_LIB_DELETE:
		{
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, id_delete_cb, NULL);
			ED_undo_push(C, "Delete Library");
			break;
		}
		case OL_LIB_RELOCATE:
		{
			/* rename */
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, lib_relocate_cb, NULL);
			ED_undo_push(C, "Relocate Library");
			break;
		}
		case OL_LIB_RELOAD:
		{
			/* rename */
			outliner_do_libdata_operation(C, op->reports, scene, soops, &soops->tree, lib_reload_cb, NULL);
			break;
		}
		default:
			/* invalid - unhandled */
			break;
	}

	/* wrong notifier still... */
	WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);

	/* XXX: this is just so that outliner is always up to date */
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, NULL);

	return OPERATOR_FINISHED;
}


void OUTLINER_OT_lib_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner Library Operation";
	ot->idname = "OUTLINER_OT_lib_operation";
	ot->description = "";

	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_lib_operation_exec;
	ot->poll = ED_operator_outliner_active;

	ot->prop = RNA_def_enum(ot->srna, "type", outliner_lib_op_type_items, 0, "Library Operation", "");
}

/* **************************************** */

static void outliner_do_id_set_operation(SpaceOops *soops, int type, ListBase *lb, ID *newid,
                                         void (*operation_cb)(TreeElement *, TreeStoreElem *, TreeStoreElem *, ID *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->flag & TSE_SELECTED) {
			if (tselem->type == type) {
				TreeStoreElem *tsep = te->parent ? TREESTORE(te->parent) : NULL;
				operation_cb(te, tselem, tsep, newid);
			}
		}
		if (TSELEM_OPEN(tselem, soops)) {
			outliner_do_id_set_operation(soops, type, &te->subtree, newid, operation_cb);
		}
	}
}

/* ------------------------------------------ */

static void actionset_id_cb(TreeElement *UNUSED(te), TreeStoreElem *tselem, TreeStoreElem *tsep, ID *actId)
{
	bAction *act = (bAction *)actId;
	
	if (tselem->type == TSE_ANIM_DATA) {
		/* "animation" entries - action is child of this */
		BKE_animdata_set_action(NULL, tselem->id, act);
	}
	/* TODO: if any other "expander" channels which own actions need to support this menu, 
	 * add: tselem->type = ...
	 */
	else if (tsep && (tsep->type == TSE_ANIM_DATA)) {
		/* "animation" entries case again */
		BKE_animdata_set_action(NULL, tsep->id, act);
	}
	// TODO: other cases not supported yet
}

static int outliner_action_set_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
	
	bAction *act;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
	
	/* get action to use */
	act = BLI_findlink(&CTX_data_main(C)->action, RNA_enum_get(op->ptr, "action"));
	
	if (act == NULL) {
		BKE_report(op->reports, RPT_ERROR, "No valid action to add");
		return OPERATOR_CANCELLED;
	}
	else if (act->idroot == 0) {
		/* hopefully in this case (i.e. library of userless actions), the user knows what they're doing... */
		BKE_reportf(op->reports, RPT_WARNING,
		            "Action '%s' does not specify what data-blocks it can be used on "
		            "(try setting the 'ID Root Type' setting from the data-blocks editor "
		            "for this action to avoid future problems)",
		            act->id.name + 2);
	}
	
	/* perform action if valid channel */
	if (datalevel == TSE_ANIM_DATA)
		outliner_do_id_set_operation(soops, datalevel, &soops->tree, (ID *)act, actionset_id_cb);
	else if (idlevel == ID_AC)
		outliner_do_id_set_operation(soops, idlevel, &soops->tree, (ID *)act, actionset_id_cb);
	else
		return OPERATOR_CANCELLED;
		
	/* set notifier that things have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
	ED_undo_push(C, "Set action");
	
	/* done */
	return OPERATOR_FINISHED;
}

void OUTLINER_OT_action_set(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Outliner Set Action";
	ot->idname = "OUTLINER_OT_action_set";
	ot->description = "Change the active action used";
	
	/* api callbacks */
	ot->invoke = WM_enum_search_invoke;
	ot->exec = outliner_action_set_exec;
	ot->poll = ED_operator_outliner_active;
	
	/* flags */
	ot->flag = 0;
	
	/* props */
	// TODO: this would be nicer as an ID-pointer...
	prop = RNA_def_enum(ot->srna, "action", DummyRNA_NULL_items, 0, "Action", "");
	RNA_def_enum_funcs(prop, RNA_action_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

/* **************************************** */

typedef enum eOutliner_AnimDataOps {
	OUTLINER_ANIMOP_INVALID = 0,
	
	OUTLINER_ANIMOP_CLEAR_ADT,
	
	OUTLINER_ANIMOP_SET_ACT,
	OUTLINER_ANIMOP_CLEAR_ACT,
	
	OUTLINER_ANIMOP_REFRESH_DRV,
	OUTLINER_ANIMOP_CLEAR_DRV
	
	//OUTLINER_ANIMOP_COPY_DRIVERS,
	//OUTLINER_ANIMOP_PASTE_DRIVERS
} eOutliner_AnimDataOps;

static EnumPropertyItem prop_animdata_op_types[] = {
	{OUTLINER_ANIMOP_CLEAR_ADT, "CLEAR_ANIMDATA", 0, "Clear Animation Data", "Remove this animation data container"},
	{OUTLINER_ANIMOP_SET_ACT, "SET_ACT", 0, "Set Action", ""},
	{OUTLINER_ANIMOP_CLEAR_ACT, "CLEAR_ACT", 0, "Unlink Action", ""},
	{OUTLINER_ANIMOP_REFRESH_DRV, "REFRESH_DRIVERS", 0, "Refresh Drivers", ""},
	//{OUTLINER_ANIMOP_COPY_DRIVERS, "COPY_DRIVERS", 0, "Copy Drivers", ""},
	//{OUTLINER_ANIMOP_PASTE_DRIVERS, "PASTE_DRIVERS", 0, "Paste Drivers", ""},
	{OUTLINER_ANIMOP_CLEAR_DRV, "CLEAR_DRIVERS", 0, "Clear Drivers", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_animdata_operation_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
	eOutliner_AnimDataOps event;
	short updateDeps = 0;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	event = RNA_enum_get(op->ptr, "type");
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
	
	if (datalevel != TSE_ANIM_DATA)
		return OPERATOR_CANCELLED;
	
	/* perform the core operation */
	switch (event) {
		case OUTLINER_ANIMOP_CLEAR_ADT:
			/* Remove Animation Data - this may remove the active action, in some cases... */
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, clear_animdata_cb, NULL);
			
			WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
			ED_undo_push(C, "Clear Animation Data");
			break;
		
		case OUTLINER_ANIMOP_SET_ACT:
			/* delegate once again... */
			WM_operator_name_call(C, "OUTLINER_OT_action_set", WM_OP_INVOKE_REGION_WIN, NULL);
			break;
		
		case OUTLINER_ANIMOP_CLEAR_ACT:
			/* clear active action - using standard rules */
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, unlinkact_animdata_cb, NULL);
			
			WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
			ED_undo_push(C, "Unlink action");
			break;
			
		case OUTLINER_ANIMOP_REFRESH_DRV:
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, refreshdrivers_animdata_cb, NULL);
			
			WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, NULL);
			//ED_undo_push(C, "Refresh Drivers"); /* no undo needed - shouldn't have any impact? */
			updateDeps = 1;
			break;
			
		case OUTLINER_ANIMOP_CLEAR_DRV:
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, cleardrivers_animdata_cb, NULL);
			
			WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, NULL);
			ED_undo_push(C, "Clear Drivers");
			updateDeps = 1;
			break;
			
		default: // invalid
			break;
	}
	
	/* update dependencies */
	if (updateDeps) {
		/* rebuild depsgraph for the new deps */
		DAG_relations_tag_update(CTX_data_main(C));
	}
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_animdata_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner Animation Data Operation";
	ot->idname = "OUTLINER_OT_animdata_operation";
	ot->description = "";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_animdata_operation_exec;
	ot->poll = ED_operator_outliner_active;
	
	ot->flag = 0;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_animdata_op_types, 0, "Animation Operation", "");
}

/* **************************************** */

static EnumPropertyItem prop_constraint_op_types[] = {
	{OL_CONSTRAINTOP_ENABLE, "ENABLE", ICON_RESTRICT_VIEW_OFF, "Enable", ""},
	{OL_CONSTRAINTOP_DISABLE, "DISABLE", ICON_RESTRICT_VIEW_ON, "Disable", ""},
	{OL_CONSTRAINTOP_DELETE, "DELETE", ICON_X, "Delete", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_constraint_operation_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
	eOutliner_PropConstraintOps event;

	event = RNA_enum_get(op->ptr, "type");
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

	outliner_do_data_operation(soops, datalevel, event, &soops->tree, constraint_cb, C);

	if (event == OL_CONSTRAINTOP_DELETE) {
		outliner_cleanup_tree(soops);
	}

	ED_undo_push(C, "Constraint operation");

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_constraint_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner Constraint Operation";
	ot->idname = "OUTLINER_OT_constraint_operation";
	ot->description = "";

	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_constraint_operation_exec;
	ot->poll = ED_operator_outliner_active;

	ot->flag = 0;

	ot->prop = RNA_def_enum(ot->srna, "type", prop_constraint_op_types, 0, "Constraint Operation", "");
}

/* ******************** */

static EnumPropertyItem prop_modifier_op_types[] = {
	{OL_MODIFIER_OP_TOGVIS, "TOGVIS", ICON_RESTRICT_VIEW_OFF, "Toggle viewport use", ""},
	{OL_MODIFIER_OP_TOGREN, "TOGREN", ICON_RESTRICT_RENDER_OFF, "Toggle render use", ""},
	{OL_MODIFIER_OP_DELETE, "DELETE", ICON_X, "Delete", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_modifier_operation_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
	eOutliner_PropModifierOps event;

	event = RNA_enum_get(op->ptr, "type");
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);

	outliner_do_data_operation(soops, datalevel, event, &soops->tree, modifier_cb, C);

	if (event == OL_MODIFIER_OP_DELETE) {
		outliner_cleanup_tree(soops);
	}

	ED_undo_push(C, "Modifier operation");

	return OPERATOR_FINISHED;
}

void OUTLINER_OT_modifier_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner Modifier Operation";
	ot->idname = "OUTLINER_OT_modifier_operation";
	ot->description = "";

	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_modifier_operation_exec;
	ot->poll = ED_operator_outliner_active;

	ot->flag = 0;

	ot->prop = RNA_def_enum(ot->srna, "type", prop_modifier_op_types, 0, "Modifier Operation", "");
}

/* ******************** */

// XXX: select linked is for RNA structs only
static EnumPropertyItem prop_data_op_types[] = {
	{OL_DOP_SELECT, "SELECT", 0, "Select", ""},
	{OL_DOP_DESELECT, "DESELECT", 0, "Deselect", ""},
	{OL_DOP_HIDE, "HIDE", 0, "Hide", ""},
	{OL_DOP_UNHIDE, "UNHIDE", 0, "Unhide", ""},
	{OL_DOP_SELECT_LINKED, "SELECT_LINKED", 0, "Select Linked", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_data_operation_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
	eOutliner_PropDataOps event;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	event = RNA_enum_get(op->ptr, "type");
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
	
	switch (datalevel) {
		case TSE_POSE_CHANNEL:
		{
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, pchan_cb, NULL);
			WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
			ED_undo_push(C, "PoseChannel operation");

			break;
		}
		case TSE_BONE:
		{
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, bone_cb, NULL);
			WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
			ED_undo_push(C, "Bone operation");

			break;
		}
		case TSE_EBONE:
		{
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, ebone_cb, NULL);
			WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
			ED_undo_push(C, "EditBone operation");

			break;
		}
		case TSE_SEQUENCE:
		{
			Scene *scene = CTX_data_scene(C);
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, sequence_cb, scene);

			break;
		}
		case TSE_GP_LAYER:
		{
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, gp_layer_cb, NULL);
			WM_event_add_notifier(C, NC_GPENCIL | ND_DATA, NULL);
			ED_undo_push(C, "Grease Pencil Layer operation");

			break;
		}
		case TSE_RNA_STRUCT:
			if (event == OL_DOP_SELECT_LINKED) {
				outliner_do_data_operation(soops, datalevel, event, &soops->tree, data_select_linked_cb, C);
			}

			break;

		default:
			BKE_report(op->reports, RPT_WARNING, "Not yet implemented");
			break;
	}
	
	return OPERATOR_FINISHED;
}


void OUTLINER_OT_data_operation(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Outliner Data Operation";
	ot->idname = "OUTLINER_OT_data_operation";
	ot->description = "";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = outliner_data_operation_exec;
	ot->poll = ED_operator_outliner_active;
	
	ot->flag = 0;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_data_op_types, 0, "Data Operation", "");
}


/* ******************** */


static int do_outliner_operation_event(bContext *C, ARegion *ar, SpaceOops *soops,
                                       TreeElement *te, const float mval[2])
{
	ReportList *reports = CTX_wm_reports(C); // XXX...
	
	if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
		int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
		TreeStoreElem *tselem = TREESTORE(te);
		
		/* select object that's clicked on and popup context menu */
		if (!(tselem->flag & TSE_SELECTED)) {
			
			if (outliner_has_one_flag(&soops->tree, TSE_SELECTED, 1))
				outliner_set_flag(&soops->tree, TSE_SELECTED, 0);
			
			tselem->flag |= TSE_SELECTED;
			/* redraw, same as outliner_select function */
			soops->storeflag |= SO_TREESTORE_REDRAW;
			ED_region_tag_redraw(ar);
		}
		
		set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
		
		if (scenelevel) {
			if (objectlevel || datalevel || idlevel) {
				BKE_report(reports, RPT_WARNING, "Mixed selection");
			}
			else {
				WM_operator_name_call(C, "OUTLINER_OT_scene_operation", WM_OP_INVOKE_REGION_WIN, NULL);
			}
		}
		else if (objectlevel) {
			WM_operator_name_call(C, "OUTLINER_OT_object_operation", WM_OP_INVOKE_REGION_WIN, NULL);
		}
		else if (idlevel) {
			if (idlevel == -1 || datalevel) {
				BKE_report(reports, RPT_WARNING, "Mixed selection");
			}
			else {
				switch (idlevel) {
					case ID_GR:
						WM_operator_name_call(C, "OUTLINER_OT_group_operation", WM_OP_INVOKE_REGION_WIN, NULL);
						break;
					case ID_LI:
						WM_operator_name_call(C, "OUTLINER_OT_lib_operation", WM_OP_INVOKE_REGION_WIN, NULL);
						break;
					default:
						WM_operator_name_call(C, "OUTLINER_OT_id_operation", WM_OP_INVOKE_REGION_WIN, NULL);
						break;
				}
			}
		}
		else if (datalevel) {
			if (datalevel == -1) {
				BKE_report(reports, RPT_WARNING, "Mixed selection");
			}
			else {
				if (datalevel == TSE_ANIM_DATA)
					WM_operator_name_call(C, "OUTLINER_OT_animdata_operation", WM_OP_INVOKE_REGION_WIN, NULL);
				else if (datalevel == TSE_DRIVER_BASE) {
					/* do nothing... no special ops needed yet */
				}
				else if (ELEM(datalevel, TSE_R_LAYER_BASE, TSE_R_LAYER, TSE_R_PASS)) {
					/*WM_operator_name_call(C, "OUTLINER_OT_renderdata_operation", WM_OP_INVOKE_REGION_WIN, NULL)*/
				}
				else if (datalevel == TSE_ID_BASE) {
					/* do nothing... there are no ops needed here yet */
				}
				else if (datalevel == TSE_CONSTRAINT) {
					WM_operator_name_call(C, "OUTLINER_OT_constraint_operation", WM_OP_INVOKE_REGION_WIN, NULL);
				}
				else if (datalevel == TSE_MODIFIER) {
					WM_operator_name_call(C, "OUTLINER_OT_modifier_operation", WM_OP_INVOKE_REGION_WIN, NULL);
				}
				else {
					WM_operator_name_call(C, "OUTLINER_OT_data_operation", WM_OP_INVOKE_REGION_WIN, NULL);
				}
			}
		}
		
		return 1;
	}
	
	for (te = te->subtree.first; te; te = te->next) {
		if (do_outliner_operation_event(C, ar, soops, te, mval))
			return 1;
	}
	return 0;
}


static int outliner_operation(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	uiBut *but = UI_context_active_but_get(C);
	TreeElement *te;
	float fmval[2];

	if (but) {
		UI_but_tooltip_timer_remove(C, but);
	}

	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], &fmval[0], &fmval[1]);
	
	for (te = soops->tree.first; te; te = te->next) {
		if (do_outliner_operation_event(C, ar, soops, te, fmval)) {
			break;
		}
	}
	
	return OPERATOR_FINISHED;
}

/* Menu only! Calls other operators */
void OUTLINER_OT_operation(wmOperatorType *ot)
{
	ot->name = "Execute Operation";
	ot->idname = "OUTLINER_OT_operation";
	ot->description = "Context menu for item operations";
	
	ot->invoke = outliner_operation;
	
	ot->poll = ED_operator_outliner_active;
}

/* ****************************************************** */
