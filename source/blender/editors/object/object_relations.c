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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_relations.c
 *  \ingroup edobj
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_constraint_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_lattice_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_world_types.h"
#include "DNA_object_types.h"
#include "DNA_vfont_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_linklist.h"
#include "BLI_string.h"
#include "BLI_kdtree.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_fcurve.h"
#include "BKE_lamp.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_library_query.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_sca.h"
#include "BKE_scene.h"
#include "BKE_speaker.h"
#include "BKE_texture.h"
#include "BKE_editmesh.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "object_intern.h"

/*********************** Make Vertex Parent Operator ************************/

static int vertex_parent_set_poll(bContext *C)
{
	return ED_operator_editmesh(C) || ED_operator_editsurfcurve(C) || ED_operator_editlattice(C);
}

static int vertex_parent_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	BMVert *eve;
	BMIter iter;
	Curve *cu;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	Object *par;
	int a, v1 = 0, v2 = 0, v3 = 0, v4 = 0, nr = 1;

	/* we need 1 to 3 selected vertices */

	if (obedit->type == OB_MESH) {
		Mesh *me = obedit->data;
		BMEditMesh *em;

		EDBM_mesh_load(obedit);
		EDBM_mesh_make(scene->toolsettings, obedit);

		em = me->edit_btmesh;

		EDBM_mesh_normals_update(em);
		BKE_editmesh_tessface_calc(em);

		/* derivedMesh might be needed for solving parenting,
		 * so re-create it here */
		makeDerivedMesh(scene, obedit, em, CD_MASK_BAREMESH | CD_MASK_ORIGINDEX, 0);

		BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
				if (v1 == 0) v1 = nr;
				else if (v2 == 0) v2 = nr;
				else if (v3 == 0) v3 = nr;
				else if (v4 == 0) v4 = nr;
				else break;
			}
			nr++;
		}
	}
	else if (ELEM(obedit->type, OB_SURF, OB_CURVE)) {
		ListBase *editnurb = object_editcurve_get(obedit);

		cu = obedit->data;

		nu = editnurb->first;
		while (nu) {
			if (nu->type == CU_BEZIER) {
				bezt = nu->bezt;
				a = nu->pntsu;
				while (a--) {
					if (BEZSELECTED_HIDDENHANDLES(cu, bezt)) {
						if (v1 == 0) v1 = nr;
						else if (v2 == 0) v2 = nr;
						else if (v3 == 0) v3 = nr;
						else if (v4 == 0) v4 = nr;
						else break;
					}
					nr++;
					bezt++;
				}
			}
			else {
				bp = nu->bp;
				a = nu->pntsu * nu->pntsv;
				while (a--) {
					if (bp->f1 & SELECT) {
						if (v1 == 0) v1 = nr;
						else if (v2 == 0) v2 = nr;
						else if (v3 == 0) v3 = nr;
						else if (v4 == 0) v4 = nr;
						else break;
					}
					nr++;
					bp++;
				}
			}
			nu = nu->next;
		}
	}
	else if (obedit->type == OB_LATTICE) {
		Lattice *lt = obedit->data;

		a = lt->editlatt->latt->pntsu * lt->editlatt->latt->pntsv * lt->editlatt->latt->pntsw;
		bp = lt->editlatt->latt->def;
		while (a--) {
			if (bp->f1 & SELECT) {
				if (v1 == 0) v1 = nr;
				else if (v2 == 0) v2 = nr;
				else if (v3 == 0) v3 = nr;
				else if (v4 == 0) v4 = nr;
				else break;
			}
			nr++;
			bp++;
		}
	}

	if (v4 || !((v1 && v2 == 0 && v3 == 0) || (v1 && v2 && v3))) {
		BKE_report(op->reports, RPT_ERROR, "Select either 1 or 3 vertices to parent to");
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob != obedit) {
			DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
			par = obedit->parent;

			if (BKE_object_parent_loop_check(par, ob)) {
				BKE_report(op->reports, RPT_ERROR, "Loop in parents");
			}
			else {
				Object workob;

				ob->parent = BASACT->object;
				if (v3) {
					ob->partype = PARVERT3;
					ob->par1 = v1 - 1;
					ob->par2 = v2 - 1;
					ob->par3 = v3 - 1;

					/* inverse parent matrix */
					BKE_object_workob_calc_parent(scene, ob, &workob);
					invert_m4_m4(ob->parentinv, workob.obmat);
				}
				else {
					ob->partype = PARVERT1;
					ob->par1 = v1 - 1;

					/* inverse parent matrix */
					BKE_object_workob_calc_parent(scene, ob, &workob);
					invert_m4_m4(ob->parentinv, workob.obmat);
				}
			}
		}
	}
	CTX_DATA_END;

	DAG_relations_tag_update(bmain);

	WM_event_add_notifier(C, NC_OBJECT, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_vertex_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Vertex Parent";
	ot->description = "Parent selected objects to the selected vertices";
	ot->idname = "OBJECT_OT_vertex_parent_set";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->poll = vertex_parent_set_poll;
	ot->exec = vertex_parent_set_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Make Proxy Operator *************************/

/* set the object to proxify */
static int make_proxy_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = ED_object_active_context(C);

	/* sanity checks */
	if (!scene || scene->id.lib || !ob)
		return OPERATOR_CANCELLED;

	/* Get object to work on - use a menu if we need to... */
	if (ob->dup_group && ob->dup_group->id.lib) {
		/* gives menu with list of objects in group */
		/* proxy_group_objects_menu(C, op, ob, ob->dup_group); */
		WM_enum_search_invoke(C, op, event);
		return OPERATOR_CANCELLED;
	}
	else if (ob->id.lib) {
		uiPopupMenu *pup = uiPupMenuBegin(C, IFACE_("OK?"), ICON_QUESTION);
		uiLayout *layout = uiPupMenuLayout(pup);

		/* create operator menu item with relevant properties filled in */
		uiItemFullO_ptr(layout, op->type, op->type->name, ICON_NONE, NULL,
		                WM_OP_EXEC_REGION_WIN, UI_ITEM_O_RETURN_PROPS);

		/* present the menu and be done... */
		uiPupMenuEnd(C, pup);
	}
	else {
		/* error.. cannot continue */
		BKE_report(op->reports, RPT_ERROR, "Can only make proxy for a referenced object or group");
	}

	/* this invoke just calls another instance of this operator... */
	return OPERATOR_CANCELLED;
}

static int make_proxy_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *ob, *gob = ED_object_active_context(C);
	GroupObject *go;
	Scene *scene = CTX_data_scene(C);

	if (gob->dup_group != NULL) {
		go = BLI_findlink(&gob->dup_group->gobject, RNA_enum_get(op->ptr, "object"));
		ob = go->ob;
	}
	else {
		ob = gob;
		gob = NULL;
	}

	if (ob) {
		Object *newob;
		Base *newbase, *oldbase = BASACT;
		char name[MAX_ID_NAME + 4];

		/* Add new object for the proxy */
		newob = BKE_object_add(bmain, scene, OB_EMPTY);

		BLI_snprintf(name, sizeof(name), "%s_proxy", ((ID *)(gob ? gob : ob))->name + 2);

		rename_id(&newob->id, name);

		/* set layers OK */
		newbase = BASACT;    /* BKE_object_add sets active... */
		newbase->lay = oldbase->lay;
		newob->lay = newbase->lay;

		/* remove base, leave user count of object, it gets linked in BKE_object_make_proxy */
		if (gob == NULL) {
			BKE_scene_base_unlink(scene, oldbase);
			MEM_freeN(oldbase);
		}

		BKE_object_make_proxy(newob, ob, gob);

		/* depsgraph flushes are needed for the new data */
		DAG_relations_tag_update(bmain);
		DAG_id_tag_update(&newob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
		WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, newob);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "No object to make proxy for");
		return OPERATOR_CANCELLED;
	}

	return OPERATOR_FINISHED;
}

