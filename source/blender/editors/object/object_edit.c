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

/** \file blender/editors/object/object_edit.c
 *  \ingroup edobj
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <float.h>
#include <ctype.h>
#include <stddef.h> //for offsetof

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_material_types.h"
#include "DNA_meta_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_meshdata_types.h"
#include "DNA_vfont_types.h"
#include "DNA_mesh_types.h"

#include "IMB_imbuf_types.h"

#include "BKE_anim.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_effect.h"
#include "BKE_depsgraph.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_property.h"
#include "BKE_sca.h"
#include "BKE_softbody.h"
#include "BKE_modifier.h"
#include "BKE_editmesh.h"
#include "BKE_report.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_mesh.h"
#include "ED_mball.h"
#include "ED_lattice.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_image.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

/* for menu/popup icons etc etc*/

#include "UI_interface.h"
#include "WM_api.h"
#include "WM_types.h"

#include "object_intern.h"  // own include

/* ************* XXX **************** */
static void error(const char *UNUSED(arg)) {}
static void waitcursor(int UNUSED(val)) {}
static int pupmenu(const char *UNUSED(msg)) { return 0; }

/* port over here */
static void error_libdata(void) {}

Object *ED_object_context(bContext *C)
{
	return CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
}

/* find the correct active object per context
 * note: context can be NULL when called from a enum with PROP_ENUM_NO_CONTEXT */
Object *ED_object_active_context(bContext *C)
{
	Object *ob = NULL;
	if (C) {
		ob = ED_object_context(C);
		if (!ob) ob = CTX_data_active_object(C);
	}
	return ob;
}


/* ********* clear/set restrict view *********/
static int object_hide_view_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;
	Scene *scene = CTX_data_scene(C);
	Base *base;
	bool changed = false;
	
	/* XXX need a context loop to handle such cases */
	for (base = FIRSTBASE; base; base = base->next) {
		if ((base->lay & v3d->lay) && base->object->restrictflag & OB_RESTRICT_VIEW) {
			base->flag |= SELECT;
			base->object->flag = base->flag;
			base->object->restrictflag &= ~OB_RESTRICT_VIEW; 
			changed = true;
		}
	}
	if (changed) {
		DAG_id_type_tag(bmain, ID_OB);
		DAG_relations_tag_update(bmain);
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_view_clear(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Clear Restrict View";
	ot->description = "Reveal the object by setting the hide flag";
	ot->idname = "OBJECT_OT_hide_view_clear";
	
	/* api callbacks */
	ot->exec = object_hide_view_clear_exec;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_hide_view_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	bool changed = false;
	const bool unselected = RNA_boolean_get(op->ptr, "unselected");
	
	CTX_DATA_BEGIN(C, Base *, base, visible_bases)
	{
		if (!unselected) {
			if (base->flag & SELECT) {
				base->flag &= ~SELECT;
				base->object->flag = base->flag;
				base->object->restrictflag |= OB_RESTRICT_VIEW;
				changed = true;
				if (base == BASACT) {
					ED_base_object_activate(C, NULL);
				}
			}
		}
		else {
			if (!(base->flag & SELECT)) {
				base->object->restrictflag |= OB_RESTRICT_VIEW;
				changed = true;
				if (base == BASACT) {
					ED_base_object_activate(C, NULL);
				}
			}
		}
	}
	CTX_DATA_END;

	if (changed) {
		DAG_id_type_tag(bmain, ID_OB);
		DAG_relations_tag_update(bmain);
		
		WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
		
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_view_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Restrict View";
	ot->description = "Hide the object by setting the hide flag";
	ot->idname = "OBJECT_OT_hide_view_set";
	
	/* api callbacks */
	ot->exec = object_hide_view_set_exec;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected objects");
	
}

/* 99% same as above except no need for scene refreshing (TODO, update render preview) */
static int object_hide_render_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	bool changed = false;

	/* XXX need a context loop to handle such cases */
	CTX_DATA_BEGIN(C, Object *, ob, selected_editable_objects)
	{
		if (ob->restrictflag & OB_RESTRICT_RENDER) {
			ob->restrictflag &= ~OB_RESTRICT_RENDER;
			changed = true;
		}
	}
	CTX_DATA_END;

	if (changed)
		WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_render_clear(wmOperatorType *ot)
{

	/* identifiers */
	ot->name = "Clear Restrict Render";
	ot->description = "Reveal the render object by setting the hide render flag";
	ot->idname = "OBJECT_OT_hide_render_clear";

	/* api callbacks */
	ot->exec = object_hide_render_clear_exec;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_hide_render_set_exec(bContext *C, wmOperator *op)
{
	const bool unselected = RNA_boolean_get(op->ptr, "unselected");

	CTX_DATA_BEGIN(C, Base *, base, visible_bases)
	{
		if (!unselected) {
			if (base->flag & SELECT) {
				base->object->restrictflag |= OB_RESTRICT_RENDER;
			}
		}
		else {
			if (!(base->flag & SELECT)) {
				base->object->restrictflag |= OB_RESTRICT_RENDER;
			}
		}
	}
	CTX_DATA_END;
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_OUTLINER, NULL);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_render_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set Restrict Render";
	ot->description = "Hide the render object by setting the hide render flag";
	ot->idname = "OBJECT_OT_hide_render_set";

	/* api callbacks */
	ot->exec = object_hide_render_set_exec;
	ot->poll = ED_operator_view3d_active;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected objects");
}

/* ******************* toggle editmode operator  ***************** */

/**
 * Load EditMode data back into the object,
 * optionally freeing the editmode data.
 */
static bool ED_object_editmode_load_ex(Object *obedit, const bool freedata)
{
	if (obedit == NULL) {
		return false;
	}

	if (obedit->type == OB_MESH) {
		Mesh *me = obedit->data;

		if (me->edit_btmesh->bm->totvert > MESH_MAX_VERTS) {
			error("Too many vertices");
			return false;
		}

		EDBM_mesh_load(obedit);

		if (freedata) {
			EDBM_mesh_free(me->edit_btmesh);
			MEM_freeN(me->edit_btmesh);
			me->edit_btmesh = NULL;
		}
		if (obedit->restore_mode & OB_MODE_WEIGHT_PAINT) {
			ED_mesh_mirror_spatial_table(NULL, NULL, NULL, 'e');
			ED_mesh_mirror_topo_table(NULL, 'e');
		}
	}
	else if (obedit->type == OB_ARMATURE) {
		ED_armature_from_edit(obedit->data);
		if (freedata)
			ED_armature_edit_free(obedit->data);
	}
	else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
		load_editNurb(obedit);
		if (freedata) free_editNurb(obedit);
	}
	else if (obedit->type == OB_FONT) {
		load_editText(obedit);
		if (freedata) free_editText(obedit);
	}
	else if (obedit->type == OB_LATTICE) {
		load_editLatt(obedit);
		if (freedata) free_editLatt(obedit);
	}
	else if (obedit->type == OB_MBALL) {
		load_editMball(obedit);
		if (freedata) free_editMball(obedit);
	}

	return true;
}

bool ED_object_editmode_load(Object *obedit)
{
	return ED_object_editmode_load_ex(obedit, false);
}

void ED_object_editmode_exit(bContext *C, int flag)
{
	/* Note! only in exceptional cases should 'EM_DO_UNDO' NOT be in the flag */
	/* Note! if 'EM_FREEDATA' isn't in the flag, use ED_object_editmode_load directly */
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	const bool freedata = (flag & EM_FREEDATA) != 0;

	if (flag & EM_WAITCURSOR) waitcursor(1);

	if (ED_object_editmode_load_ex(obedit, freedata) == false) {
		/* in rare cases (background mode) its possible active object
		 * is flagged for editmode, without 'obedit' being set [#35489] */
		if (UNLIKELY(scene->basact && (scene->basact->object->mode & OB_MODE_EDIT))) {
			scene->basact->object->mode &= ~OB_MODE_EDIT;
		}
		if (flag & EM_WAITCURSOR) waitcursor(0);
		return;
	}

	/* freedata only 0 now on file saves and render */
	if (freedata) {
		ListBase pidlist;
		PTCacheID *pid;

		/* for example; displist make is different in editmode */
		scene->obedit = NULL; // XXX for context

		/* flag object caches as outdated */
		BKE_ptcache_ids_from_object(&pidlist, obedit, scene, 0);
		for (pid = pidlist.first; pid; pid = pid->next) {
			if (pid->type != PTCACHE_TYPE_PARTICLES) /* particles don't need reset on geometry change */
				pid->cache->flag |= PTCACHE_OUTDATED;
		}
		BLI_freelistN(&pidlist);
		
		BKE_ptcache_object_reset(scene, obedit, PTCACHE_RESET_OUTDATED);

		/* also flush ob recalc, doesn't take much overhead, but used for particles */
		DAG_id_tag_update(&obedit->id, OB_RECALC_OB | OB_RECALC_DATA);
	
		if (flag & EM_DO_UNDO)
			ED_undo_push(C, "Editmode");

		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, scene);

		obedit->mode &= ~OB_MODE_EDIT;
	}

	if (flag & EM_WAITCURSOR) waitcursor(0);
}


