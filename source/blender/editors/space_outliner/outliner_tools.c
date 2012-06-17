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
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_fcurve.h"
#include "BKE_group.h"
#include "BKE_library.h"
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
					case ID_NLA: case ID_TXT: case ID_GR:
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

static void unlink_action_cb(bContext *C, Scene *UNUSED(scene), TreeElement *UNUSED(te),
                             TreeStoreElem *tsep, TreeStoreElem *UNUSED(tselem))
{
	/* just set action to NULL */
	BKE_animdata_set_action(CTX_wm_reports(C), tsep->id, NULL);
}

static void unlink_material_cb(bContext *UNUSED(C), Scene *UNUSED(scene), TreeElement *te,
                               TreeStoreElem *tsep, TreeStoreElem *UNUSED(tselem))
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

	for (a = 0; a < totcol; a++) {
		if (a == te->index && matar[a]) {
			matar[a]->id.us--;
			matar[a] = NULL;
		}
	}
}

static void unlink_texture_cb(bContext *UNUSED(C), Scene *UNUSED(scene), TreeElement *te,
                              TreeStoreElem *tsep, TreeStoreElem *UNUSED(tselem))
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
	else return;
	
	for (a = 0; a < MAX_MTEX; a++) {
		if (a == te->index && mtex[a]) {
			if (mtex[a]->tex) {
				mtex[a]->tex->id.us--;
				mtex[a]->tex = NULL;
			}
		}
	}
}

static void unlink_group_cb(bContext *UNUSED(C), Scene *UNUSED(scene), TreeElement *UNUSED(te),
                            TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Group *group = (Group *)tselem->id;
	
	if (tsep) {
		if (GS(tsep->id->name) == ID_OB) {
			Object *ob = (Object *)tsep->id;
			ob->dup_group = NULL;
		}
	}
	else {
		BKE_group_unlink(group);
	}
}

static void unlink_world_cb(bContext *UNUSED(C), Scene *UNUSED(scene), TreeElement *UNUSED(te),
                            TreeStoreElem *tsep, TreeStoreElem *tselem)
{
	Scene *parscene = (Scene *)tsep->id;
	World *wo = (World *)tselem->id;
	
	/* need to use parent scene not just scene, otherwise may end up getting wrong one */
	id_us_min(&wo->id);
	parscene->world = NULL;
}

static void outliner_do_libdata_operation(bContext *C, Scene *scene, SpaceOops *soops, ListBase *lb, 
                                          void (*operation_cb)(bContext *C, Scene *scene, TreeElement *,
                                                               TreeStoreElem *, TreeStoreElem *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
		if (tselem->flag & TSE_SELECTED) {
			if (tselem->type == 0) {
				TreeStoreElem *tsep = te->parent ? TREESTORE(te->parent) : NULL;
				operation_cb(C, scene, te, tsep, tselem);
			}
		}
		if (TSELEM_OPEN(tselem, soops)) {
			outliner_do_libdata_operation(C, scene, soops, &te->subtree, operation_cb);
		}
	}
}

/* */

static void object_select_cb(bContext *UNUSED(C), Scene *scene, TreeElement *te,
                             TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Base *base = (Base *)te->directdata;
	
	if (base == NULL) base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base && ((base->object->restrictflag & OB_RESTRICT_VIEW) == 0)) {
		base->flag |= SELECT;
		base->object->flag |= SELECT;
	}
}

static void object_deselect_cb(bContext *UNUSED(C), Scene *scene, TreeElement *te,
                               TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Base *base = (Base *)te->directdata;
	
	if (base == NULL) base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base) {
		base->flag &= ~SELECT;
		base->object->flag &= ~SELECT;
	}
}

static void object_delete_cb(bContext *C, Scene *scene, TreeElement *te,
                             TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Base *base = (Base *)te->directdata;
	
	if (base == NULL)
		base = BKE_scene_base_find(scene, (Object *)tselem->id);
	if (base) {
		// check also library later
		if (scene->obedit == base->object)
			ED_object_exit_editmode(C, EM_FREEDATA | EM_FREEUNDO | EM_WAITCURSOR | EM_DO_UNDO);
		
		ED_base_object_free_and_unlink(CTX_data_main(C), scene, base);
		te->directdata = NULL;
		tselem->id = NULL;
	}
}