/* Generic itemf's for operators that take library args */
static EnumPropertyItem *proxy_group_object_itemf(bContext *C, PointerRNA *UNUSED(ptr),
                                                  PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem item_tmp = {0}, *item = NULL;
	int totitem = 0;
	int i = 0;
	Object *ob = ED_object_active_context(C);
	GroupObject *go;

	if (!ob || !ob->dup_group)
		return DummyRNA_DEFAULT_items;

	/* find the object to affect */
	for (go = ob->dup_group->gobject.first; go; go = go->next) {
		item_tmp.identifier = item_tmp.name = go->ob->id.name + 2;
		item_tmp.value = i++;
		RNA_enum_item_add(&item, &totitem, &item_tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

void OBJECT_OT_proxy_make(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Make Proxy";
	ot->idname = "OBJECT_OT_proxy_make";
	ot->description = "Add empty object to become local replacement data of a library-linked object";

	/* callbacks */
	ot->invoke = make_proxy_invoke;
	ot->exec = make_proxy_exec;
	ot->poll = ED_operator_object_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	/* XXX, relies on hard coded ID at the moment */
	prop = RNA_def_enum(ot->srna, "object", DummyRNA_DEFAULT_items, 0, "Proxy Object",
	                    "Name of lib-linked/grouped object to make a proxy for");
	RNA_def_enum_funcs(prop, proxy_group_object_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

/********************** Clear Parent Operator ******************* */

typedef enum eObClearParentTypes {
	CLEAR_PARENT_ALL = 0,
	CLEAR_PARENT_KEEP_TRANSFORM,
	CLEAR_PARENT_INVERSE
} eObClearParentTypes;

EnumPropertyItem prop_clear_parent_types[] = {
	{CLEAR_PARENT_ALL, "CLEAR", 0, "Clear Parent",
	 "Completely clear the parenting relationship, including involved modifiers is any"},
	{CLEAR_PARENT_KEEP_TRANSFORM, "CLEAR_KEEP_TRANSFORM", 0, "Clear and Keep Transformation",
	 "As 'Clear Parent', but keep the current visual transformations of the object"},
	{CLEAR_PARENT_INVERSE, "CLEAR_INVERSE", 0, "Clear Parent Inverse",
	 "Reset the transform corrections applied to the parenting relationship, does not remove parenting itself"},
	{0, NULL, 0, NULL, NULL}
};

/* Helper for ED_object_parent_clear() - Remove deform-modifiers associated with parent */
static void object_remove_parent_deform_modifiers(Object *ob, const Object *par)
{
	if (ELEM(par->type, OB_ARMATURE, OB_LATTICE, OB_CURVE)) {
		ModifierData *md, *mdn;

		/* assume that we only need to remove the first instance of matching deform modifier here */
		for (md = ob->modifiers.first; md; md = mdn) {
			bool free = false;

			mdn = md->next;

			/* need to match types (modifier + parent) and references */
			if ((md->type == eModifierType_Armature) && (par->type == OB_ARMATURE)) {
				ArmatureModifierData *amd = (ArmatureModifierData *)md;
				if (amd->object == par) {
					free = true;
				}
			}
			else if ((md->type == eModifierType_Lattice) && (par->type == OB_LATTICE)) {
				LatticeModifierData *lmd = (LatticeModifierData *)md;
				if (lmd->object == par) {
					free = true;
				}
			}
			else if ((md->type == eModifierType_Curve) && (par->type == OB_CURVE)) {
				CurveModifierData *cmd = (CurveModifierData *)md;
				if (cmd->object == par) {
					free = true;
				}
			}

			/* free modifier if match */
			if (free) {
				BLI_remlink(&ob->modifiers, md);
				modifier_free(md);
			}
		}
	}
}

void ED_object_parent_clear(Object *ob, const int type)
{
	if (ob->parent == NULL)
		return;

	switch (type) {
		case CLEAR_PARENT_ALL:
		{
			/* for deformers, remove corresponding modifiers to prevent a large number of modifiers building up */
			object_remove_parent_deform_modifiers(ob, ob->parent);

			/* clear parenting relationship completely */
			ob->parent = NULL;
			break;
		}
		case CLEAR_PARENT_KEEP_TRANSFORM:
		{
			/* remove parent, and apply the parented transform result as object's local transforms */
			ob->parent = NULL;
			BKE_object_apply_mat4(ob, ob->obmat, true, false);
			break;
		}
		case CLEAR_PARENT_INVERSE:
		{
			/* object stays parented, but the parent inverse (i.e. offset from parent to retain binding state)
			 * is cleared. In other words: nothing to do here! */
			break;
		}
	}

	/* Always clear parentinv matrix for sake of consistency, see T41950. */
	unit_m4(ob->parentinv);

	DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
}

/* note, poll should check for editable scene */
static int parent_clear_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	const int type = RNA_enum_get(op->ptr, "type");

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		ED_object_parent_clear(ob, type);
	}
	CTX_DATA_END;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Parent";
	ot->description = "Clear the object's parenting";
	ot->idname = "OBJECT_OT_parent_clear";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = parent_clear_exec;

	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna, "type", prop_clear_parent_types, CLEAR_PARENT_ALL, "Type", "");
}

/* ******************** Make Parent Operator *********************** */

void ED_object_parent(Object *ob, Object *par, const int type, const char *substr)
{
	/* Always clear parentinv matrix for sake of consistency, see T41950. */
	unit_m4(ob->parentinv);

	if (!par || BKE_object_parent_loop_check(par, ob)) {
		ob->parent = NULL;
		ob->partype = PAROBJECT;
		ob->parsubstr[0] = 0;
		return;
	}

	/* this could use some more checks */

	ob->parent = par;
	ob->partype &= ~PARTYPE;
	ob->partype |= type;
	BLI_strncpy(ob->parsubstr, substr, sizeof(ob->parsubstr));
}

/* Operator Property */
EnumPropertyItem prop_make_parent_types[] = {
	{PAR_OBJECT, "OBJECT", 0, "Object", ""},
	{PAR_ARMATURE, "ARMATURE", 0, "Armature Deform", ""},
	{PAR_ARMATURE_NAME, "ARMATURE_NAME", 0, "   With Empty Groups", ""},
	{PAR_ARMATURE_AUTO, "ARMATURE_AUTO", 0, "   With Automatic Weights", ""},
	{PAR_ARMATURE_ENVELOPE, "ARMATURE_ENVELOPE", 0, "   With Envelope Weights", ""},
	{PAR_BONE, "BONE", 0, "Bone", ""},
	{PAR_BONE_RELATIVE, "BONE_RELATIVE", 0, "Bone Relative", ""},
	{PAR_CURVE, "CURVE", 0, "Curve Deform", ""},
	{PAR_FOLLOW, "FOLLOW", 0, "Follow Path", ""},
	{PAR_PATH_CONST, "PATH_CONST", 0, "Path Constraint", ""},
	{PAR_LATTICE, "LATTICE", 0, "Lattice Deform", ""},
	{PAR_VERTEX, "VERTEX", 0, "Vertex", ""},
	{PAR_VERTEX_TRI, "VERTEX_TRI", 0, "Vertex (Triangle)", ""},
	{0, NULL, 0, NULL, NULL}
};

bool ED_object_parent_set(ReportList *reports, Main *bmain, Scene *scene, Object *ob, Object *par,
                          int partype, const bool xmirror, const bool keep_transform, const int vert_par[3])
{
	bPoseChannel *pchan = NULL;
	const bool pararm = ELEM(partype, PAR_ARMATURE, PAR_ARMATURE_NAME, PAR_ARMATURE_ENVELOPE, PAR_ARMATURE_AUTO);

	DAG_id_tag_update(&par->id, OB_RECALC_OB);

	/* preconditions */
	if (partype == PAR_FOLLOW || partype == PAR_PATH_CONST) {
		if (par->type != OB_CURVE)
			return 0;
		else {
			Curve *cu = par->data;

			if ((cu->flag & CU_PATH) == 0) {
				cu->flag |= CU_PATH | CU_FOLLOW;
				BKE_displist_make_curveTypes(scene, par, 0);  /* force creation of path data */
			}
			else {
				cu->flag |= CU_FOLLOW;
			}

			/* if follow, add F-Curve for ctime (i.e. "eval_time") so that path-follow works */
			if (partype == PAR_FOLLOW) {
				/* get or create F-Curve */
				bAction *act = verify_adt_action(&cu->id, 1);
				FCurve *fcu = verify_fcurve(act, NULL, NULL, "eval_time", 0, 1);

				/* setup dummy 'generator' modifier here to get 1-1 correspondence still working */
				if (!fcu->bezt && !fcu->fpt && !fcu->modifiers.first)
					add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_GENERATOR);
			}

			/* fall back on regular parenting now (for follow only) */
			if (partype == PAR_FOLLOW)
				partype = PAR_OBJECT;
		}
	}
	else if (ELEM(partype, PAR_BONE, PAR_BONE_RELATIVE)) {
		pchan = BKE_pose_channel_active(par);

		if (pchan == NULL) {
			BKE_report(reports, RPT_ERROR, "No active bone");
			return false;
		}
	}

	if (ob != par) {
		if (BKE_object_parent_loop_check(par, ob)) {
			BKE_report(reports, RPT_ERROR, "Loop in parents");
			return false;
		}
		else {
			Object workob;

			/* apply transformation of previous parenting */
			if (keep_transform) {
				/* was removed because of bug [#23577],
				 * but this can be handy in some cases too [#32616], so make optional */
				BKE_object_apply_mat4(ob, ob->obmat, false, false);
			}

			/* set the parent (except for follow-path constraint option) */
			if (partype != PAR_PATH_CONST) {
				ob->parent = par;
				/* Always clear parentinv matrix for sake of consistency, see T41950. */
				unit_m4(ob->parentinv);
			}

			/* handle types */
			if (pchan)
				BLI_strncpy(ob->parsubstr, pchan->name, sizeof(ob->parsubstr));
			else
				ob->parsubstr[0] = 0;

			if (partype == PAR_PATH_CONST) {
				/* don't do anything here, since this is not technically "parenting" */
			}
			else if (ELEM(partype, PAR_CURVE, PAR_LATTICE) || (pararm)) {
				/* partype is now set to PAROBJECT so that invisible 'virtual' modifiers don't need to be created
				 * NOTE: the old (2.4x) method was to set ob->partype = PARSKEL, creating the virtual modifiers
				 */
				ob->partype = PAROBJECT;  /* note, dna define, not operator property */
				/* ob->partype = PARSKEL; */  /* note, dna define, not operator property */

				/* BUT, to keep the deforms, we need a modifier, and then we need to set the object that it uses
				 * - We need to ensure that the modifier we're adding doesn't already exist, so we check this by
				 *   assuming that the parent is selected too...
				 */
				/* XXX currently this should only happen for meshes, curves, surfaces,
				 * and lattices - this stuff isn't available for metas yet */
				if (ELEM(ob->type, OB_MESH, OB_CURVE, OB_SURF, OB_FONT, OB_LATTICE)) {
					ModifierData *md;

					switch (partype) {
						case PAR_CURVE: /* curve deform */
							if (modifiers_isDeformedByCurve(ob) != par) {
								md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Curve);
								if (md) {
									((CurveModifierData *)md)->object = par;
								}
							}
							break;
						case PAR_LATTICE: /* lattice deform */
							if (modifiers_isDeformedByLattice(ob) != par) {
								md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Lattice);
								if (md) {
									((LatticeModifierData *)md)->object = par;
								}
							}
							break;
						default: /* armature deform */
							if (modifiers_isDeformedByArmature(ob) != par) {
								md = ED_object_modifier_add(reports, bmain, scene, ob, NULL, eModifierType_Armature);
								if (md) {
									((ArmatureModifierData *)md)->object = par;
								}
							}
							break;
					}
				}
			}
			else if (partype == PAR_BONE) {
				ob->partype = PARBONE;  /* note, dna define, not operator property */
				if (pchan->bone)
					pchan->bone->flag &= ~BONE_RELATIVE_PARENTING;
			}
			else if (partype == PAR_BONE_RELATIVE) {
				ob->partype = PARBONE;  /* note, dna define, not operator property */
				if (pchan->bone)
					pchan->bone->flag |= BONE_RELATIVE_PARENTING;
			}
			else if (partype == PAR_VERTEX) {
				ob->partype = PARVERT1;
				ob->par1 = vert_par[0];
			}
			else if (partype == PAR_VERTEX_TRI) {
				ob->partype = PARVERT3;
				copy_v3_v3_int(&ob->par1, vert_par);
			}
			else {
				ob->partype = PAROBJECT;  /* note, dna define, not operator property */
			}

			/* constraint */
			if (partype == PAR_PATH_CONST) {
				bConstraint *con;
				bFollowPathConstraint *data;
				float cmat[4][4], vec[3];

				con = BKE_constraint_add_for_object(ob, "AutoPath", CONSTRAINT_TYPE_FOLLOWPATH);

				data = con->data;
				data->tar = par;

				BKE_constraint_target_matrix_get(scene, con, 0, CONSTRAINT_OBTYPE_OBJECT, NULL, cmat, scene->r.cfra);
				sub_v3_v3v3(vec, ob->obmat[3], cmat[3]);

				copy_v3_v3(ob->loc, vec);
			}
			else if (pararm && (ob->type == OB_MESH) && (par->type == OB_ARMATURE)) {
				if (partype == PAR_ARMATURE_NAME)
					create_vgroups_from_armature(reports, scene, ob, par, ARM_GROUPS_NAME, false);
				else if (partype == PAR_ARMATURE_ENVELOPE)
					create_vgroups_from_armature(reports, scene, ob, par, ARM_GROUPS_ENVELOPE, xmirror);
				else if (partype == PAR_ARMATURE_AUTO) {
					WM_cursor_wait(1);
					create_vgroups_from_armature(reports, scene, ob, par, ARM_GROUPS_AUTO, xmirror);
					WM_cursor_wait(0);
				}
				/* get corrected inverse */
				ob->partype = PAROBJECT;
				BKE_object_workob_calc_parent(scene, ob, &workob);

				invert_m4_m4(ob->parentinv, workob.obmat);
			}
			else {
				/* calculate inverse parent matrix */
				BKE_object_workob_calc_parent(scene, ob, &workob);
				invert_m4_m4(ob->parentinv, workob.obmat);
			}

			DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA);
		}
	}

	return true;
}