void ED_object_editmode_enter(bContext *C, int flag)
{
	Scene *scene = CTX_data_scene(C);
	Base *base = NULL;
	Object *ob;
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = NULL;
	bool ok = false;

	if (scene->id.lib) return;

	if (sa && sa->spacetype == SPACE_VIEW3D)
		v3d = sa->spacedata.first;

	if ((flag & EM_IGNORE_LAYER) == 0) {
		base = CTX_data_active_base(C); /* active layer checked here for view3d */

		if (base == NULL) return;
		else if (v3d && (base->lay & v3d->lay) == 0) return;
		else if (!v3d && (base->lay & scene->lay) == 0) return;
	}
	else {
		base = scene->basact;
	}

	if (ELEM(NULL, base, base->object, base->object->data)) return;

	ob = base->object;

	/* this checks actual object->data, for cases when other scenes have it in editmode context */
	if (BKE_object_is_in_editmode(ob))
		return;
	
	if (BKE_object_obdata_is_libdata(ob)) {
		error_libdata();
		return;
	}

	if (flag & EM_WAITCURSOR) waitcursor(1);

	ob->restore_mode = ob->mode;

	/* note, when switching scenes the object can have editmode data but
	 * not be scene->obedit: bug 22954, this avoids calling self eternally */
	if ((ob->restore_mode & OB_MODE_EDIT) == 0)
		ED_object_toggle_modes(C, ob->mode);

	ob->mode = OB_MODE_EDIT;

	if (ob->type == OB_MESH) {
		BMEditMesh *em;
		ok = 1;
		scene->obedit = ob;  /* context sees this */

		EDBM_mesh_make(scene->toolsettings, ob);

		em = BKE_editmesh_from_object(ob);
		if (LIKELY(em)) {
			/* order doesn't matter */
			EDBM_mesh_normals_update(em);
			BKE_editmesh_tessface_calc(em);

			BM_mesh_select_mode_flush(em->bm);
		}

		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_EDITMODE_MESH, scene);
	}
	else if (ob->type == OB_ARMATURE) {
		bArmature *arm = ob->data;
		if (!arm) return;
		/*
		 * The function BKE_object_obdata_is_libdata make a problem here, the
		 * check for ob->proxy return 0 and let blender enter to edit mode
		 * this causes a crash when you try leave the edit mode.
		 * The problem is that i can't remove the ob->proxy check from
		 * BKE_object_obdata_is_libdata that prevent the bugfix #6614, so
		 * i add this little hack here.
		 */
		if (arm->id.lib) {
			error_libdata();
			return;
		}
		ok = 1;
		scene->obedit = ob;
		ED_armature_to_edit(arm);
		/* to ensure all goes in restposition and without striding */
		DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME); /* XXX: should this be OB_RECALC_DATA? */

		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_EDITMODE_ARMATURE, scene);
	}
	else if (ob->type == OB_FONT) {
		scene->obedit = ob; /* XXX for context */
		ok = 1;
		make_editText(ob);

		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_EDITMODE_TEXT, scene);
	}
	else if (ob->type == OB_MBALL) {
		scene->obedit = ob; /* XXX for context */
		ok = 1;
		make_editMball(ob);

		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_EDITMODE_MBALL, scene);
	}
	else if (ob->type == OB_LATTICE) {
		scene->obedit = ob; /* XXX for context */
		ok = 1;
		make_editLatt(ob);

		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_EDITMODE_LATTICE, scene);
	}
	else if (ob->type == OB_SURF || ob->type == OB_CURVE) {
		ok = 1;
		scene->obedit = ob; /* XXX for context */
		make_editNurb(ob);

		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_EDITMODE_CURVE, scene);
	}

	if (ok) {
		DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
	}
	else {
		scene->obedit = NULL; /* XXX for context */
		ob->mode &= ~OB_MODE_EDIT;
		WM_event_add_notifier(C, NC_SCENE | ND_MODE | NS_MODE_OBJECT, scene);
	}

	if (flag & EM_DO_UNDO) ED_undo_push(C, "Enter Editmode");
	if (flag & EM_WAITCURSOR) waitcursor(0);
}