static void id_local_cb(bContext *C, Scene *UNUSED(scene), TreeElement *UNUSED(te),
                        TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	if (tselem->id->lib && (tselem->id->flag & LIB_EXTERN)) {
		/* if the ID type has no special local function,
		 * just clear the lib */
		if (id_make_local(tselem->id, FALSE) == FALSE) {
			Main *bmain = CTX_data_main(C);
			id_clear_lib_data(bmain, tselem->id);
		}
	}
}

static void id_fake_user_set_cb(bContext *UNUSED(C), Scene *UNUSED(scene), TreeElement *UNUSED(te),
                                TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	ID *id = tselem->id;
	
	if ((id) && ((id->flag & LIB_FAKEUSER) == 0)) {
		id->flag |= LIB_FAKEUSER;
		id_us_plus(id);
	}
}

static void id_fake_user_clear_cb(bContext *UNUSED(C), Scene *UNUSED(scene), TreeElement *UNUSED(te),
                                  TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	ID *id = tselem->id;
	
	if ((id) && (id->flag & LIB_FAKEUSER)) {
		id->flag &= ~LIB_FAKEUSER;
		id_us_min(id);
	}
}

static void singleuser_action_cb(bContext *C, Scene *UNUSED(scene), TreeElement *UNUSED(te),
                                 TreeStoreElem *tsep, TreeStoreElem *tselem)
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

static void singleuser_world_cb(bContext *C, Scene *UNUSED(scene), TreeElement *UNUSED(te),
                                TreeStoreElem *tsep, TreeStoreElem *tselem)
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

static void group_linkobs2scene_cb(bContext *UNUSED(C), Scene *scene, TreeElement *UNUSED(te),
                                   TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Group *group = (Group *)tselem->id;
	GroupObject *gob;
	Base *base;
	
	for (gob = group->gobject.first; gob; gob = gob->next) {
		base = BKE_scene_base_find(scene, gob->ob);
		if (base) {
			base->object->flag |= SELECT;
			base->flag |= SELECT;
		}
		else {
			/* link to scene */
			base = MEM_callocN(sizeof(Base), "add_base");
			BLI_addhead(&scene->base, base);
			base->lay = (1 << 20) - 1; /*v3d->lay;*/ /* would be nice to use the 3d layer but the include's not here */
			gob->ob->flag |= SELECT;
			base->flag = gob->ob->flag;
			base->object = gob->ob;
			id_lib_extern((ID *)gob->ob); /* in case these are from a linked group */
		}
	}
}

static void group_instance_cb(bContext *C, Scene *scene, TreeElement *UNUSED(te),
                              TreeStoreElem *UNUSED(tsep), TreeStoreElem *tselem)
{
	Group *group = (Group *)tselem->id;

	Object *ob = ED_object_add_type(C, OB_EMPTY, scene->cursor, NULL, FALSE, scene->layact);
	rename_id(&ob->id, group->id.name + 2);
	ob->dup_group = group;
	ob->transflag |= OB_DUPLIGROUP;
	id_lib_extern(&group->id);
}

void outliner_do_object_operation(bContext *C, Scene *scene_act, SpaceOops *soops, ListBase *lb, 
                                  void (*operation_cb)(bContext *C, Scene *scene, TreeElement *,
                                                       TreeStoreElem *, TreeStoreElem *))
{
	TreeElement *te;
	TreeStoreElem *tselem;
	
	for (te = lb->first; te; te = te->next) {
		tselem = TREESTORE(te);
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
				operation_cb(C, scene_owner ? scene_owner : scene_act, te, NULL, tselem);
			}
		}
		if (TSELEM_OPEN(tselem, soops)) {
			outliner_do_object_operation(C, scene_act, soops, &te->subtree, operation_cb);
		}
	}
}

/* ******************************************** */

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