static void parent_set_vert_find(KDTree *tree, Object *child, int vert_par[3], bool is_tri)
{
	const float *co_find = child->obmat[3];
	if (is_tri) {
		KDTreeNearest nearest[3];
		int tot;

		tot = BLI_kdtree_find_nearest_n(tree, co_find, nearest, 3);
		BLI_assert(tot == 3);

		vert_par[0] = nearest[0].index;
		vert_par[1] = nearest[1].index;
		vert_par[2] = nearest[2].index;

		BLI_assert(min_iii(UNPACK3(vert_par)) >= 0);
	}
	else {
		vert_par[0] = BLI_kdtree_find_nearest(tree, co_find, NULL);
		BLI_assert(vert_par[0] >= 0);
		vert_par[1] = 0;
		vert_par[2] = 0;
	}
}

static int parent_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *par = ED_object_active_context(C);
	int partype = RNA_enum_get(op->ptr, "type");
	const bool xmirror = RNA_boolean_get(op->ptr, "xmirror");
	const bool keep_transform = RNA_boolean_get(op->ptr, "keep_transform");
	bool ok = true;

	/* vertex parent (kdtree) */
	const bool is_vert_par = ELEM(partype, PAR_VERTEX, PAR_VERTEX_TRI);
	const bool is_tri = partype == PAR_VERTEX_TRI;
	int tree_tot;
	struct KDTree *tree = NULL;
	int vert_par[3] = {0, 0, 0};
	const int *vert_par_p = is_vert_par ? vert_par : NULL;


	if (is_vert_par) {
		tree = BKE_object_as_kdtree(par, &tree_tot);
		BLI_assert(tree != NULL);

		if (tree_tot < (is_tri ? 3 : 1)) {
			BKE_report(op->reports, RPT_ERROR, "Not enough vertices for vertex-parent");
			ok = false;
		}
	}

	if (ok) {
		/* Non vertex-parent */
		CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
		{
			if (is_vert_par) {
				parent_set_vert_find(tree, ob, vert_par, is_tri);
			}

			if (!ED_object_parent_set(op->reports, bmain, scene, ob, par, partype, xmirror, keep_transform, vert_par_p)) {
				ok = false;
				break;
			}
		}
		CTX_DATA_END;
	}

	if (is_vert_par) {
		BLI_kdtree_free(tree);
	}

	if (!ok)
		return OPERATOR_CANCELLED;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	WM_event_add_notifier(C, NC_OBJECT | ND_PARENT, NULL);

	return OPERATOR_FINISHED;
}


static int parent_set_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	Object *ob = ED_object_active_context(C);
	uiPopupMenu *pup = uiPupMenuBegin(C, IFACE_("Set Parent To"), ICON_NONE);
	uiLayout *layout = uiPupMenuLayout(pup);

	wmOperatorType *ot = WM_operatortype_find("OBJECT_OT_parent_set", true);
	PointerRNA opptr;

#if 0
	uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_OBJECT);
#else
	opptr = uiItemFullO_ptr(layout, ot, IFACE_("Object"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT, UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "type", PAR_OBJECT);
	RNA_boolean_set(&opptr, "keep_transform", false);

	opptr = uiItemFullO_ptr(layout, ot, IFACE_("Object (Keep Transform)"), ICON_NONE, NULL, WM_OP_EXEC_DEFAULT,
	                        UI_ITEM_O_RETURN_PROPS);
	RNA_enum_set(&opptr, "type", PAR_OBJECT);
	RNA_boolean_set(&opptr, "keep_transform", true);
#endif
	/* ob becomes parent, make the associated menus */
	if (ob->type == OB_ARMATURE) {
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_ARMATURE);
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_ARMATURE_NAME);
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_ARMATURE_ENVELOPE);
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_ARMATURE_AUTO);
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_BONE);
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_BONE_RELATIVE);
	}
	else if (ob->type == OB_CURVE) {
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_CURVE);
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_FOLLOW);
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_PATH_CONST);
	}
	else if (ob->type == OB_LATTICE) {
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_LATTICE);
	}

	/* vertex parenting */
	if (OB_TYPE_SUPPORT_PARVERT(ob->type)) {
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_VERTEX);
		uiItemEnumO_ptr(layout, ot, NULL, 0, "type", PAR_VERTEX_TRI);
	}

	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