static int editmode_toggle_exec(bContext *C, wmOperator *op)
{
	const int mode_flag = OB_MODE_EDIT;
	const bool is_mode_set = (CTX_data_edit_object(C) != NULL);
	ToolSettings *toolsettings =  CTX_data_tool_settings(C);

	if (!is_mode_set) {
		Scene *scene = CTX_data_scene(C);
		if (!ED_object_mode_compat_set(C, scene->basact->object, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	if (!is_mode_set)
		ED_object_editmode_enter(C, EM_WAITCURSOR);
	else
		ED_object_editmode_exit(C, EM_FREEDATA | EM_FREEUNDO | EM_WAITCURSOR);  /* had EM_DO_UNDO but op flag calls undo too [#24685] */
	
	ED_space_image_uv_sculpt_update(CTX_wm_manager(C), toolsettings);

	return OPERATOR_FINISHED;
}

static int editmode_toggle_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	/* covers proxies too */
	if (ELEM(NULL, ob, ob->data) || ((ID *)ob->data)->lib)
		return 0;

	/* if hidden but in edit mode, we still display */
	if ((ob->restrictflag & OB_RESTRICT_VIEW) && !(ob->mode & OB_MODE_EDIT))
		return 0;

	return (ELEM(ob->type, OB_MESH, OB_ARMATURE, OB_FONT, OB_MBALL, OB_LATTICE, OB_SURF, OB_CURVE));
}

void OBJECT_OT_editmode_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Toggle Editmode";
	ot->description = "Toggle object's editmode";
	ot->idname = "OBJECT_OT_editmode_toggle";
	
	/* api callbacks */
	ot->exec = editmode_toggle_exec;
	ot->poll = editmode_toggle_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* *************************** */

static int posemode_exec(bContext *C, wmOperator *op)
{
	Base *base = CTX_data_active_base(C);
	Object *ob = base->object;
	const int mode_flag = OB_MODE_POSE;
	const bool is_mode_set = (ob->mode & mode_flag) != 0;
	
	if (!is_mode_set) {
		if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
			return OPERATOR_CANCELLED;
		}
	}

	if (ob->type == OB_ARMATURE) {
		if (ob == CTX_data_edit_object(C)) {
			ED_object_editmode_exit(C, EM_FREEDATA | EM_DO_UNDO);
			ED_armature_enter_posemode(C, base);
		}
		else if (is_mode_set)
			ED_armature_exit_posemode(C, base);
		else
			ED_armature_enter_posemode(C, base);
		
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_PASS_THROUGH;
}

void OBJECT_OT_posemode_toggle(wmOperatorType *ot) 
{
	/* identifiers */
	ot->name = "Toggle Pose Mode";
	ot->idname = "OBJECT_OT_posemode_toggle";
	ot->description = "Enable or disable posing/selecting bones";
	
	/* api callbacks */
	ot->exec = posemode_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flag */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static void copymenu_properties(Scene *scene, View3D *v3d, Object *ob)
{	
//XXX no longer used - to be removed - replaced by game_properties_copy_exec
	bProperty *prop;
	Base *base;
	int nr, tot = 0;
	char *str;
	
	prop = ob->prop.first;
	while (prop) {
		tot++;
		prop = prop->next;
	}
	
	str = MEM_callocN(50 + 33 * tot, "copymenu prop");
	
	if (tot)
		strcpy(str, "Copy Property %t|Replace All|Merge All|%l");
	else
		strcpy(str, "Copy Property %t|Clear All (no properties on active)");
	
	tot = 0;
	prop = ob->prop.first;
	while (prop) {
		tot++;
		strcat(str, "|");
		strcat(str, prop->name);
		prop = prop->next;
	}

	nr = pupmenu(str);
	
	if (nr == 1 || nr == 2) {
		for (base = FIRSTBASE; base; base = base->next) {
			if ((base != BASACT) && (TESTBASELIB(v3d, base))) {
				if (nr == 1) { /* replace */
					BKE_bproperty_copy_list(&base->object->prop, &ob->prop);
				}
				else {
					for (prop = ob->prop.first; prop; prop = prop->next) {
						BKE_bproperty_object_set(base->object, prop);
					}
				}
			}
		}
	}
	else if (nr > 0) {
		prop = BLI_findlink(&ob->prop, nr - 4); /* account for first 3 menu items & menu index starting at 1*/
		
		if (prop) {
			for (base = FIRSTBASE; base; base = base->next) {
				if ((base != BASACT) && (TESTBASELIB(v3d, base))) {
					BKE_bproperty_object_set(base->object, prop);
				}
			}
		}
	}
	MEM_freeN(str);
	
}

static void copymenu_logicbricks(Scene *scene, View3D *v3d, Object *ob)
{
//XXX no longer used - to be removed - replaced by logicbricks_copy_exec
	Base *base;
	
	for (base = FIRSTBASE; base; base = base->next) {
		if (base->object != ob) {
			if (TESTBASELIB(v3d, base)) {
				
				/* first: free all logic */
				free_sensors(&base->object->sensors);
				unlink_controllers(&base->object->controllers);
				free_controllers(&base->object->controllers);
				unlink_actuators(&base->object->actuators);
				free_actuators(&base->object->actuators);
				
				/* now copy it, this also works without logicbricks! */
				clear_sca_new_poins_ob(ob);
				copy_sensors(&base->object->sensors, &ob->sensors);
				copy_controllers(&base->object->controllers, &ob->controllers);
				copy_actuators(&base->object->actuators, &ob->actuators);
				set_sca_new_poins_ob(base->object);
				
				/* some menu settings */
				base->object->scavisflag = ob->scavisflag;
				base->object->scaflag = ob->scaflag;
				
				/* set the initial state */
				base->object->state = ob->state;
				base->object->init_state = ob->init_state;
			}
		}
	}
}

/* both pointers should exist */
static void copy_texture_space(Object *to, Object *ob)
{
	float *poin1 = NULL, *poin2 = NULL;
	short texflag = 0;
	
	if (ob->type == OB_MESH) {
		texflag = ((Mesh *)ob->data)->texflag;
		poin2 = ((Mesh *)ob->data)->loc;
	}
	else if (ELEM(ob->type, OB_CURVE, OB_SURF, OB_FONT)) {
		texflag = ((Curve *)ob->data)->texflag;
		poin2 = ((Curve *)ob->data)->loc;
	}
	else if (ob->type == OB_MBALL) {
		texflag = ((MetaBall *)ob->data)->texflag;
		poin2 = ((MetaBall *)ob->data)->loc;
	}
	else
		return;
		
	if (to->type == OB_MESH) {
		((Mesh *)to->data)->texflag = texflag;
		poin1 = ((Mesh *)to->data)->loc;
	}
	else if (ELEM(to->type, OB_CURVE, OB_SURF, OB_FONT)) {
		((Curve *)to->data)->texflag = texflag;
		poin1 = ((Curve *)to->data)->loc;
	}
	else if (to->type == OB_MBALL) {
		((MetaBall *)to->data)->texflag = texflag;
		poin1 = ((MetaBall *)to->data)->loc;
	}
	else
		return;
	
	memcpy(poin1, poin2, 9 * sizeof(float));  /* this was noted in DNA_mesh, curve, mball */
	
	if (to->type == OB_MESH) {
		/* pass */
	}
	else if (to->type == OB_MBALL) {
		BKE_mball_texspace_calc(to);
	}
	else {
		BKE_curve_texspace_calc(to->data);
	}
	
}

/* UNUSED, keep in case we want to copy functionality for use elsewhere */
static void copy_attr(Main *bmain, Scene *scene, View3D *v3d, short event)
{
	Object *ob;
	Base *base;
	Curve *cu, *cu1;
	Nurb *nu;
	bool do_depgraph_update = false;
	
	if (scene->id.lib) return;

	if (!(ob = OBACT)) return;
	
	if (scene->obedit) { // XXX get from context
		/* obedit_copymenu(); */
		return;
	}
	if (event == 9) {
		copymenu_properties(scene, v3d, ob);
		return;
	}
	else if (event == 10) {
		copymenu_logicbricks(scene, v3d, ob);
		return;
	}
	else if (event == 24) {
		/* moved to BKE_object_link_modifiers */
		/* copymenu_modifiers(bmain, scene, v3d, ob); */
		return;
	}

	for (base = FIRSTBASE; base; base = base->next) {
		if (base != BASACT) {
			if (TESTBASELIB(v3d, base)) {
				DAG_id_tag_update(&base->object->id, OB_RECALC_DATA);
				
				if (event == 1) {  /* loc */
					copy_v3_v3(base->object->loc, ob->loc);
					copy_v3_v3(base->object->dloc, ob->dloc);
				}
				else if (event == 2) {  /* rot */
					copy_v3_v3(base->object->rot, ob->rot);
					copy_v3_v3(base->object->drot, ob->drot);

					copy_qt_qt(base->object->quat, ob->quat);
					copy_qt_qt(base->object->dquat, ob->dquat);
				}
				else if (event == 3) {  /* size */
					copy_v3_v3(base->object->size, ob->size);
					copy_v3_v3(base->object->dscale, ob->dscale);
				}
				else if (event == 4) {  /* drawtype */
					base->object->dt = ob->dt;
					base->object->dtx = ob->dtx;
					base->object->empty_drawtype = ob->empty_drawtype;
					base->object->empty_drawsize = ob->empty_drawsize;
				}
				else if (event == 5) {  /* time offs */
					base->object->sf = ob->sf;
				}
				else if (event == 6) {  /* dupli */
					base->object->dupon = ob->dupon;
					base->object->dupoff = ob->dupoff;
					base->object->dupsta = ob->dupsta;
					base->object->dupend = ob->dupend;
					
					base->object->transflag &= ~OB_DUPLI;
					base->object->transflag |= (ob->transflag & OB_DUPLI);

					base->object->dup_group = ob->dup_group;
					if (ob->dup_group)
						id_lib_extern(&ob->dup_group->id);
				}
				else if (event == 7) {    /* mass */
					base->object->mass = ob->mass;
				}
				else if (event == 8) {    /* damping */
					base->object->damping = ob->damping;
					base->object->rdamping = ob->rdamping;
				}
				else if (event == 11) {   /* all physical attributes */
					base->object->gameflag = ob->gameflag;
					base->object->inertia = ob->inertia;
					base->object->formfactor = ob->formfactor;
					base->object->damping = ob->damping;
					base->object->rdamping = ob->rdamping;
					base->object->min_vel = ob->min_vel;
					base->object->max_vel = ob->max_vel;
					if (ob->gameflag & OB_BOUNDS) {
						base->object->collision_boundtype = ob->collision_boundtype;
					}
					base->object->margin = ob->margin;
					base->object->bsoft = copy_bulletsoftbody(ob->bsoft);

				}
				else if (event == 17) {   /* tex space */
					copy_texture_space(base->object, ob);
				}
				else if (event == 18) {   /* font settings */
					
					if (base->object->type == ob->type) {
						cu = ob->data;
						cu1 = base->object->data;

						cu1->spacemode = cu->spacemode;
						cu1->spacing = cu->spacing;
						cu1->linedist = cu->linedist;
						cu1->shear = cu->shear;
						cu1->fsize = cu->fsize;
						cu1->xof = cu->xof;
						cu1->yof = cu->yof;
						cu1->textoncurve = cu->textoncurve;
						cu1->wordspace = cu->wordspace;
						cu1->ulpos = cu->ulpos;
						cu1->ulheight = cu->ulheight;
						if (cu1->vfont) cu1->vfont->id.us--;
						cu1->vfont = cu->vfont;
						id_us_plus((ID *)cu1->vfont);
						if (cu1->vfontb) cu1->vfontb->id.us--;
						cu1->vfontb = cu->vfontb;
						id_us_plus((ID *)cu1->vfontb);
						if (cu1->vfonti) cu1->vfonti->id.us--;
						cu1->vfonti = cu->vfonti;
						id_us_plus((ID *)cu1->vfonti);
						if (cu1->vfontbi) cu1->vfontbi->id.us--;
						cu1->vfontbi = cu->vfontbi;
						id_us_plus((ID *)cu1->vfontbi);
						
						BLI_strncpy(cu1->family, cu->family, sizeof(cu1->family));
						
						DAG_id_tag_update(&base->object->id, OB_RECALC_DATA);
					}
				}
				else if (event == 19) {   /* bevel settings */
					
					if (ELEM(base->object->type, OB_CURVE, OB_FONT)) {
						cu = ob->data;
						cu1 = base->object->data;
						
						cu1->bevobj = cu->bevobj;
						cu1->taperobj = cu->taperobj;
						cu1->width = cu->width;
						cu1->bevresol = cu->bevresol;
						cu1->ext1 = cu->ext1;
						cu1->ext2 = cu->ext2;
						
						DAG_id_tag_update(&base->object->id, OB_RECALC_DATA);
					}
				}
				else if (event == 25) {   /* curve resolution */

					if (ELEM(base->object->type, OB_CURVE, OB_FONT)) {
						cu = ob->data;
						cu1 = base->object->data;
						
						cu1->resolu = cu->resolu;
						cu1->resolu_ren = cu->resolu_ren;
						
						nu = cu1->nurb.first;
						
						while (nu) {
							nu->resolu = cu1->resolu;
							nu = nu->next;
						}
						
						DAG_id_tag_update(&base->object->id, OB_RECALC_DATA);
					}
				}
				else if (event == 21) {
					if (base->object->type == OB_MESH) {
						ModifierData *md = modifiers_findByType(ob, eModifierType_Subsurf);

						if (md) {
							ModifierData *tmd = modifiers_findByType(base->object, eModifierType_Subsurf);

							if (!tmd) {
								tmd = modifier_new(eModifierType_Subsurf);
								BLI_addtail(&base->object->modifiers, tmd);
							}

							modifier_copyData(md, tmd);
							DAG_id_tag_update(&base->object->id, OB_RECALC_DATA);
						}
					}
				}
				else if (event == 22) {
					/* Copy the constraint channels over */
					BKE_constraints_copy(&base->object->constraints, &ob->constraints, true);
					
					do_depgraph_update = true;
				}
				else if (event == 23) {
					base->object->softflag = ob->softflag;
					if (base->object->soft) sbFree(base->object->soft);
					
					base->object->soft = copy_softbody(ob->soft, false);

					if (!modifiers_findByType(base->object, eModifierType_Softbody)) {
						BLI_addhead(&base->object->modifiers, modifier_new(eModifierType_Softbody));
					}
				}
				else if (event == 26) {
#if 0 // XXX old animation system
					copy_nlastrips(&base->object->nlastrips, &ob->nlastrips);
#endif // XXX old animation system
				}
				else if (event == 27) {   /* autosmooth */
					if (base->object->type == OB_MESH) {
						Mesh *me = ob->data;
						Mesh *cme = base->object->data;
						cme->smoothresh = me->smoothresh;
						if (me->flag & ME_AUTOSMOOTH)
							cme->flag |= ME_AUTOSMOOTH;
						else
							cme->flag &= ~ME_AUTOSMOOTH;
					}
				}
				else if (event == 28) { /* UV orco */
					if (ELEM(base->object->type, OB_CURVE, OB_SURF)) {
						cu = ob->data;
						cu1 = base->object->data;
						
						if (cu->flag & CU_UV_ORCO)
							cu1->flag |= CU_UV_ORCO;
						else
							cu1->flag &= ~CU_UV_ORCO;
					}
				}
				else if (event == 29) { /* protected bits */
					base->object->protectflag = ob->protectflag;
				}
				else if (event == 30) { /* index object */
					base->object->index = ob->index;
				}
				else if (event == 31) { /* object color */
					copy_v4_v4(base->object->col, ob->col);
				}
			}
		}
	}
	
	if (do_depgraph_update)
		DAG_relations_tag_update(bmain);
}

static void UNUSED_FUNCTION(copy_attr_menu) (Main *bmain, Scene *scene, View3D *v3d)
{
	Object *ob;
	short event;
	char str[512];
	
	if (!(ob = OBACT)) return;
	
	if (scene->obedit) { /* XXX get from context */
/*		if (ob->type == OB_MESH) */
/* XXX			mesh_copy_menu(); */
		return;
	}
	
	/* Object Mode */
	
	/* If you change this menu, don't forget to update the menu in header_view3d.c
	 * view3d_edit_object_copyattrmenu() and in toolbox.c
	 */
	
	strcpy(str,
	       "Copy Attributes %t|Location %x1|Rotation %x2|Size %x3|Draw Options %x4|"
	       "Time Offset %x5|Dupli %x6|Object Color %x31|%l|Mass %x7|Damping %x8|All Physical Attributes %x11|Properties %x9|"
	       "Logic Bricks %x10|Protected Transform %x29|%l");
	
	strcat(str, "|Object Constraints %x22");
	strcat(str, "|NLA Strips %x26");
	
/* XXX	if (OB_TYPE_SUPPORT_MATERIAL(ob->type)) { */
/*		strcat(str, "|Texture Space %x17"); */
/*	} */
	
	if (ob->type == OB_FONT) strcat(str, "|Font Settings %x18|Bevel Settings %x19");
	if (ob->type == OB_CURVE) strcat(str, "|Bevel Settings %x19|UV Orco %x28");
	
	if ((ob->type == OB_FONT) || (ob->type == OB_CURVE)) {
		strcat(str, "|Curve Resolution %x25");
	}

	if (ob->type == OB_MESH) {
		strcat(str, "|Subsurf Settings %x21|AutoSmooth %x27");
	}

	if (ob->soft) strcat(str, "|Soft Body Settings %x23");
	
	strcat(str, "|Pass Index %x30");
	
	if (ob->type == OB_MESH || ob->type == OB_CURVE || ob->type == OB_LATTICE || ob->type == OB_SURF) {
		strcat(str, "|Modifiers ... %x24");
	}

	event = pupmenu(str);
	if (event <= 0) return;
	
	copy_attr(bmain, scene, v3d, event);
}

/* ******************* force field toggle operator ***************** */

void ED_object_check_force_modifiers(Main *bmain, Scene *scene, Object *object)
{
	PartDeflect *pd = object->pd;
	ModifierData *md = modifiers_findByType(object, eModifierType_Surface);

	/* add/remove modifier as needed */
	if (!md) {
		if (pd && (pd->shape == PFIELD_SHAPE_SURFACE) && ELEM(pd->forcefield, PFIELD_GUIDE, PFIELD_TEXTURE) == 0)
			if (ELEM(object->type, OB_MESH, OB_SURF, OB_FONT, OB_CURVE))
				ED_object_modifier_add(NULL, bmain, scene, object, NULL, eModifierType_Surface);
	}
	else {
		if (!pd || pd->shape != PFIELD_SHAPE_SURFACE || pd->forcefield != PFIELD_FORCE)
			ED_object_modifier_remove(NULL, bmain, object, md);
	}
}

static int forcefield_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_active_object(C);

	if (ob->pd == NULL)
		ob->pd = object_add_collision_fields(PFIELD_FORCE);
	else if (ob->pd->forcefield == 0)
		ob->pd->forcefield = PFIELD_FORCE;
	else
		ob->pd->forcefield = 0;
	
	ED_object_check_force_modifiers(CTX_data_main(C), CTX_data_scene(C), ob);
	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
	WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_forcefield_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name = "Toggle Force Field";
	ot->description = "Toggle object's force field";
	ot->idname = "OBJECT_OT_forcefield_toggle";
	
	/* api callbacks */
	ot->exec = forcefield_toggle_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************************** */
/* Motion Paths */

/* For the objects with animation: update paths for those that have got them
 * This should selectively update paths that exist...
 *
 * To be called from various tools that do incremental updates 
 */
void ED_objects_recalculate_paths(bContext *C, Scene *scene)
{
	ListBase targets = {NULL, NULL};
	
	/* loop over objects in scene */
	CTX_DATA_BEGIN(C, Object *, ob, selected_editable_objects)
	{
		/* set flag to force recalc, then grab path(s) from object */
		ob->avs.recalc |= ANIMVIZ_RECALC_PATHS;
		animviz_get_object_motionpaths(ob, &targets);
	}
	CTX_DATA_END;
	
	/* recalculate paths, then free */
	animviz_calc_motionpaths(scene, &targets);
	BLI_freelistN(&targets);
}


/* show popup to determine settings */
static int object_calculate_paths_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	Object *ob = CTX_data_active_object(C);
	
	if (ob == NULL)
		return OPERATOR_CANCELLED;
	
	/* set default settings from existing/stored settings */
	{
		bAnimVizSettings *avs = &ob->avs;
		
		RNA_int_set(op->ptr, "start_frame", avs->path_sf);
		RNA_int_set(op->ptr, "end_frame", avs->path_ef);
	}
	
	/* show popup dialog to allow editing of range... */
	/* FIXME: hardcoded dimensions here are just arbitrary */
	return WM_operator_props_dialog_popup(C, op, 10 * UI_UNIT_X, 10 * UI_UNIT_Y);
}

/* Calculate/recalculate whole paths (avs.path_sf to avs.path_ef) */
static int object_calculate_paths_exec(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	int start = RNA_int_get(op->ptr, "start_frame");
	int end = RNA_int_get(op->ptr, "end_frame");
	
	/* set up path data for bones being calculated */
	CTX_DATA_BEGIN(C, Object *, ob, selected_editable_objects)
	{
		bAnimVizSettings *avs = &ob->avs;
		
		/* grab baking settings from operator settings */
		avs->path_sf = start;
		avs->path_ef = end;
		
		/* verify that the selected object has the appropriate settings */
		animviz_verify_motionpaths(op->reports, scene, ob, NULL);
	}
	CTX_DATA_END;
	
	/* calculate the paths for objects that have them (and are tagged to get refreshed) */
	ED_objects_recalculate_paths(C, scene);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED; 
}

void OBJECT_OT_paths_calculate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Calculate Object Paths";
	ot->idname = "OBJECT_OT_paths_calculate";
	ot->description = "Calculate motion paths for the selected objects";
	
	/* api callbacks */
	ot->invoke = object_calculate_paths_invoke;
	ot->exec = object_calculate_paths_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "start_frame", 1, MINAFRAME, MAXFRAME, "Start", 
	            "First frame to calculate object paths on", MINFRAME, MAXFRAME / 2.0);
	RNA_def_int(ot->srna, "end_frame", 250, MINAFRAME, MAXFRAME, "End", 
	            "Last frame to calculate object paths on", MINFRAME, MAXFRAME / 2.0);
}