static void pchan_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
	bPoseChannel *pchan = (bPoseChannel *)te->directdata;
	
	if (event == 1)
		pchan->bone->flag |= BONE_SELECTED;
	else if (event == 2)
		pchan->bone->flag &= ~BONE_SELECTED;
	else if (event == 3) {
		pchan->bone->flag |= BONE_HIDDEN_P;
		pchan->bone->flag &= ~BONE_SELECTED;
	}
	else if (event == 4)
		pchan->bone->flag &= ~BONE_HIDDEN_P;
}

static void bone_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
	Bone *bone = (Bone *)te->directdata;
	
	if (event == 1)
		bone->flag |= BONE_SELECTED;
	else if (event == 2)
		bone->flag &= ~BONE_SELECTED;
	else if (event == 3) {
		bone->flag |= BONE_HIDDEN_P;
		bone->flag &= ~BONE_SELECTED;
	}
	else if (event == 4)
		bone->flag &= ~BONE_HIDDEN_P;
}

static void ebone_cb(int event, TreeElement *te, TreeStoreElem *UNUSED(tselem), void *UNUSED(arg))
{
	EditBone *ebone = (EditBone *)te->directdata;
	
	if (event == 1)
		ebone->flag |= BONE_SELECTED;
	else if (event == 2)
		ebone->flag &= ~BONE_SELECTED;
	else if (event == 3) {
		ebone->flag |= BONE_HIDDEN_A;
		ebone->flag &= ~BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL;
	}
	else if (event == 4)
		ebone->flag &= ~BONE_HIDDEN_A;
}

static void sequence_cb(int event, TreeElement *te, TreeStoreElem *tselem, void *scene_ptr)
{
	Sequence *seq = (Sequence *)te->directdata;
	if (event == 1) {
		Scene *scene = (Scene *)scene_ptr;
		Editing *ed = BKE_sequencer_editing_get(scene, FALSE);
		if (BLI_findindex(ed->seqbasep, seq) != -1) {
			ED_sequencer_select_sequence_single(scene, seq, TRUE);
		}
	}

	(void)tselem;
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

/* **************************************** */

static EnumPropertyItem prop_object_op_types[] = {
	{1, "SELECT", 0, "Select", ""},
	{2, "DESELECT", 0, "Deselect", ""},
	{4, "DELETE", 0, "Delete", ""},
	{6, "TOGVIS", 0, "Toggle Visible", ""},
	{7, "TOGSEL", 0, "Toggle Selectable", ""},
	{8, "TOGREN", 0, "Toggle Renderable", ""},
	{9, "RENAME", 0, "Rename", ""},
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

	if (event == 1) {
		Scene *sce = scene;  // to be able to delete, scenes are set...
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_select_cb);
		if (scene != sce) {
			ED_screen_set_scene(C, CTX_wm_screen(C), sce);
		}
		
		str = "Select Objects";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}
	else if (event == 2) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_deselect_cb);
		str = "Deselect Objects";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}
	else if (event == 4) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_delete_cb);

		/* XXX: tree management normally happens from draw_outliner(), but when
		 *      you're clicking to fast on Delete object from context menu in
		 *      outliner several mouse events can be handled in one cycle without
		 *      handling notifiers/redraw which leads to deleting the same object twice.
		 *      cleanup tree here to prevent such cases. */
		outliner_cleanup_tree(soops);

		DAG_scene_sort(bmain, scene);
		str = "Delete Objects";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	}
	else if (event == 5) {    /* disabled, see above enum (ton) */
		outliner_do_object_operation(C, scene, soops, &soops->tree, id_local_cb);
		str = "Localized Objects";
	}
	else if (event == 6) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_visibility_cb);
		str = "Toggle Visibility";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_VISIBLE, scene);
	}
	else if (event == 7) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_selectability_cb);
		str = "Toggle Selectability";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}
	else if (event == 8) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, object_toggle_renderability_cb);
		str = "Toggle Renderability";
		WM_event_add_notifier(C, NC_SCENE | ND_OB_RENDER, scene);
	}
	else if (event == 9) {
		outliner_do_object_operation(C, scene, soops, &soops->tree, item_rename_cb);
		str = "Rename Object";
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

static EnumPropertyItem prop_group_op_types[] = {
	{0, "UNLINK",   0, "Unlink Group", ""},
	{1, "LOCAL",    0, "Make Local Group", ""},
	{2, "LINK",     0, "Link Group Objects to Scene", ""},
	{3, "INSTANCE", 0, "Instance Groups in Scene", ""},
	{4, "TOGVIS",   0, "Toggle Visible Group", ""},
	{5, "TOGSEL",   0, "Toggle Selectable", ""},
	{6, "TOGREN",   0, "Toggle Renderable", ""},
	{7, "RENAME",   0, "Rename", ""},
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
		case 0: outliner_do_libdata_operation(C, scene, soops, &soops->tree, unlink_group_cb); break;
		case 1: outliner_do_libdata_operation(C, scene, soops, &soops->tree, id_local_cb); break;
		case 2: outliner_do_libdata_operation(C, scene, soops, &soops->tree, group_linkobs2scene_cb); break;
		case 3: outliner_do_libdata_operation(C, scene, soops, &soops->tree, group_instance_cb); break;
		case 4: outliner_do_libdata_operation(C, scene, soops, &soops->tree, group_toggle_visibility_cb); break;
		case 5: outliner_do_libdata_operation(C, scene, soops, &soops->tree, group_toggle_selectability_cb); break;
		case 6: outliner_do_libdata_operation(C, scene, soops, &soops->tree, group_toggle_renderability_cb); break;
		case 7: outliner_do_libdata_operation(C, scene, soops, &soops->tree, item_rename_cb); break;
		default:
			BLI_assert(0);
			return OPERATOR_CANCELLED;
	}
	

	if (event == 3) { /* instance */
		Main *bmain = CTX_data_main(C);

		/* works without this except if you try render right after, see: 22027 */
		DAG_scene_sort(bmain, scene);
	}
	
	ED_undo_push(C, prop_group_op_types[event].name);
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
	
	OUTLINER_IDOP_FAKE_ADD,
	OUTLINER_IDOP_FAKE_CLEAR,
	OUTLINER_IDOP_RENAME
} eOutlinerIdOpTypes;