static bool parent_set_draw_check_prop(PointerRNA *ptr, PropertyRNA *prop)
{
	const char *prop_id = RNA_property_identifier(prop);
	const int type = RNA_enum_get(ptr, "type");

	/* Only show XMirror for PAR_ARMATURE_ENVELOPE and PAR_ARMATURE_AUTO! */
	if (STREQ(prop_id, "xmirror")) {
		if (ELEM(type, PAR_ARMATURE_ENVELOPE, PAR_ARMATURE_AUTO))
			return true;
		else
			return false;
	}

	return true;
}

static void parent_set_ui(bContext *C, wmOperator *op)
{
	uiLayout *layout = op->layout;
	wmWindowManager *wm = CTX_wm_manager(C);
	PointerRNA ptr;

	RNA_pointer_create(&wm->id, op->type->srna, op->properties, &ptr);

	/* Main auto-draw call. */
	uiDefAutoButsRNA(layout, &ptr, parent_set_draw_check_prop, '\0');
}

void OBJECT_OT_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Parent";
	ot->description = "Set the object's parenting";
	ot->idname = "OBJECT_OT_parent_set";

	/* api callbacks */
	ot->invoke = parent_set_invoke;
	ot->exec = parent_set_exec;
	ot->poll = ED_operator_object_active;
	ot->ui = parent_set_ui;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", prop_make_parent_types, 0, "Type", "");
	RNA_def_boolean(ot->srna, "xmirror", false, "X Mirror",
	                "Apply weights symmetrically along X axis, for Envelope/Automatic vertex groups creation");
	RNA_def_boolean(ot->srna, "keep_transform", false, "Keep Transform",
	                "Apply transformation before parenting");
}

/* ************ Make Parent Without Inverse Operator ******************* */

static int parent_noinv_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *par = ED_object_active_context(C);

	DAG_id_tag_update(&par->id, OB_RECALC_OB);

	/* context iterator */
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob != par) {
			if (BKE_object_parent_loop_check(par, ob)) {
				BKE_report(op->reports, RPT_ERROR, "Loop in parents");
			}
			else {
				/* clear inverse matrix and also the object location */
				unit_m4(ob->parentinv);
				memset(ob->loc, 0, 3 * sizeof(float));

				/* set recalc flags */
				DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA);

				/* set parenting type for object - object only... */
				ob->parent = par;
				ob->partype = PAROBJECT; /* note, dna define, not operator property */
			}
		}
	}
	CTX_DATA_END;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_parent_no_inverse_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Parent without Inverse";
	ot->description = "Set the object's parenting without setting the inverse parent correction";
	ot->idname = "OBJECT_OT_parent_no_inverse_set";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = parent_noinv_set_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ Clear Slow Parent Operator *********************/

static int object_slow_parent_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob->parent) {
			if (ob->partype & PARSLOW) {
				ob->partype -= PARSLOW;
				BKE_object_where_is_calc(scene, ob);
				ob->partype |= PARSLOW;
				DAG_id_tag_update(&ob->id, OB_RECALC_OB);
			}
		}
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_SCENE, scene);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_slow_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Slow Parent";
	ot->description = "Clear the object's slow parent";
	ot->idname = "OBJECT_OT_slow_parent_clear";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = object_slow_parent_clear_exec;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************** Make Slow Parent Operator *********************/

static int object_slow_parent_set_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob->parent)
			ob->partype |= PARSLOW;

		DAG_id_tag_update(&ob->id, OB_RECALC_OB);
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_SCENE, scene);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_slow_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Slow Parent";
	ot->description = "Set the object's slow parent";
	ot->idname = "OBJECT_OT_slow_parent_set";

	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = object_slow_parent_set_exec;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************** Clear Track Operator ******************* */

enum {
	CLEAR_TRACK                = 1,
	CLEAR_TRACK_KEEP_TRANSFORM = 2,
};

static EnumPropertyItem prop_clear_track_types[] = {
	{CLEAR_TRACK, "CLEAR", 0, "Clear Track", ""},
	{CLEAR_TRACK_KEEP_TRANSFORM, "CLEAR_KEEP_TRANSFORM", 0, "Clear and Keep Transformation (Clear Track)", ""},
	{0, NULL, 0, NULL, NULL}
};

/* note, poll should check for editable scene */
static int object_track_clear_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	const int type = RNA_enum_get(op->ptr, "type");

	if (CTX_data_edit_object(C)) {
		BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in edit mode");
		return OPERATOR_CANCELLED;
	}
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		bConstraint *con, *pcon;

		/* remove track-object for old track */
		ob->track = NULL;
		DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

		/* also remove all tracking constraints */
		for (con = ob->constraints.last; con; con = pcon) {
			pcon = con->prev;
			if (ELEM(con->type, CONSTRAINT_TYPE_TRACKTO, CONSTRAINT_TYPE_LOCKTRACK, CONSTRAINT_TYPE_DAMPTRACK))
				BKE_constraint_remove(&ob->constraints, con);
		}

		if (type == CLEAR_TRACK_KEEP_TRANSFORM)
			BKE_object_apply_mat4(ob, ob->obmat, true, true);
	}
	CTX_DATA_END;

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_track_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Track";
	ot->description = "Clear tracking constraint or flag from object";
	ot->idname = "OBJECT_OT_track_clear";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_track_clear_exec;

	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna, "type", prop_clear_track_types, 0, "Type", "");
}

/************************** Make Track Operator *****************************/

enum {
	CREATE_TRACK_DAMPTRACK = 1,
	CREATE_TRACK_TRACKTO   = 2,
	CREATE_TRACK_LOCKTRACK = 3,
};

static EnumPropertyItem prop_make_track_types[] = {
	{CREATE_TRACK_DAMPTRACK, "DAMPTRACK", 0, "Damped Track Constraint", ""},
	{CREATE_TRACK_TRACKTO, "TRACKTO", 0, "Track To Constraint", ""},
	{CREATE_TRACK_LOCKTRACK, "LOCKTRACK", 0, "Lock Track Constraint", ""},
	{0, NULL, 0, NULL, NULL}
};

static int track_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Object *obact = ED_object_active_context(C);

	const int type = RNA_enum_get(op->ptr, "type");

	switch (type) {
		case CREATE_TRACK_DAMPTRACK:
		{
			bConstraint *con;
			bDampTrackConstraint *data;

			CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
			{
				if (ob != obact) {
					con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_DAMPTRACK);

					data = con->data;
					data->tar = obact;
					DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

					/* Lamp, Camera and Speaker track differently by default */
					if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
						data->trackflag = TRACK_nZ;
					}
				}
			}
			CTX_DATA_END;
			break;
		}
		case CREATE_TRACK_TRACKTO:
		{
			bConstraint *con;
			bTrackToConstraint *data;

			CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
			{
				if (ob != obact) {
					con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_TRACKTO);

					data = con->data;
					data->tar = obact;
					DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

					/* Lamp, Camera and Speaker track differently by default */
					if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
						data->reserved1 = TRACK_nZ;
						data->reserved2 = UP_Y;
					}
				}
			}
			CTX_DATA_END;
			break;
		}
		case CREATE_TRACK_LOCKTRACK:
		{
			bConstraint *con;
			bLockTrackConstraint *data;

			CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
			{
				if (ob != obact) {
					con = BKE_constraint_add_for_object(ob, "AutoTrack", CONSTRAINT_TYPE_LOCKTRACK);

					data = con->data;
					data->tar = obact;
					DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

					/* Lamp, Camera and Speaker track differently by default */
					if (ELEM(ob->type, OB_LAMP, OB_CAMERA, OB_SPEAKER)) {
						data->trackflag = TRACK_nZ;
						data->lockflag = LOCK_Y;
					}
				}
			}
			CTX_DATA_END;
			break;
		}
	}

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_track_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Track";
	ot->description = "Make the object track another object, using various methods/constraints";
	ot->idname = "OBJECT_OT_track_set";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = track_set_exec;

	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_make_track_types, 0, "Type", "");
}

/************************** Move to Layer Operator *****************************/

static unsigned int move_to_layer_init(bContext *C, wmOperator *op)
{
	int values[20], a;
	unsigned int lay = 0;

	if (!RNA_struct_property_is_set(op->ptr, "layers")) {
		/* note: layers are set in bases, library objects work for this */
		CTX_DATA_BEGIN (C, Base *, base, selected_bases)
		{
			lay |= base->lay;
		}
		CTX_DATA_END;

		for (a = 0; a < 20; a++)
			values[a] = (lay & (1 << a));

		RNA_boolean_set_array(op->ptr, "layers", values);
	}
	else {
		RNA_boolean_get_array(op->ptr, "layers", values);

		for (a = 0; a < 20; a++)
			if (values[a])
				lay |= (1 << a);
	}

	return lay;
}