/* --------- */

static int object_update_paths_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	
	if (scene == NULL)
		return OPERATOR_CANCELLED;
		
	/* calculate the paths for objects that have them (and are tagged to get refreshed) */
	ED_objects_recalculate_paths(C, scene);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_paths_update(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Update Object Paths";
	ot->idname = "OBJECT_OT_paths_update";
	ot->description = "Recalculate paths for selected objects";
	
	/* api callbakcs */
	ot->exec = object_update_paths_exec;
	ot->poll = ED_operator_object_active_editable; /* TODO: this should probably check for existing paths */
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* --------- */

/* Clear motion paths for selected objects only */
void ED_objects_clear_paths(bContext *C)
{
	/* loop over objects in scene */
	CTX_DATA_BEGIN(C, Object *, ob, selected_editable_objects)
	{
		if (ob->mpath) {
			animviz_free_motionpath(ob->mpath);
			ob->mpath = NULL;
			ob->avs.path_bakeflag &= ~MOTIONPATH_BAKE_HAS_PATHS;
		}
	}
	CTX_DATA_END;
}

/* operator callback for this */
static int object_clear_paths_exec(bContext *C, wmOperator *UNUSED(op))
{	
	/* use the backend function for this */
	ED_objects_clear_paths(C);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED; 
}

void OBJECT_OT_paths_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Object Paths";
	ot->idname = "OBJECT_OT_paths_clear";
	ot->description = "Clear path caches for selected objects";
	
	/* api callbacks */
	ot->exec = object_clear_paths_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


/********************** Smooth/Flat *********************/

static int shade_smooth_exec(bContext *C, wmOperator *op)
{
	ID *data;
	Curve *cu;
	Nurb *nu;
	int clear = (strcmp(op->idname, "OBJECT_OT_shade_flat") == 0);
	bool done = false, linked_data = false;

	CTX_DATA_BEGIN(C, Object *, ob, selected_editable_objects)
	{
		data = ob->data;

		if (data && data->lib) {
			linked_data = true;
			continue;
		}

		if (ob->type == OB_MESH) {
			BKE_mesh_smooth_flag_set(ob, !clear);

			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

			done = true;
		}
		else if (ELEM(ob->type, OB_SURF, OB_CURVE)) {
			cu = ob->data;

			for (nu = cu->nurb.first; nu; nu = nu->next) {
				if (!clear) nu->flag |= ME_SMOOTH;
				else nu->flag &= ~ME_SMOOTH;
			}

			DAG_id_tag_update(&ob->id, OB_RECALC_DATA);
			WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

			done = true;
		}
	}
	CTX_DATA_END;

	if (linked_data)
		BKE_report(op->reports, RPT_WARNING, "Can't edit linked mesh or curve data");

	return (done) ? OPERATOR_FINISHED : OPERATOR_CANCELLED;
}

static int shade_poll(bContext *C)
{
	return (CTX_data_edit_object(C) == NULL);
}

void OBJECT_OT_shade_flat(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shade Flat";
	ot->description = "Render and display faces uniform, using Face Normals";
	ot->idname = "OBJECT_OT_shade_flat";
	
	/* api callbacks */
	ot->poll = shade_poll;
	ot->exec = shade_smooth_exec;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void OBJECT_OT_shade_smooth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Shade Smooth";
	ot->description = "Render and display faces smooth, using interpolated Vertex Normals";
	ot->idname = "OBJECT_OT_shade_smooth";
	
	/* api callbacks */
	ot->poll = shade_poll;
	ot->exec = shade_smooth_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************** */

static void UNUSED_FUNCTION(image_aspect) (Scene *scene, View3D *v3d)
{
	/* all selected objects with an image map: scale in image aspect */
	Base *base;
	Object *ob;
	Material *ma;
	Tex *tex;
	float x, y, space;
	int a, b, done;
	
	if (scene->obedit) return;  // XXX get from context
	if (scene->id.lib) return;
	
	for (base = FIRSTBASE; base; base = base->next) {
		if (TESTBASELIB(v3d, base)) {
			ob = base->object;
			done = false;
			
			for (a = 1; a <= ob->totcol; a++) {
				ma = give_current_material(ob, a);
				if (ma) {
					for (b = 0; b < MAX_MTEX; b++) {
						if (ma->mtex[b] && ma->mtex[b]->tex) {
							tex = ma->mtex[b]->tex;
							if (tex->type == TEX_IMAGE && tex->ima) {
								ImBuf *ibuf = BKE_image_acquire_ibuf(tex->ima, NULL, NULL);
								
								/* texturespace */
								space = 1.0;
								if (ob->type == OB_MESH) {
									float size[3];
									BKE_mesh_texspace_get(ob->data, NULL, NULL, size);
									space = size[0] / size[1];
								}
								else if (ELEM(ob->type, OB_CURVE, OB_FONT, OB_SURF)) {
									float size[3];
									BKE_curve_texspace_get(ob->data, NULL, NULL, size);
									space = size[0] / size[1];
								}
							
								x = ibuf->x / space;
								y = ibuf->y;
								
								if (x > y) ob->size[0] = ob->size[1] * x / y;
								else ob->size[1] = ob->size[0] * y / x;
								
								done = true;
								DAG_id_tag_update(&ob->id, OB_RECALC_OB);

								BKE_image_release_ibuf(tex->ima, ibuf, NULL);
							}
						}
						if (done) break;
					}
				}
				if (done) break;
			}
		}
	}
	
}

static EnumPropertyItem *object_mode_set_itemsf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
	EnumPropertyItem *input = object_mode_items;
	EnumPropertyItem *item = NULL;
	Object *ob;
	int totitem = 0;

	if (!C) /* needed for docs */
		return object_mode_items;

	ob = CTX_data_active_object(C);
	if (ob) {
		const bool use_mode_particle_edit = (BLI_listbase_is_empty(&ob->particlesystem) == false) ||
		                                    (ob->soft != NULL) ||
		                                    (modifiers_findByType(ob, eModifierType_Cloth) != NULL);
		while (input->identifier) {
			if ((input->value == OB_MODE_EDIT && OB_TYPE_SUPPORT_EDITMODE(ob->type)) ||
			    (input->value == OB_MODE_POSE && (ob->type == OB_ARMATURE)) ||
			    (input->value == OB_MODE_PARTICLE_EDIT && use_mode_particle_edit) ||
			    (ELEM(input->value, OB_MODE_SCULPT, OB_MODE_VERTEX_PAINT,
			           OB_MODE_WEIGHT_PAINT, OB_MODE_TEXTURE_PAINT) && (ob->type == OB_MESH)) ||
			    (input->value == OB_MODE_OBJECT))
			{
				RNA_enum_item_add(&item, &totitem, input);
			}
			input++;
		}
	}
	else {
		/* We need at least this one! */
		RNA_enum_items_add_value(&item, &totitem, input, OB_MODE_OBJECT);
	}

	RNA_enum_item_end(&item, &totitem);

	*r_free = true;

	return item;
}

static const char *object_mode_op_string(int mode)
{
	if (mode & OB_MODE_EDIT)
		return "OBJECT_OT_editmode_toggle";
	if (mode == OB_MODE_SCULPT)
		return "SCULPT_OT_sculptmode_toggle";
	if (mode == OB_MODE_VERTEX_PAINT)
		return "PAINT_OT_vertex_paint_toggle";
	if (mode == OB_MODE_WEIGHT_PAINT)
		return "PAINT_OT_weight_paint_toggle";
	if (mode == OB_MODE_TEXTURE_PAINT)
		return "PAINT_OT_texture_paint_toggle";
	if (mode == OB_MODE_PARTICLE_EDIT)
		return "PARTICLE_OT_particle_edit_toggle";
	if (mode == OB_MODE_POSE)
		return "OBJECT_OT_posemode_toggle";
	return NULL;
}

/* checks the mode to be set is compatible with the object
 * should be made into a generic function
 */
static bool object_mode_compat_test(Object *ob, ObjectMode mode)
{
	if (ob) {
		if (mode == OB_MODE_OBJECT)
			return true;

		switch (ob->type) {
			case OB_MESH:
				if (mode & (OB_MODE_EDIT | OB_MODE_SCULPT | OB_MODE_VERTEX_PAINT | OB_MODE_WEIGHT_PAINT |
				            OB_MODE_TEXTURE_PAINT | OB_MODE_PARTICLE_EDIT))
				{
					return true;
				}
				break;
			case OB_CURVE:
			case OB_SURF:
			case OB_FONT:
			case OB_MBALL:
				if (mode & (OB_MODE_EDIT))
					return true;
				break;
			case OB_LATTICE:
				if (mode & (OB_MODE_EDIT | OB_MODE_WEIGHT_PAINT))
					return true;
				break;
			case OB_ARMATURE:
				if (mode & (OB_MODE_EDIT | OB_MODE_POSE))
					return true;
				break;
		}
	}

	return false;
}

/**
 * Sets the mode to a compatible state (use before entering the mode).
 *
 * This is so each mode's exec function can call
 */
bool ED_object_mode_compat_set(bContext *C, Object *ob, int mode, ReportList *reports)
{
	bool ok;
	if (!ELEM(ob->mode, mode, OB_MODE_OBJECT)) {
		const char *opstring = object_mode_op_string(ob->mode);
		WM_operator_name_call(C, opstring, WM_OP_EXEC_REGION_WIN, NULL);
		ok = ELEM(ob->mode, mode, OB_MODE_OBJECT);
		if (!ok) {
			wmOperatorType *ot = WM_operatortype_find(opstring, false);
			BKE_reportf(reports, RPT_ERROR, "Unable to execute '%s', error changing modes", ot->name);
		}
	}
	else {
		ok = true;
	}

	return ok;
}

static int object_mode_set_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	ObjectMode mode = RNA_enum_get(op->ptr, "mode");
	ObjectMode restore_mode = (ob) ? ob->mode : OB_MODE_OBJECT;
	const bool toggle = RNA_boolean_get(op->ptr, "toggle");

	if (!ob || !object_mode_compat_test(ob, mode))
		return OPERATOR_PASS_THROUGH;

	if (ob->mode != mode) {
		/* we should be able to remove this call, each operator calls  */
		ED_object_mode_compat_set(C, ob, mode, op->reports);
	}

	/* Exit current mode if it's not the mode we're setting */
	if (mode != OB_MODE_OBJECT && (ob->mode != mode || toggle)) {
		/* Enter new mode */
		ED_object_toggle_modes(C, mode);
	}

	if (toggle) {
		/* Special case for Object mode! */
		if (mode == OB_MODE_OBJECT && restore_mode == OB_MODE_OBJECT && ob->restore_mode != OB_MODE_OBJECT) {
			ED_object_toggle_modes(C, ob->restore_mode);
		}
		else if (ob->mode == mode) {
			/* For toggling, store old mode so we know what to go back to */
			ob->restore_mode = restore_mode;
		}
		else if (ob->restore_mode != OB_MODE_OBJECT && ob->restore_mode != mode) {
			ED_object_toggle_modes(C, ob->restore_mode);
		}
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_mode_set(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Set Object Mode";
	ot->description = "Sets the object interaction mode";
	ot->idname = "OBJECT_OT_mode_set";
	
	/* api callbacks */
	ot->exec = object_mode_set_exec;
	
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = 0; /* no register/undo here, leave it to operators being called */
	
	ot->prop = RNA_def_enum(ot->srna, "mode", object_mode_items, OB_MODE_OBJECT, "Mode", "");
	RNA_def_enum_funcs(ot->prop, object_mode_set_itemsf);
	RNA_def_property_flag(ot->prop, PROP_SKIP_SAVE);

	prop = RNA_def_boolean(ot->srna, "toggle", 0, "Toggle", "");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}



void ED_object_toggle_modes(bContext *C, int mode)
{
	if (mode != OB_MODE_OBJECT) {
		const char *opstring = object_mode_op_string(mode);
		if (opstring) {
			WM_operator_name_call(C, opstring, WM_OP_EXEC_REGION_WIN, NULL);
		}
	}
}

/************************ Game Properties ***********************/

static int game_property_new_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	bProperty *prop;
	char name[MAX_NAME];
	int type = RNA_enum_get(op->ptr, "type");

	prop = BKE_bproperty_new(type);
	BLI_addtail(&ob->prop, prop);

	RNA_string_get(op->ptr, "name", name);
	if (name[0] != '\0') {
		BLI_strncpy(prop->name, name, sizeof(prop->name));
	}

	BKE_bproperty_unique(NULL, prop, 0); // make_unique_prop_names(prop->name);

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	return OPERATOR_FINISHED;
}


void OBJECT_OT_game_property_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Game Property";
	ot->description = "Create a new property available to the game engine";
	ot->idname = "OBJECT_OT_game_property_new";

	/* api callbacks */
	ot->exec = game_property_new_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "type", gameproperty_type_items, GPROP_FLOAT, "Type", "Type of game property to add");
	RNA_def_string(ot->srna, "name", NULL, MAX_NAME, "Name", "Name of the game property to add");
}