// TODO: implement support for changing the ID-block used
static EnumPropertyItem prop_id_op_types[] = {
	{OUTLINER_IDOP_UNLINK, "UNLINK", 0, "Unlink", ""},
	{OUTLINER_IDOP_LOCAL, "LOCAL", 0, "Make Local", ""},
	{OUTLINER_IDOP_SINGLE, "SINGLE", 0, "Make Single User", ""},
	{OUTLINER_IDOP_FAKE_ADD, "ADD_FAKE", 0, "Add Fake User",
     "Ensure datablock gets saved even if it isn't in use (e.g. for motion and material libraries)"},
	{OUTLINER_IDOP_FAKE_CLEAR, "CLEAR_FAKE", 0, "Clear Fake User", ""},
	{OUTLINER_IDOP_RENAME, "RENAME", 0, "Rename", ""},
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
					outliner_do_libdata_operation(C, scene, soops, &soops->tree, unlink_action_cb);
					
					WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
					ED_undo_push(C, "Unlink action");
					break;
				case ID_MA:
					outliner_do_libdata_operation(C, scene, soops, &soops->tree, unlink_material_cb);
					
					WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, NULL);
					ED_undo_push(C, "Unlink material");
					break;
				case ID_TE:
					outliner_do_libdata_operation(C, scene, soops, &soops->tree, unlink_texture_cb);
					
					WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, NULL);
					ED_undo_push(C, "Unlink texture");
					break;
				case ID_WO:
					outliner_do_libdata_operation(C, scene, soops, &soops->tree, unlink_world_cb);
					
					WM_event_add_notifier(C, NC_SCENE | ND_WORLD, NULL);
					ED_undo_push(C, "Unlink world");
					break;
				default:
					BKE_report(op->reports, RPT_WARNING, "Not Yet");
					break;
			}
		}
		break;
			
		case OUTLINER_IDOP_LOCAL:
		{
			/* make local */
			outliner_do_libdata_operation(C, scene, soops, &soops->tree, id_local_cb);
			ED_undo_push(C, "Localized Data");
		}
		break;
			
		case OUTLINER_IDOP_SINGLE:
		{
			/* make single user */
			switch (idlevel) {
				case ID_AC:
					outliner_do_libdata_operation(C, scene, soops, &soops->tree, singleuser_action_cb);
					
					WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
					ED_undo_push(C, "Single-User Action");
					break;
					
				case ID_WO:
					outliner_do_libdata_operation(C, scene, soops, &soops->tree, singleuser_world_cb);
					
					WM_event_add_notifier(C, NC_SCENE | ND_WORLD, NULL);
					ED_undo_push(C, "Single-User World");
					break;
					
				default:
					BKE_report(op->reports, RPT_WARNING, "Not Yet");
					break;
			}
		}
		break;
			
		case OUTLINER_IDOP_FAKE_ADD:
		{
			/* set fake user */
			outliner_do_libdata_operation(C, scene, soops, &soops->tree, id_fake_user_set_cb);
			
			WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
			ED_undo_push(C, "Add Fake User");
		}
		break;
			
		case OUTLINER_IDOP_FAKE_CLEAR:
		{
			/* clear fake user */
			outliner_do_libdata_operation(C, scene, soops, &soops->tree, id_fake_user_clear_cb);
			
			WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
			ED_undo_push(C, "Clear Fake User");
		}
		break;
		case OUTLINER_IDOP_RENAME:
		{
			/* rename */
			outliner_do_libdata_operation(C, scene, soops, &soops->tree, item_rename_cb);
			
			WM_event_add_notifier(C, NC_ID | NA_EDITED, NULL);
			ED_undo_push(C, "Rename");
		}
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
		BKE_report(op->reports, RPT_ERROR, "No valid Action to add");
		return OPERATOR_CANCELLED;
	}
	else if (act->idroot == 0) {
		/* hopefully in this case (i.e. library of userless actions), the user knows what they're doing... */
		BKE_reportf(op->reports, RPT_WARNING,
		            "Action '%s' does not specify what datablocks it can be used on. "
		            "Try setting the 'ID Root Type' setting from the Datablocks Editor "
		            "for this Action to avoid future problems",
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
	ot->prop = prop;
}

/* **************************************** */

typedef enum eOutliner_AnimDataOps {
	OUTLINER_ANIMOP_INVALID = 0,
	
	OUTLINER_ANIMOP_SET_ACT,
	OUTLINER_ANIMOP_CLEAR_ACT,
	
	OUTLINER_ANIMOP_REFRESH_DRV,
	OUTLINER_ANIMOP_CLEAR_DRV
	
	//OUTLINER_ANIMOP_COPY_DRIVERS,
	//OUTLINER_ANIMOP_PASTE_DRIVERS
} eOutliner_AnimDataOps;

static EnumPropertyItem prop_animdata_op_types[] = {
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
		Main *bmain = CTX_data_main(C);
		Scene *scene = CTX_data_scene(C);
		
		/* rebuild depsgraph for the new deps */
		DAG_scene_sort(bmain, scene);
		
		/* force an update of depsgraph */
		DAG_ids_flush_update(bmain, 0);
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

static EnumPropertyItem prop_data_op_types[] = {
	{1, "SELECT", 0, "Select", ""},
	{2, "DESELECT", 0, "Deselect", ""},
	{3, "HIDE", 0, "Hide", ""},
	{4, "UNHIDE", 0, "Unhide", ""},
	{0, NULL, 0, NULL, NULL}
};

static int outliner_data_operation_exec(bContext *C, wmOperator *op)
{
	SpaceOops *soops = CTX_wm_space_outliner(C);
	int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
	int event;
	
	/* check for invalid states */
	if (soops == NULL)
		return OPERATOR_CANCELLED;
	
	event = RNA_enum_get(op->ptr, "type");
	set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
	
	if (datalevel == TSE_POSE_CHANNEL) {
		if (event > 0) {
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, pchan_cb, NULL);
			WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
			ED_undo_push(C, "PoseChannel operation");
		}
	}
	else if (datalevel == TSE_BONE) {
		if (event > 0) {
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, bone_cb, NULL);
			WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
			ED_undo_push(C, "Bone operation");
		}
	}
	else if (datalevel == TSE_EBONE) {
		if (event > 0) {
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, ebone_cb, NULL);
			WM_event_add_notifier(C, NC_OBJECT | ND_POSE, NULL);
			ED_undo_push(C, "EditBone operation");
		}
	}
	else if (datalevel == TSE_SEQUENCE) {
		if (event > 0) {
			Scene *scene = CTX_data_scene(C);
			outliner_do_data_operation(soops, datalevel, event, &soops->tree, sequence_cb, scene);
		}
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


static int do_outliner_operation_event(bContext *C, Scene *scene, ARegion *ar, SpaceOops *soops,
                                       TreeElement *te, wmEvent *event, const float mval[2])
{
	ReportList *reports = CTX_wm_reports(C); // XXX...
	
	if (mval[1] > te->ys && mval[1] < te->ys + UI_UNIT_Y) {
		int scenelevel = 0, objectlevel = 0, idlevel = 0, datalevel = 0;
		TreeStoreElem *tselem = TREESTORE(te);
		
		/* select object that's clicked on and popup context menu */
		if (!(tselem->flag & TSE_SELECTED)) {
			
			if (outliner_has_one_flag(soops, &soops->tree, TSE_SELECTED, 1) )
				outliner_set_flag(soops, &soops->tree, TSE_SELECTED, 0);
			
			tselem->flag |= TSE_SELECTED;
			/* redraw, same as outliner_select function */
			soops->storeflag |= SO_TREESTORE_REDRAW;
			ED_region_tag_redraw(ar);
		}
		
		set_operation_types(soops, &soops->tree, &scenelevel, &objectlevel, &idlevel, &datalevel);
		
		if (scenelevel) {
			//if (objectlevel || datalevel || idlevel) error("Mixed selection");
			//else pupmenu("Scene Operations%t|Delete");
		}
		else if (objectlevel) {
			WM_operator_name_call(C, "OUTLINER_OT_object_operation", WM_OP_INVOKE_REGION_WIN, NULL);
		}
		else if (idlevel) {
			if (idlevel == -1 || datalevel) BKE_report(reports, RPT_WARNING, "Mixed selection");
			else {
				if (idlevel == ID_GR)
					WM_operator_name_call(C, "OUTLINER_OT_group_operation", WM_OP_INVOKE_REGION_WIN, NULL);
				else
					WM_operator_name_call(C, "OUTLINER_OT_id_operation", WM_OP_INVOKE_REGION_WIN, NULL);
			}
		}
		else if (datalevel) {
			if (datalevel == -1) BKE_report(reports, RPT_WARNING, "Mixed selection");
			else {
				if (datalevel == TSE_ANIM_DATA)
					WM_operator_name_call(C, "OUTLINER_OT_animdata_operation", WM_OP_INVOKE_REGION_WIN, NULL);
				else if (datalevel == TSE_DRIVER_BASE)
					/* do nothing... no special ops needed yet */;
				else if (ELEM3(datalevel, TSE_R_LAYER_BASE, TSE_R_LAYER, TSE_R_PASS))
					/*WM_operator_name_call(C, "OUTLINER_OT_renderdata_operation", WM_OP_INVOKE_REGION_WIN, NULL)*/;
				else
					WM_operator_name_call(C, "OUTLINER_OT_data_operation", WM_OP_INVOKE_REGION_WIN, NULL);
			}
		}
		
		return 1;
	}
	
	for (te = te->subtree.first; te; te = te->next) {
		if (do_outliner_operation_event(C, scene, ar, soops, te, event, mval))
			return 1;
	}
	return 0;
}


static int outliner_operation(bContext *C, wmOperator *UNUSED(op), wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar = CTX_wm_region(C);
	SpaceOops *soops = CTX_wm_space_outliner(C);
	TreeElement *te;
	float fmval[2];
	
	UI_view2d_region_to_view(&ar->v2d, event->mval[0], event->mval[1], fmval, fmval + 1);
	
	for (te = soops->tree.first; te; te = te->next) {
		if (do_outliner_operation_event(C, scene, ar, soops, te, event, fmval)) break;
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