static int move_to_layer_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	View3D *v3d = CTX_wm_view3d(C);
	if (v3d && v3d->localvd) {
		return WM_operator_confirm_message(C, op, "Move from localview");
	}
	else {
		move_to_layer_init(C, op);
		return WM_operator_props_popup(C, op, event);
	}
}

static int move_to_layer_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	unsigned int lay, local;
	/* bool is_lamp = false; */ /* UNUSED */

	lay = move_to_layer_init(C, op);
	lay &= 0xFFFFFF;

	if (lay == 0) return OPERATOR_CANCELLED;

	if (v3d && v3d->localvd) {
		/* now we can move out of localview. */
		/* note: layers are set in bases, library objects work for this */
		CTX_DATA_BEGIN (C, Base *, base, selected_bases)
		{
			lay = base->lay & ~v3d->lay;
			base->lay = lay;
			base->object->lay = lay;
			base->object->flag &= ~SELECT;
			base->flag &= ~SELECT;
			/* if (base->object->type == OB_LAMP) is_lamp = true; */
		}
		CTX_DATA_END;
	}
	else {
		/* normal non localview operation */
		/* note: layers are set in bases, library objects work for this */
		CTX_DATA_BEGIN (C, Base *, base, selected_bases)
		{
			/* upper byte is used for local view */
			local = base->lay & 0xFF000000;
			base->lay = lay + local;
			base->object->lay = lay;
			/* if (base->object->type == OB_LAMP) is_lamp = true; */
		}
		CTX_DATA_END;
	}

	/* warning, active object may be hidden now */

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, scene);
	WM_event_add_notifier(C, NC_SCENE | ND_LAYER_CONTENT, scene);

	DAG_relations_tag_update(bmain);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_move_to_layer(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Move to Layer";
	ot->description = "Move the object to different layers";
	ot->idname = "OBJECT_OT_move_to_layer";

	/* api callbacks */
	ot->invoke = move_to_layer_invoke;
	ot->exec = move_to_layer_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean_layer_member(ot->srna, "layers", 20, NULL, "Layer", "");
}

/************************** Link to Scene Operator *****************************/

#if 0
static void link_to_scene(Main *UNUSED(bmain), unsigned short UNUSED(nr))
{
	Scene *sce = (Scene *) BLI_findlink(&bmain->scene, G.curscreen->scenenr - 1);
	Base *base, *nbase;

	if (sce == NULL) return;
	if (sce->id.lib) return;

	for (base = FIRSTBASE; base; base = base->next) {
		if (TESTBASE(v3d, base)) {
			nbase = MEM_mallocN(sizeof(Base), "newbase");
			*nbase = *base;
			BLI_addhead(&(sce->base), nbase);
			id_us_plus((ID *)base->object);
		}
	}
}
#endif

Base *ED_object_scene_link(Scene *scene, Object *ob)
{
	Base *base;

	if (BKE_scene_base_find(scene, ob)) {
		return NULL;
	}

	base = BKE_scene_base_add(scene, ob);
	id_us_plus(&ob->id);

	return base;
}

static int make_links_scene_exec(bContext *C, wmOperator *op)
{
	Scene *scene_to = BLI_findlink(&CTX_data_main(C)->scene, RNA_enum_get(op->ptr, "scene"));

	if (scene_to == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Could not find scene");
		return OPERATOR_CANCELLED;
	}

	if (scene_to == CTX_data_scene(C)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot link objects into the same scene");
		return OPERATOR_CANCELLED;
	}

	if (scene_to->id.lib) {
		BKE_report(op->reports, RPT_ERROR, "Cannot link objects into a linked scene");
		return OPERATOR_CANCELLED;
	}

	CTX_DATA_BEGIN (C, Base *, base, selected_bases)
	{
		ED_object_scene_link(scene_to, base->object);
	}
	CTX_DATA_END;

	/* redraw the 3D view because the object center points are colored differently */
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, NULL);

	/* one day multiple scenes will be visible, then we should have some update function for them */
	return OPERATOR_FINISHED;
}

enum {
	MAKE_LINKS_OBDATA     = 1,
	MAKE_LINKS_MATERIALS  = 2,
	MAKE_LINKS_ANIMDATA   = 3,
	MAKE_LINKS_GROUP      = 4,
	MAKE_LINKS_DUPLIGROUP = 5,
	MAKE_LINKS_MODIFIERS  = 6,
	MAKE_LINKS_FONTS      = 7,
};

/* Return true if make link data is allowed, false otherwise */
static bool allow_make_links_data(const int type, Object *ob_src, Object *ob_dst)
{
	switch (type) {
		case MAKE_LINKS_OBDATA:
			if (ob_src->type == ob_dst->type && ob_src->type != OB_EMPTY) {
				return true;
			}
			break;
		case MAKE_LINKS_MATERIALS:
			if (OB_TYPE_SUPPORT_MATERIAL(ob_src->type) && OB_TYPE_SUPPORT_MATERIAL(ob_dst->type)) {
				return true;
			}
			break;
		case MAKE_LINKS_ANIMDATA:
		case MAKE_LINKS_GROUP:
		case MAKE_LINKS_DUPLIGROUP:
			return true;
		case MAKE_LINKS_MODIFIERS:
			if (!ELEM(OB_EMPTY, ob_src->type, ob_dst->type)) {
				return true;
			}
			break;
		case MAKE_LINKS_FONTS:
			if ((ob_src->data != ob_dst->data) && (ob_src->type == OB_FONT) && (ob_dst->type == OB_FONT)) {
				return true;
			}
			break;
	}
	return false;
}

static int make_links_data_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	const int type = RNA_enum_get(op->ptr, "type");
	Object *ob_src;
	ID *obdata_id;
	int a;

	/* group */
	LinkNode *ob_groups = NULL;
	bool is_cycle = false;
	bool is_lib = false;

	ob_src = ED_object_active_context(C);

	/* avoid searching all groups in source object each time */
	if (type == MAKE_LINKS_GROUP) {
		ob_groups = BKE_object_groups(ob_src);
	}

	CTX_DATA_BEGIN (C, Base *, base_dst, selected_editable_bases)
	{
		Object *ob_dst = base_dst->object;

		if (ob_src != ob_dst) {
			if (allow_make_links_data(type, ob_src, ob_dst)) {
				obdata_id = ob_dst->data;

				switch (type) {
					case MAKE_LINKS_OBDATA: /* obdata */
						obdata_id->us--;

						obdata_id = ob_src->data;
						id_us_plus(obdata_id);
						ob_dst->data = obdata_id;

						/* if amount of material indices changed: */
						test_object_materials(bmain, ob_dst->data);

						DAG_id_tag_update(&ob_dst->id, OB_RECALC_DATA);
						break;
					case MAKE_LINKS_MATERIALS:
						/* new approach, using functions from kernel */
						for (a = 0; a < ob_src->totcol; a++) {
							Material *ma = give_current_material(ob_src, a + 1);
							assign_material(ob_dst, ma, a + 1, BKE_MAT_ASSIGN_USERPREF); /* also works with ma==NULL */
						}
						DAG_id_tag_update(&ob_dst->id, 0);
						break;
					case MAKE_LINKS_ANIMDATA:
						BKE_copy_animdata_id((ID *)ob_dst, (ID *)ob_src, false);
						if (ob_dst->data && ob_src->data) {
							if (obdata_id->lib) {
								is_lib = true;
								break;
							}
							BKE_copy_animdata_id((ID *)ob_dst->data, (ID *)ob_src->data, false);
						}
						DAG_id_tag_update(&ob_dst->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
						break;
					case MAKE_LINKS_GROUP:
					{
						LinkNode *group_node;

						/* first clear groups */
						BKE_object_groups_clear(scene, base_dst, ob_dst);

						/* now add in the groups from the link nodes */
						for (group_node = ob_groups; group_node; group_node = group_node->next) {
							if (ob_dst->dup_group != group_node->link) {
								BKE_group_object_add(group_node->link, ob_dst, scene, base_dst);
							}
							else {
								is_cycle = true;
							}
						}
						break;
					}
					case MAKE_LINKS_DUPLIGROUP:
						ob_dst->dup_group = ob_src->dup_group;
						if (ob_dst->dup_group) {
							id_lib_extern(&ob_dst->dup_group->id);
							ob_dst->transflag |= OB_DUPLIGROUP;
						}
						break;
					case MAKE_LINKS_MODIFIERS:
						BKE_object_link_modifiers(ob_dst, ob_src);
						DAG_id_tag_update(&ob_dst->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
						break;
					case MAKE_LINKS_FONTS:
					{
						Curve *cu_src = ob_src->data;
						Curve *cu_dst = ob_dst->data;

						if (obdata_id->lib) {
							is_lib = true;
							break;
						}

						if (cu_dst->vfont) cu_dst->vfont->id.us--;
						cu_dst->vfont = cu_src->vfont;
						id_us_plus((ID *)cu_dst->vfont);
						if (cu_dst->vfontb) cu_dst->vfontb->id.us--;
						cu_dst->vfontb = cu_src->vfontb;
						id_us_plus((ID *)cu_dst->vfontb);
						if (cu_dst->vfonti) cu_dst->vfonti->id.us--;
						cu_dst->vfonti = cu_src->vfonti;
						id_us_plus((ID *)cu_dst->vfonti);
						if (cu_dst->vfontbi) cu_dst->vfontbi->id.us--;
						cu_dst->vfontbi = cu_src->vfontbi;
						id_us_plus((ID *)cu_dst->vfontbi);

						DAG_id_tag_update(&ob_dst->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);
						break;
					}
				}
			}
		}
	}
	CTX_DATA_END;

	if (type == MAKE_LINKS_GROUP) {
		if (ob_groups) {
			BLI_linklist_free(ob_groups, NULL);
		}

		if (is_cycle) {
			BKE_report(op->reports, RPT_WARNING, "Skipped some groups because of cycle detected");
		}
	}

	if (is_lib) {
		BKE_report(op->reports, RPT_WARNING, "Skipped editing library object data");
	}

	DAG_relations_tag_update(bmain);
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, CTX_wm_view3d(C));
	WM_event_add_notifier(C, NC_OBJECT, NULL);

	return OPERATOR_FINISHED;
}