static int game_property_remove_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	bProperty *prop;
	int index = RNA_int_get(op->ptr, "index");

	if (!ob)
		return OPERATOR_CANCELLED;

	prop = BLI_findlink(&ob->prop, index);

	if (prop) {
		BLI_remlink(&ob->prop, prop);
		BKE_bproperty_free(prop);

		WM_event_add_notifier(C, NC_LOGIC, NULL);
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void OBJECT_OT_game_property_remove(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Remove Game Property";
	ot->description = "Remove game property";
	ot->idname = "OBJECT_OT_game_property_remove";

	/* api callbacks */
	ot->exec = game_property_remove_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_int(ot->srna, "index", 0, 0, INT_MAX, "Index", "Property index to remove ", 0, INT_MAX);
}

#define COPY_PROPERTIES_REPLACE 1
#define COPY_PROPERTIES_MERGE   2
#define COPY_PROPERTIES_COPY    3

static EnumPropertyItem game_properties_copy_operations[] = {
	{COPY_PROPERTIES_REPLACE, "REPLACE", 0, "Replace Properties", ""},
	{COPY_PROPERTIES_MERGE, "MERGE", 0, "Merge Properties", ""},
	{COPY_PROPERTIES_COPY, "COPY", 0, "Copy a Property", ""},
	{0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem *gameprops_itemf(bContext *C, PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{	
	Object *ob = ED_object_active_context(C);
	EnumPropertyItem tmp = {0, "", 0, "", ""};
	EnumPropertyItem *item = NULL;
	bProperty *prop;
	int a, totitem = 0;
	
	if (!ob)
		return DummyRNA_NULL_items;

	for (a = 1, prop = ob->prop.first; prop; prop = prop->next, a++) {
		tmp.value = a;
		tmp.identifier = prop->name;
		tmp.name = prop->name;
		RNA_enum_item_add(&item, &totitem, &tmp);
	}

	RNA_enum_item_end(&item, &totitem);
	*r_free = true;

	return item;
}

static int game_property_copy_exec(bContext *C, wmOperator *op)
{
	Object *ob = ED_object_active_context(C);
	bProperty *prop;
	int type = RNA_enum_get(op->ptr, "operation");
	int propid = RNA_enum_get(op->ptr, "property");

	if (propid > 0) { /* copy */
		prop = BLI_findlink(&ob->prop, propid - 1);
		
		if (prop) {
			CTX_DATA_BEGIN(C, Object *, ob_iter, selected_editable_objects)
			{
				if (ob != ob_iter)
					BKE_bproperty_object_set(ob_iter, prop);
			} CTX_DATA_END;
		}
	}

	else {
		CTX_DATA_BEGIN(C, Object *, ob_iter, selected_editable_objects)
		{
			if (ob != ob_iter) {
				if (type == COPY_PROPERTIES_REPLACE) {
					BKE_bproperty_copy_list(&ob_iter->prop, &ob->prop);
				}
				else {
					/* merge - the default when calling with no argument */
					for (prop = ob->prop.first; prop; prop = prop->next) {
						BKE_bproperty_object_set(ob_iter, prop);
					}
				}
			}
		}
		CTX_DATA_END;
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_game_property_copy(wmOperatorType *ot)
{
	PropertyRNA *prop;
	/* identifiers */
	ot->name = "Copy Game Property";
	ot->idname = "OBJECT_OT_game_property_copy";
	ot->description = "Copy/merge/replace a game property from active object to all selected objects";

	/* api callbacks */
	ot->exec = game_property_copy_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "operation", game_properties_copy_operations, 3, "Operation", "");
	prop = RNA_def_enum(ot->srna, "property", DummyRNA_NULL_items, 0, "Property", "Properties to copy");
	RNA_def_property_flag(prop, PROP_SKIP_SAVE | PROP_ENUM_NO_TRANSLATE);
	RNA_def_enum_funcs(prop, gameprops_itemf);
	ot->prop = prop;
}

static int game_property_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	CTX_DATA_BEGIN(C, Object *, ob_iter, selected_editable_objects)
	{
		BKE_bproperty_free_list(&ob_iter->prop);
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_LOGIC, NULL);
	return OPERATOR_FINISHED;
}
void OBJECT_OT_game_property_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Game Properties";
	ot->idname = "OBJECT_OT_game_property_clear";
	ot->description = "Remove all game properties from all selected objects";

	/* api callbacks */
	ot->exec = game_property_clear_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************ Copy Logic Bricks ***********************/

static int logicbricks_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_active_context(C);

	CTX_DATA_BEGIN(C, Object *, ob_iter, selected_editable_objects)
	{
		if (ob != ob_iter) {
			/* first: free all logic */
			free_sensors(&ob_iter->sensors);
			unlink_controllers(&ob_iter->controllers);
			free_controllers(&ob_iter->controllers);
			unlink_actuators(&ob_iter->actuators);
			free_actuators(&ob_iter->actuators);
		
			/* now copy it, this also works without logicbricks! */
			clear_sca_new_poins_ob(ob);
			copy_sensors(&ob_iter->sensors, &ob->sensors);
			copy_controllers(&ob_iter->controllers, &ob->controllers);
			copy_actuators(&ob_iter->actuators, &ob->actuators);
			set_sca_new_poins_ob(ob_iter);
		
			/* some menu settings */
			ob_iter->scavisflag = ob->scavisflag;
			ob_iter->scaflag = ob->scaflag;
		
			/* set the initial state */
			ob_iter->state = ob->state;
			ob_iter->init_state = ob->init_state;

			if (ob_iter->totcol == ob->totcol) {
				ob_iter->actcol = ob->actcol;
				WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob_iter);
			}
		}
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_LOGIC, NULL);

	return OPERATOR_FINISHED;
}

void OBJECT_OT_logic_bricks_copy(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Logic Bricks to Selected";
	ot->description = "Copy logic bricks to other selected objects";
	ot->idname = "OBJECT_OT_logic_bricks_copy";

	/* api callbacks */
	ot->exec = logicbricks_copy_exec;
	ot->poll = ED_operator_object_active_editable;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int game_physics_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = ED_object_active_context(C);
	
	CTX_DATA_BEGIN(C, Object *, ob_iter, selected_editable_objects)
	{
		if (ob != ob_iter) {
			ob_iter->gameflag = ob->gameflag;
			ob_iter->gameflag2 = ob->gameflag2;
			ob_iter->inertia = ob->inertia;
			ob_iter->formfactor = ob->formfactor;
			ob_iter->damping = ob->damping;
			ob_iter->rdamping = ob->rdamping;
			ob_iter->min_vel = ob->min_vel;
			ob_iter->max_vel = ob->max_vel;
			ob_iter->obstacleRad = ob->obstacleRad;
			ob_iter->mass = ob->mass;
			copy_v3_v3(ob_iter->anisotropicFriction, ob->anisotropicFriction);
			ob_iter->collision_boundtype = ob->collision_boundtype;
			ob_iter->margin = ob->margin;
			ob_iter->bsoft = copy_bulletsoftbody(ob->bsoft);
			if (ob->restrictflag & OB_RESTRICT_RENDER) 
				ob_iter->restrictflag |= OB_RESTRICT_RENDER;
			else
				ob_iter->restrictflag &= ~OB_RESTRICT_RENDER;

			ob_iter->col_group = ob->col_group;
			ob_iter->col_mask = ob->col_mask;
		}
	}
	CTX_DATA_END;
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_game_physics_copy(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Copy Game Physics Properties to Selected";
	ot->description = "Copy game physics properties to other selected objects";
	ot->idname = "OBJECT_OT_game_physics_copy";
	
	/* api callbacks */
	ot->exec = game_physics_copy_exec;
	ot->poll = ED_operator_object_active_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