void OBJECT_OT_make_links_scene(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Link Objects to Scene";
	ot->description = "Link selection to another scene";
	ot->idname = "OBJECT_OT_make_links_scene";

	/* api callbacks */
	ot->invoke = WM_enum_search_invoke;
	ot->exec = make_links_scene_exec;
	/* better not run the poll check */

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	prop = RNA_def_enum(ot->srna, "scene", DummyRNA_NULL_items, 0, "Scene", "");
	RNA_def_enum_funcs(prop, RNA_scene_local_itemf);
	RNA_def_property_flag(prop, PROP_ENUM_NO_TRANSLATE);
	ot->prop = prop;
}

void OBJECT_OT_make_links_data(wmOperatorType *ot)
{
	static EnumPropertyItem make_links_items[] = {
		{MAKE_LINKS_OBDATA,     "OBDATA", 0, "Object Data", ""},
		{MAKE_LINKS_MATERIALS,  "MATERIAL", 0, "Materials", ""},
		{MAKE_LINKS_ANIMDATA,   "ANIMATION", 0, "Animation Data", ""},
		{MAKE_LINKS_GROUP,      "GROUPS", 0, "Group", ""},
		{MAKE_LINKS_DUPLIGROUP, "DUPLIGROUP", 0, "DupliGroup", ""},
		{MAKE_LINKS_MODIFIERS,  "MODIFIERS", 0, "Modifiers", ""},
		{MAKE_LINKS_FONTS,      "FONTS", 0, "Fonts", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Link Data";
	ot->description = "Apply active object links to other selected objects";
	ot->idname = "OBJECT_OT_make_links_data";

	/* api callbacks */
	ot->exec = make_links_data_exec;
	ot->poll = ED_operator_object_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", make_links_items, 0, "Type", "");
}


/**************************** Make Single User ********************************/

static void single_object_users(Main *bmain, Scene *scene, View3D *v3d, const int flag, const bool copy_groups)
{
	Base *base;
	Object *ob, *obn;
	Group *group, *groupn;
	GroupObject *go;

	clear_sca_new_poins();  /* sensor/contr/act */

	/* newid may still have some trash from Outliner tree building,
	 * so clear that first to avoid errors [#26002] */
	for (ob = bmain->object.first; ob; ob = ob->id.next)
		ob->id.newid = NULL;

	/* duplicate (must set newid) */
	for (base = FIRSTBASE; base; base = base->next) {
		ob = base->object;

		if ((base->flag & flag) == flag) {
			if (ob->id.lib == NULL && ob->id.us > 1) {
				/* base gets copy of object */
				obn = BKE_object_copy(ob);
				base->object = obn;

				if (copy_groups) {
					if (ob->flag & OB_FROMGROUP) {
						obn->flag |= OB_FROMGROUP;
					}
				}
				else {
					/* copy already clears */
				}
				base->flag = obn->flag;

				ob->id.us--;
			}
		}
	}

	/* duplicate groups that consist entirely of duplicated objects */
	for (group = bmain->group.first; group; group = group->id.next) {
		group->id.newid = NULL;

		if (copy_groups && group->gobject.first) {
			bool all_duplicated = true;

			for (go = group->gobject.first; go; go = go->next) {
				if (!(go->ob && (go->ob->id.newid))) {
					all_duplicated = false;
					break;
				}
			}

			if (all_duplicated) {
				groupn = BKE_group_copy(group);

				for (go = groupn->gobject.first; go; go = go->next)
					go->ob = (Object *)go->ob->id.newid;
			}
		}
	}

	/* group pointers in scene */
	BKE_scene_groups_relink(scene);

	ID_NEW(scene->camera);
	if (v3d) ID_NEW(v3d->camera);

	/* object and group pointers */
	for (base = FIRSTBASE; base; base = base->next) {
		BKE_object_relink(base->object);
	}

	set_sca_new_poins();
}

/* not an especially efficient function, only added so the single user
 * button can be functional.*/
void ED_object_single_user(Main *bmain, Scene *scene, Object *ob)
{
	Base *base;
	const bool copy_groups = false;

	for (base = FIRSTBASE; base; base = base->next) {
		if (base->object == ob) base->flag |=  OB_DONE;
		else base->flag &= ~OB_DONE;
	}

	single_object_users(bmain, scene, NULL, OB_DONE, copy_groups);
}

static void new_id_matar(Material **matar, const int totcol)
{
	ID *id;
	int a;

	for (a = 0; a < totcol; a++) {
		id = (ID *)matar[a];
		if (id && id->lib == NULL) {
			if (id->newid) {
				matar[a] = (Material *)id->newid;
				id_us_plus(id->newid);
				id->us--;
			}
			else if (id->us > 1) {
				matar[a] = BKE_material_copy(matar[a]);
				id->us--;
				id->newid = (ID *)matar[a];
			}
		}
	}
}

static void single_obdata_users(Main *bmain, Scene *scene, const int flag)
{
	Object *ob;
	Lamp *la;
	Curve *cu;
	/* Camera *cam; */
	Base *base;
	Mesh *me;
	Lattice *lat;
	ID *id;
	int a;

	for (base = FIRSTBASE; base; base = base->next) {
		ob = base->object;
		if (ob->id.lib == NULL && (base->flag & flag) == flag) {
			id = ob->data;

			if (id && id->us > 1 && id->lib == NULL) {
				DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

				switch (ob->type) {
					case OB_LAMP:
						ob->data = la = BKE_lamp_copy(ob->data);
						for (a = 0; a < MAX_MTEX; a++) {
							if (la->mtex[a]) {
								ID_NEW(la->mtex[a]->object);
							}
						}
						break;
					case OB_CAMERA:
						ob->data = BKE_camera_copy(ob->data);
						break;
					case OB_MESH:
						ob->data = me = BKE_mesh_copy(ob->data);
						if (me->key)
							BKE_copy_animdata_id_action((ID *)me->key);
						break;
					case OB_MBALL:
						ob->data = BKE_mball_copy(ob->data);
						break;
					case OB_CURVE:
					case OB_SURF:
					case OB_FONT:
						ob->data = cu = BKE_curve_copy(ob->data);
						ID_NEW(cu->bevobj);
						ID_NEW(cu->taperobj);
						if (cu->key)
							BKE_copy_animdata_id_action((ID *)cu->key);
						break;
					case OB_LATTICE:
						ob->data = lat = BKE_lattice_copy(ob->data);
						if (lat->key)
							BKE_copy_animdata_id_action((ID *)lat->key);
						break;
					case OB_ARMATURE:
						DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
						ob->data = BKE_armature_copy(ob->data);
						BKE_pose_rebuild(ob, ob->data);
						break;
					case OB_SPEAKER:
						ob->data = BKE_speaker_copy(ob->data);
						break;
					default:
						if (G.debug & G_DEBUG)
							printf("ERROR %s: can't copy %s\n", __func__, id->name);
						return;
				}

				/* Copy animation data after object data became local,
				 * otherwise old and new object data will share the same
				 * AnimData structure, which is not what we want.
				 *                                             (sergey)
				 */
				BKE_copy_animdata_id_action((ID *)ob->data);

				id->us--;
				id->newid = ob->data;
			}
		}
	}

	me = bmain->mesh.first;
	while (me) {
		ID_NEW(me->texcomesh);
		me = me->id.next;
	}
}

static void single_object_action_users(Scene *scene, const int flag)
{
	Object *ob;
	Base *base;

	for (base = FIRSTBASE; base; base = base->next) {
		ob = base->object;
		if (ob->id.lib == NULL && (flag == 0 || (base->flag & SELECT)) ) {
			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			BKE_copy_animdata_id_action(&ob->id);
		}
	}
}

static void single_mat_users(Scene *scene, const int flag, const bool do_textures)
{
	Object *ob;
	Base *base;
	Material *ma, *man;
	Tex *tex;
	int a, b;

	for (base = FIRSTBASE; base; base = base->next) {
		ob = base->object;
		if (ob->id.lib == NULL && (flag == 0 || (base->flag & SELECT)) ) {
			for (a = 1; a <= ob->totcol; a++) {
				ma = give_current_material(ob, a);
				if (ma) {
					/* do not test for LIB_NEW: this functions guaranteed delivers single_users! */

					if (ma->id.us > 1) {
						man = BKE_material_copy(ma);
						BKE_copy_animdata_id_action(&man->id);

						man->id.us = 0;
						assign_material(ob, man, a, BKE_MAT_ASSIGN_USERPREF);

						if (do_textures) {
							for (b = 0; b < MAX_MTEX; b++) {
								if (ma->mtex[b] && (tex = ma->mtex[b]->tex)) {
									if (tex->id.us > 1) {
										tex->id.us--;
										tex = BKE_texture_copy(tex);
										BKE_copy_animdata_id_action(&tex->id);
										man->mtex[b]->tex = tex;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

static void do_single_tex_user(Tex **from)
{
	Tex *tex, *texn;

	tex = *from;
	if (tex == NULL) return;

	if (tex->id.newid) {
		*from = (Tex *)tex->id.newid;
		id_us_plus(tex->id.newid);
		tex->id.us--;
	}
	else if (tex->id.us > 1) {
		texn = BKE_texture_copy(tex);
		BKE_copy_animdata_id_action(&texn->id);
		tex->id.newid = (ID *)texn;
		tex->id.us--;
		*from = texn;
	}
}

static void single_tex_users_expand(Main *bmain)
{
	/* only when 'parent' blocks are LIB_NEW */
	Material *ma;
	Lamp *la;
	World *wo;
	int b;

	for (ma = bmain->mat.first; ma; ma = ma->id.next) {
		if (ma->id.flag & LIB_NEW) {
			for (b = 0; b < MAX_MTEX; b++) {
				if (ma->mtex[b] && ma->mtex[b]->tex) {
					do_single_tex_user(&(ma->mtex[b]->tex));
				}
			}
		}
	}

	for (la = bmain->lamp.first; la; la = la->id.next) {
		if (la->id.flag & LIB_NEW) {
			for (b = 0; b < MAX_MTEX; b++) {
				if (la->mtex[b] && la->mtex[b]->tex) {
					do_single_tex_user(&(la->mtex[b]->tex));
				}
			}
		}
	}

	for (wo = bmain->world.first; wo; wo = wo->id.next) {
		if (wo->id.flag & LIB_NEW) {
			for (b = 0; b < MAX_MTEX; b++) {
				if (wo->mtex[b] && wo->mtex[b]->tex) {
					do_single_tex_user(&(wo->mtex[b]->tex));
				}
			}
		}
	}
}

static void single_mat_users_expand(Main *bmain)
{
	/* only when 'parent' blocks are LIB_NEW */
	Object *ob;
	Mesh *me;
	Curve *cu;
	MetaBall *mb;
	Material *ma;
	int a;

	for (ob = bmain->object.first; ob; ob = ob->id.next)
		if (ob->id.flag & LIB_NEW)
			new_id_matar(ob->mat, ob->totcol);

	for (me = bmain->mesh.first; me; me = me->id.next)
		if (me->id.flag & LIB_NEW)
			new_id_matar(me->mat, me->totcol);

	for (cu = bmain->curve.first; cu; cu = cu->id.next)
		if (cu->id.flag & LIB_NEW)
			new_id_matar(cu->mat, cu->totcol);

	for (mb = bmain->mball.first; mb; mb = mb->id.next)
		if (mb->id.flag & LIB_NEW)
			new_id_matar(mb->mat, mb->totcol);

	/* material imats  */
	for (ma = bmain->mat.first; ma; ma = ma->id.next)
		if (ma->id.flag & LIB_NEW)
			for (a = 0; a < MAX_MTEX; a++)
				if (ma->mtex[a])
					ID_NEW(ma->mtex[a]->object);
}

/* used for copying scenes */
void ED_object_single_users(Main *bmain, Scene *scene, const bool full, const bool copy_groups)
{
	single_object_users(bmain, scene, NULL, 0, copy_groups);

	if (full) {
		single_obdata_users(bmain, scene, 0);
		single_object_action_users(scene, 0);
		single_mat_users_expand(bmain);
		single_tex_users_expand(bmain);
	}

	BKE_main_id_clear_newpoins(bmain);
}

/******************************* Make Local ***********************************/

/* helper for below, ma was checked to be not NULL */
static void make_local_makelocalmaterial(Material *ma)
{
	AnimData *adt;
	int b;

	id_make_local(&ma->id, false);

	for (b = 0; b < MAX_MTEX; b++)
		if (ma->mtex[b] && ma->mtex[b]->tex)
			id_make_local(&ma->mtex[b]->tex->id, false);

	adt = BKE_animdata_from_id(&ma->id);
	if (adt) BKE_animdata_make_local(adt);

	/* nodetree? XXX */
}

enum {
	MAKE_LOCAL_SELECT_OB              = 1,
	MAKE_LOCAL_SELECT_OBDATA          = 2,
	MAKE_LOCAL_SELECT_OBDATA_MATERIAL = 3,
	MAKE_LOCAL_ALL                    = 4,
};

static bool tag_localizable_looper(void *UNUSED(user_data), ID **id_pointer, const int UNUSED(cd_flag))
{
	if (*id_pointer) {
		(*id_pointer)->flag &= ~LIB_DOIT;
	}
	return true;
}

static void tag_localizable_objects(bContext *C, const int mode)
{
	Main *bmain = CTX_data_main(C);
	Object *object;

	BKE_main_id_tag_all(bmain, false);

	/* Set LIB_DOIT flag for all selected objects, so next we can check whether
	 * object is gonna to become local or not.
	 */
	CTX_DATA_BEGIN (C, Object *, object, selected_objects)
	{
		object->id.flag |= LIB_DOIT;

		/* If data is also gonna to become local, mark data we're interested in
		 * as gonna-to-be-local.
		 */
		if (mode == MAKE_LOCAL_SELECT_OBDATA && object->data) {
			ID *data_id = (ID *) object->data;
			data_id->flag |= LIB_DOIT;
		}
	}
	CTX_DATA_END;

	/* Also forbid making objects local if other library objects are using
	 * them for modifiers or constraints.
	 */
	for (object = bmain->object.first; object; object = object->id.next) {
		if ((object->id.flag & LIB_DOIT) == 0) {
			BKE_library_foreach_ID_link(&object->id, tag_localizable_looper, NULL, IDWALK_READONLY);
		}
		if (object->data) {
			ID *data_id = (ID *) object->data;
			if ((data_id->flag & LIB_DOIT) == 0) {
				BKE_library_foreach_ID_link(data_id, tag_localizable_looper, NULL, IDWALK_READONLY);
			}
		}
	}

	/* TODO(sergey): Drivers targets? */
}

/**
 * Instance indirectly referenced zero user objects,
 * otherwise they're lost on reload, see T40595.
 */
static bool make_local_all__instance_indirect_unused(Main *bmain, Scene *scene)
{
	Object *ob;
	bool changed = false;

	for (ob = bmain->object.first; ob; ob = ob->id.next) {
		if (ob->id.lib && (ob->id.us == 0)) {
			Base *base;

			ob->id.us = 1;

			/* not essential, but for correctness */
			id_lib_extern(&ob->id);

			base = BKE_scene_base_add(scene, ob);
			base->flag |= SELECT;
			base->object->flag = base->flag;
			DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME);

			changed = true;
		}
	}

	return changed;
}

static int make_local_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	AnimData *adt;
	ParticleSystem *psys;
	Material *ma, ***matarar;
	Lamp *la;
	ID *id;
	const int mode = RNA_enum_get(op->ptr, "type");
	int a, b;

	if (mode == MAKE_LOCAL_ALL) {
		/* de-select so the user can differentiate newly instanced from existing objects */
		BKE_scene_base_deselect_all(scene);

		if (make_local_all__instance_indirect_unused(bmain, scene)) {
			BKE_report(op->reports, RPT_INFO,
			           "Orphan library objects added to the current scene to avoid loss");
		}

		BKE_library_make_local(bmain, NULL, false); /* NULL is all libs */
		WM_event_add_notifier(C, NC_WINDOW, NULL);
		return OPERATOR_FINISHED;
	}

	tag_localizable_objects(C, mode);
	BKE_main_id_clear_newpoins(bmain);

	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		if ((ob->id.flag & LIB_DOIT) == 0) {
			continue;
		}

		if (ob->id.lib)
			id_make_local(&ob->id, false);
	}
	CTX_DATA_END;

	/* maybe object pointers */
	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		if (ob->id.lib == NULL) {
			ID_NEW(ob->parent);
		}
	}
	CTX_DATA_END;

	CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
	{
		if ((ob->id.flag & LIB_DOIT) == 0) {
			continue;
		}

		id = ob->data;

		if (id && (ELEM(mode, MAKE_LOCAL_SELECT_OBDATA, MAKE_LOCAL_SELECT_OBDATA_MATERIAL))) {
			id_make_local(id, false);
			adt = BKE_animdata_from_id(id);
			if (adt) BKE_animdata_make_local(adt);

			/* tag indirect data direct */
			matarar = give_matarar(ob);
			if (matarar) {
				for (a = 0; a < ob->totcol; a++) {
					ma = (*matarar)[a];
					if (ma)
						id_lib_extern(&ma->id);
				}
			}
		}

		for (psys = ob->particlesystem.first; psys; psys = psys->next)
			id_make_local(&psys->part->id, false);

		adt = BKE_animdata_from_id(&ob->id);
		if (adt) BKE_animdata_make_local(adt);
	}
	CTX_DATA_END;

	if (mode == MAKE_LOCAL_SELECT_OBDATA_MATERIAL) {
		CTX_DATA_BEGIN (C, Object *, ob, selected_objects)
		{
			if ((ob->id.flag & LIB_DOIT) == 0) {
				continue;
			}

			if (ob->type == OB_LAMP) {
				la = ob->data;

				for (b = 0; b < MAX_MTEX; b++)
					if (la->mtex[b] && la->mtex[b]->tex)
						id_make_local(&la->mtex[b]->tex->id, false);
			}
			else {
				for (a = 0; a < ob->totcol; a++) {
					ma = ob->mat[a];
					if (ma)
						make_local_makelocalmaterial(ma);
				}

				matarar = (Material ***)give_matarar(ob);
				if (matarar) {
					for (a = 0; a < ob->totcol; a++) {
						ma = (*matarar)[a];
						if (ma)
							make_local_makelocalmaterial(ma);
					}
				}
			}
		}
		CTX_DATA_END;
	}

	WM_event_add_notifier(C, NC_WINDOW, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_make_local(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{MAKE_LOCAL_SELECT_OB, "SELECT_OBJECT", 0, "Selected Objects", ""},
		{MAKE_LOCAL_SELECT_OBDATA, "SELECT_OBDATA", 0, "Selected Objects and Data", ""},
		{MAKE_LOCAL_SELECT_OBDATA_MATERIAL, "SELECT_OBDATA_MATERIAL", 0, "Selected Objects, Data and Materials", ""},
		{MAKE_LOCAL_ALL, "ALL", 0, "All", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Make Local";
	ot->description = "Make library linked datablocks local to this file";
	ot->idname = "OBJECT_OT_make_local";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = make_local_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
}

enum {
	/* Be careful with those values, they are used as bitflags in some cases, in others as bool...
	 * See single_object_users, single_obdata_users, single_object_action_users, etc.< */
	MAKE_SINGLE_USER_ALL      = 0,
	MAKE_SINGLE_USER_SELECTED = SELECT,
};

static int make_single_user_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C); /* ok if this is NULL */
	const int flag = RNA_enum_get(op->ptr, "type");
	const bool copy_groups = false;
	bool update_deps = false;

	BKE_main_id_clear_newpoins(bmain);

	if (RNA_boolean_get(op->ptr, "object")) {
		single_object_users(bmain, scene, v3d, flag, copy_groups);

		/* needed since object relationships may have changed */
		update_deps = true;
	}

	if (RNA_boolean_get(op->ptr, "obdata")) {
		single_obdata_users(bmain, scene, flag);
	}

	if (RNA_boolean_get(op->ptr, "material")) {
		single_mat_users(scene, flag, RNA_boolean_get(op->ptr, "texture"));
	}

#if 0 /* can't do this separate from materials */
	if (RNA_boolean_get(op->ptr, "texture"))
		single_mat_users(scene, flag, true);
#endif
	if (RNA_boolean_get(op->ptr, "animation")) {
		single_object_action_users(scene, flag);
	}

	/* TODO(sergey): This should not be needed, however some tool still could rely
	 *               on the fact, that id->newid is kept NULL by default.
	 *               Need to make sure all the guys are learing newid before they're
	 *               using it, not after.
	 */
	BKE_main_id_clear_newpoins(bmain);

	WM_event_add_notifier(C, NC_WINDOW, NULL);

	if (update_deps) {
		DAG_relations_tag_update(bmain);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_make_single_user(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[] = {
		{MAKE_SINGLE_USER_SELECTED, "SELECTED_OBJECTS", 0, "Selected Objects", ""},
		{MAKE_SINGLE_USER_ALL, "ALL", 0, "All", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name = "Make Single User";
	ot->description = "Make linked data local to each object";
	ot->idname = "OBJECT_OT_make_single_user";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = make_single_user_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", type_items, SELECT, "Type", "");

	RNA_def_boolean(ot->srna, "object", 0, "Object", "Make single user objects");
	RNA_def_boolean(ot->srna, "obdata", 0, "Object Data", "Make single user object data");
	RNA_def_boolean(ot->srna, "material", 0, "Materials", "Make materials local to each datablock");
	RNA_def_boolean(ot->srna, "texture", 0, "Textures",
	                "Make textures local to each material (needs 'Materials' to be set too)");
	RNA_def_boolean(ot->srna, "animation", 0, "Object Animation", "Make animation data local to each object");
}

static int drop_named_material_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Base *base = ED_view3d_give_base_under_cursor(C, event->mval);
	Material *ma;
	char name[MAX_ID_NAME - 2];

	RNA_string_get(op->ptr, "name", name);
	ma = (Material *)BKE_libblock_find_name(ID_MA, name);
	if (base == NULL || ma == NULL)
		return OPERATOR_CANCELLED;

	assign_material(base->object, ma, 1, BKE_MAT_ASSIGN_USERPREF);

	DAG_id_tag_update(&base->object->id, OB_RECALC_OB);

	WM_event_add_notifier(C, NC_OBJECT | ND_OB_SHADING, base->object);
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	WM_event_add_notifier(C, NC_MATERIAL | ND_SHADING_LINKS, ma);

	return OPERATOR_FINISHED;
}

/* used for dropbox */
/* assigns to object under cursor, only first material slot */
void OBJECT_OT_drop_named_material(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Drop Named Material on Object";
	ot->description = "";
	ot->idname = "OBJECT_OT_drop_named_material";

	/* api callbacks */
	ot->invoke = drop_named_material_invoke;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

	/* properties */
	RNA_def_string(ot->srna, "name", "Material", MAX_ID_NAME - 2, "Name", "Material name to assign");
}

static int object_unlink_data_exec(bContext *C, wmOperator *op)
{
	ID *id;
	PropertyPointerRNA pprop;

	uiIDContextProperty(C, &pprop.ptr, &pprop.prop);

	if (pprop.prop == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Incorrect context for running object data unlink");
		return OPERATOR_CANCELLED;
	}

	id = pprop.ptr.id.data;

	if (GS(id->name) == ID_OB) {
		Object *ob = (Object *)id;
		if (ob->data) {
			ID *id_data = ob->data;

			if (GS(id_data->name) == ID_IM) {
				id_us_min(id_data);
				ob->data = NULL;
			}
			else {
				BKE_report(op->reports, RPT_ERROR, "Can't unlink this object data");
				return OPERATOR_CANCELLED;
			}
		}
	}

	RNA_property_update(C, &pprop.ptr, pprop.prop);

	return OPERATOR_FINISHED;
}

/**
 * \note Only for empty-image objects, this operator is needed
 */
void OBJECT_OT_unlink_data(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlink";
	ot->idname = "OBJECT_OT_unlink_data";
	ot->description = "";

	/* api callbacks */
	ot->exec = object_unlink_data_exec;

	/* flags */
	ot->flag = OPTYPE_INTERNAL;
}
