/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Ton Roosendaal, Blender Foundation '05, full recode.
 *				 Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 * support for animation modes - Reevan McKay
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_context.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_report.h"

#include "BIF_gl.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_anim_api.h"
#include "ED_keyframing.h"
#include "ED_object.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_transform.h" /* for autokey TFM_TRANSLATION, etc */
#include "ED_view3d.h"

#include "UI_interface.h"

#include "armature_intern.h"

/* ************* XXX *************** */
static int pupmenu() {return 0;}
static void error() {};
static void BIF_undo_push() {}
static void countall() {}
static void autokeyframe_pose_cb_func() {}
/* ************* XXX *************** */

/* This function is used to indicate that a bone is selected and needs keyframes inserted */
void set_pose_keys (Object *ob)
{
	bArmature *arm= ob->data;
	bPoseChannel *chan;

	if (ob->pose){
		for (chan=ob->pose->chanbase.first; chan; chan=chan->next){
			Bone *bone= chan->bone;
			if ((bone) && (bone->flag & BONE_SELECTED) && (arm->layer & bone->layer))
				chan->flag |= POSE_KEY;	
			else
				chan->flag &= ~POSE_KEY;
		}
	}
}

/* This function is used to process the necessary updates for */
void ED_armature_enter_posemode(bContext *C, Base *base)
{
	Object *ob= base->object;
	
	if (ob->id.lib){
		error ("Can't pose libdata");
		return;
	}
	
	switch (ob->type){
		case OB_ARMATURE:
			ob->restore_mode = ob->mode;
			ob->mode |= OB_MODE_POSE;
			
			WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_POSE, NULL);
			
			break;
		default:
			return;
	}

	//ED_object_toggle_modes(C, ob->mode);
}

void ED_armature_exit_posemode(bContext *C, Base *base)
{
	if(base) {
		Object *ob= base->object;
		
		ob->restore_mode = ob->mode;
		ob->mode &= ~OB_MODE_POSE;
		
		WM_event_add_notifier(C, NC_SCENE|ND_MODE|NS_MODE_OBJECT, NULL);
	}	
}

/* if a selected or active bone is protected, throw error (oonly if warn==1) and return 1 */
/* only_selected==1 : the active bone is allowed to be protected */
static short pose_has_protected_selected(Object *ob, short only_selected, short warn)
{
	/* check protection */
	if (ob->proxy) {
		bPoseChannel *pchan;
		bArmature *arm= ob->data;
		
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if (pchan->bone && (pchan->bone->layer & arm->layer)) {
				if (pchan->bone->layer & arm->layer_protected) {
					if (only_selected && (pchan->bone->flag & BONE_ACTIVE));
					else if (pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) 
					   break;
				}
			}
		}
		if (pchan) {
			if (warn) error("Cannot change Proxy protected bones");
			return 1;
		}
	}
	return 0;
}

/* only for real IK, not for auto-IK */
int ED_pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan)
{
	bConstraint *con;
	Bone *bone;
	
	/* No need to check if constraint is active (has influence),
	 * since all constraints with CONSTRAINT_IK_AUTO are active */
	for(con= pchan->constraints.first; con; con= con->next) {
		if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data= con->data;
			if((data->flag & CONSTRAINT_IK_AUTO)==0)
				return 1;
		}
	}
	for(bone= pchan->bone->childbase.first; bone; bone= bone->next) {
		pchan= get_pose_channel(ob->pose, bone->name);
		if(pchan && ED_pose_channel_in_IK_chain(ob, pchan))
			return 1;
	}
	return 0;
}

/* ********************************************** */

/* For the object with pose/action: update paths for those that have got them
 * This should selectively update paths that exist...
 *
 * To be called from various tools that do incremental updates 
 */
void ED_pose_recalculate_paths(bContext *C, Scene *scene, Object *ob)
{
	bArmature *arm;
	bPoseChannel *pchan;
	Base *base;
	float *fp;
	int cfra;
	int sfra, efra;
	
	/* sanity checks */
	if ELEM(NULL, ob, ob->pose)
		return;
	arm= ob->data;
	
	/* set frame values */
	cfra = CFRA;
	sfra = efra = cfra; 
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if ((pchan->bone) && (arm->layer & pchan->bone->layer)) {
			if (pchan->path) {
				/* if the pathsf and pathef aren't initialised, abort! */
				if (ELEM(0, pchan->pathsf, pchan->pathef))	
					return;
				
				/* try to increase area to do (only as much as needed) */
				sfra= MIN2(sfra, pchan->pathsf);
				efra= MAX2(efra, pchan->pathef);
			}
		}
	}
	if (efra <= sfra) return;
	
	/* hack: for unsaved files, set OB_RECALC so that paths can get calculated */
	if ((ob->recalc & OB_RECALC)==0) {
		ob->recalc |= OB_RECALC;
		ED_anim_object_flush_update(C, ob);
	}
	else
		ED_anim_object_flush_update(C, ob);
	
	/* calculate path over requested range */
	for (CFRA=sfra; CFRA<=efra; CFRA++) {
		/* do all updates */
		for (base= FIRSTBASE; base; base= base->next) {
			if (base->object->recalc) {
				int temp= base->object->recalc;
				
				if (base->object->adt)
					BKE_animsys_evaluate_animdata(&base->object->id, base->object->adt, (float)CFRA, ADT_RECALC_ALL);
				
				/* update object */
				object_handle_update(scene, base->object);
				base->object->recalc= temp;
			}
		}
		
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ((pchan->bone) && (arm->layer & pchan->bone->layer)) {
				if (pchan->path) {
					/* only update if:
					 *	- in range of this pchan's existing path
					 *	- ... insert evil filtering/optimising conditions here...
					 */
					if (IN_RANGE(CFRA, pchan->pathsf, pchan->pathef)) {
						fp= pchan->path+3*(CFRA-sfra);
						
						if (arm->pathflag & ARM_PATH_HEADS) { 
							VECCOPY(fp, pchan->pose_head);
						}
						else {
							VECCOPY(fp, pchan->pose_tail);
						}
						
						Mat4MulVecfl(ob->obmat, fp);
					}
				}
			}
		}
	}
	
	/* reset flags */
	CFRA= cfra;
	ob->pose->flag &= ~POSE_RECALCPATHS;
	
	/* flush one final time - to restore to the original state */
	for (base= FIRSTBASE; base; base= base->next) {
		if (base->object->recalc) {
			int temp= base->object->recalc;
			
			if (base->object->adt)
				BKE_animsys_evaluate_animdata(&base->object->id, base->object->adt, (float)CFRA, ADT_RECALC_ALL);
			
			object_handle_update(scene, base->object);
			base->object->recalc= temp;
		}
	}
}

/* --------- */

/* For the object with pose/action: create path curves for selected bones 
 * This recalculates the WHOLE path within the pchan->pathsf and pchan->pathef range
 */
static int pose_calculate_paths_exec (bContext *C, wmOperator *op)
{
	wmWindow *win= CTX_wm_window(C);
	ScrArea *sa= CTX_wm_area(C);
	Scene *scene= CTX_data_scene(C);
	Object *ob;
	bArmature *arm;
	bPoseChannel *pchan;
	Base *base;
	float *fp;
	int cfra;
	int sfra, efra;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
		
	/* only continue if there's an object */
	if ELEM(NULL, ob, ob->pose)
		return OPERATOR_CANCELLED;
	arm= ob->data;
	
	/* version patch for older files here (do_versions patch too complicated) */
	if ((arm->pathsf == 0) || (arm->pathef == 0)) {
		arm->pathsf = SFRA;
		arm->pathef = EFRA;
	}
	if (arm->pathsize == 0) {
		arm->pathsize = 1;
	}
	
	/* get frame values to use */
	cfra= CFRA;
	sfra = arm->pathsf;
	efra = arm->pathef;
	
	if (efra <= sfra) {
		BKE_report(op->reports, RPT_ERROR, "Can't calculate paths when pathlen <= 0");
		return OPERATOR_CANCELLED;
	}
	
	/* hack: for unsaved files, set OB_RECALC so that paths can get calculated */
	if ((ob->recalc & OB_RECALC)==0) {
		ob->recalc |= OB_RECALC;
		ED_anim_object_flush_update(C, ob);
	}
	else
		ED_anim_object_flush_update(C, ob);
	
	/* alloc the path cache arrays */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED)) {
			if (arm->layer & pchan->bone->layer) {
				pchan->pathlen= efra-sfra+1;
				pchan->pathsf= sfra;
				pchan->pathef= efra+1;
				if (pchan->path)
					MEM_freeN(pchan->path);
				pchan->path= MEM_callocN(3*pchan->pathlen*sizeof(float), "pchan path");
			}
		}
	}
	
	/* step through frame range sampling the values */
	for (CFRA=sfra; CFRA<=efra; CFRA++) {
		/* for each frame we calculate, update time-cursor... (may be too slow) */
		WM_timecursor(win, CFRA);
		
		/* do all updates */
		for (base= FIRSTBASE; base; base= base->next) {
			if (base->object->recalc) {
				int temp= base->object->recalc;
				
				if (base->object->adt)
					BKE_animsys_evaluate_animdata(&base->object->id, base->object->adt, (float)CFRA, ADT_RECALC_ALL);
				
				object_handle_update(scene, base->object);
				base->object->recalc= temp;
			}
		}
		
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED)) {
				if (arm->layer & pchan->bone->layer) {
					if (pchan->path) {
						fp= pchan->path+3*(CFRA-sfra);
						
						if (arm->pathflag & ARM_PATH_HEADS) { 
							VECCOPY(fp, pchan->pose_head);
						}
						else {
							VECCOPY(fp, pchan->pose_tail);
						}
						
						Mat4MulVecfl(ob->obmat, fp);
					}
				}
			}
		}
	}
	
	/* restore original cursor */
	WM_cursor_restore(win);
	
	/* reset current frame, and clear flags */
	CFRA= cfra;
	ob->pose->flag &= ~POSE_RECALCPATHS;
	
	/* flush one final time - to restore to the original state */
	for (base= FIRSTBASE; base; base= base->next) {
		if (base->object->recalc) {
			int temp= base->object->recalc;
			
			if (base->object->adt)
				BKE_animsys_evaluate_animdata(&base->object->id, base->object->adt, (float)CFRA, ADT_RECALC_ALL);
			
			object_handle_update(scene, base->object);
			base->object->recalc= temp;
		}
	}
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED; 
}

void POSE_OT_paths_calculate (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Calculate Bone Paths";
	ot->idname= "POSE_OT_paths_calculate";
	ot->description= "Calculate paths for the selected bones.";
	
	/* api callbacks */
	ot->exec= pose_calculate_paths_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* --------- */

/* for the object with pose/action: clear path curves for selected bones only */
void ED_pose_clear_paths(Object *ob)
{
	bPoseChannel *pchan;
	
	if ELEM(NULL, ob, ob->pose)
		return;
	
	/* free the path blocks */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED)) {
			if (pchan->path) {
				MEM_freeN(pchan->path);
				pchan->path= NULL;
			}
		}
	}
}

/* operator callback for this */
static int pose_clear_paths_exec (bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
		
	/* only continue if there's an object */
	if ELEM(NULL, ob, ob->pose)
		return OPERATOR_CANCELLED;
	
	/* for now, just call the API function for this (which is shared with backend functions) */
	ED_pose_clear_paths(ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED; 
}

void POSE_OT_paths_clear (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Bone Paths";
	ot->idname= "POSE_OT_paths_clear";
	ot->description= "Clear path caches for selected bones.";
	
	/* api callbacks */
	ot->exec= pose_clear_paths_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************* Select Constraint Target Operator ************* */

// XXX this function is to be removed when the other stuff is recoded
void pose_select_constraint_target(Scene *scene)
{
	Object *obedit= scene->obedit; // XXX context
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	bConstraint *con;
	
	/* paranoia checks */
	if (!ob && !ob->pose) return;
	if (ob==obedit || (ob->mode & OB_MODE_POSE)==0) return;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (arm->layer & pchan->bone->layer) {
			if (pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
				for (con= pchan->constraints.first; con; con= con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct= targets.first; ct; ct= ct->next) {
							if ((ct->tar == ob) && (ct->subtarget[0])) {
								bPoseChannel *pchanc= get_pose_channel(ob->pose, ct->subtarget);
								if(pchanc)
									pchanc->bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 1);
					}
				}
			}
		}
	}
	
	BIF_undo_push("Select constraint target");

}

static int pose_select_constraint_target_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	bConstraint *con;
	int found= 0;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (arm->layer & pchan->bone->layer) {
			if (pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
				for (con= pchan->constraints.first; con; con= con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct= targets.first; ct; ct= ct->next) {
							if ((ct->tar == ob) && (ct->subtarget[0])) {
								bPoseChannel *pchanc= get_pose_channel(ob->pose, ct->subtarget);
								if(pchanc) {
									pchanc->bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
									found= 1;
								}
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 1);
					}
				}
			}
		}
	}

	if(!found)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_select_constraint_target(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Constraint Target";
	ot->idname= "POSE_OT_select_constraint_target";
	
	/* api callbacks */
	ot->exec= pose_select_constraint_target_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ******************* select hierarchy operator ************* */

static int pose_select_hierarchy_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	Bone *curbone, *pabone, *chbone;
	int direction = RNA_enum_get(op->ptr, "direction");
	int add_to_sel = RNA_boolean_get(op->ptr, "extend");
	int found= 0;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		curbone= pchan->bone;
		
		if (arm->layer & curbone->layer) {
			if (curbone->flag & (BONE_ACTIVE)) {
				if (direction == BONE_SELECT_PARENT) {
				
					if (pchan->parent == NULL) continue;
					else pabone= pchan->parent->bone;
					
					if ((arm->layer & pabone->layer) && !(pabone->flag & BONE_HIDDEN_P)) {
						
						if (!add_to_sel) curbone->flag &= ~BONE_SELECTED;
						curbone->flag &= ~BONE_ACTIVE;
						pabone->flag |= (BONE_ACTIVE|BONE_SELECTED);

						found= 1;
						break;
					}
				} else { // BONE_SELECT_CHILD
				
					if (pchan->child == NULL) continue;
					else chbone = pchan->child->bone;
					
					if ((arm->layer & chbone->layer) && !(chbone->flag & BONE_HIDDEN_P)) {
					
						if (!add_to_sel) curbone->flag &= ~BONE_SELECTED;
						curbone->flag &= ~BONE_ACTIVE;
						chbone->flag |= (BONE_ACTIVE|BONE_SELECTED);

						found= 1;
						break;
					}
				}
			}
		}
	}

	if(!found)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_select_hierarchy(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[]= {
	{BONE_SELECT_PARENT, "PARENT", 0, "Select Parent", ""},
	{BONE_SELECT_CHILD, "CHILD", 0, "Select Child", ""},
	{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name= "Select Hierarchy";
	ot->idname= "POSE_OT_select_hierarchy";
	
	/* api callbacks */
	ot->exec= pose_select_hierarchy_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items,
				 BONE_SELECT_PARENT, "Direction", "");
	RNA_def_boolean(ot->srna, "extend", 0, "Add to Selection", "");
	
}


void pose_copy_menu(Scene *scene)
{
	Object *obedit= scene->obedit; // XXX context
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan, *pchanact;
	short nr=0;
	int i=0;
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) return;
	if ((ob==obedit) || (ob->mode & OB_MODE_POSE)==0) return;
	
	/* find active */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (pchan->bone->flag & BONE_ACTIVE) 
			break;
	}
	
	if (pchan==NULL) return;
	pchanact= pchan;
	
	/* if proxy-protected bones selected, some things (such as locks + displays) shouldn't be changable, 
	 * but for constraints (just add local constraints)
	 */
	if (pose_has_protected_selected(ob, 1, 0)) {
		i= BLI_countlist(&(pchanact->constraints)); /* if there are 24 or less, allow for the user to select constraints */
		if (i < 25)
			nr= pupmenu("Copy Pose Attributes %t|Local Location%x1|Local Rotation%x2|Local Size%x3|%l|Visual Location %x9|Visual Rotation%x10|Visual Size%x11|%l|Constraints (All)%x4|Constraints...%x5");
		else
			nr= pupmenu("Copy Pose Attributes %t|Local Location%x1|Local Rotation%x2|Local Size%x3|%l|Visual Location %x9|Visual Rotation%x10|Visual Size%x11|%l|Constraints (All)%x4");
	}
	else {
		i= BLI_countlist(&(pchanact->constraints)); /* if there are 24 or less, allow for the user to select constraints */
		if (i < 25)
			nr= pupmenu("Copy Pose Attributes %t|Local Location%x1|Local Rotation%x2|Local Size%x3|%l|Visual Location %x9|Visual Rotation%x10|Visual Size%x11|%l|Constraints (All)%x4|Constraints...%x5|%l|Transform Locks%x6|IK Limits%x7|Bone Shape%x8");
		else
			nr= pupmenu("Copy Pose Attributes %t|Local Location%x1|Local Rotation%x2|Local Size%x3|%l|Visual Location %x9|Visual Rotation%x10|Visual Size%x11|%l|Constraints (All)%x4|%l|Transform Locks%x6|IK Limits%x7|Bone Shape%x8");
	}
	
	if (nr <= 0) 
		return;
	
	if (nr != 5)  {
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ( (arm->layer & pchan->bone->layer) &&
				 (pchan->bone->flag & BONE_SELECTED) &&
				 (pchan != pchanact) ) 
			{
				switch (nr) {
					case 1: /* Local Location */
						VECCOPY(pchan->loc, pchanact->loc);
						break;
					case 2: /* Local Rotation */
						QUATCOPY(pchan->quat, pchanact->quat);
						VECCOPY(pchan->eul, pchanact->eul);
						break;
					case 3: /* Local Size */
						VECCOPY(pchan->size, pchanact->size);
						break;
					case 4: /* All Constraints */
					{
						ListBase tmp_constraints = {NULL, NULL};
						
						/* copy constraints to tmpbase and apply 'local' tags before 
						 * appending to list of constraints for this channel
						 */
						copy_constraints(&tmp_constraints, &pchanact->constraints);
						if ((ob->proxy) && (pchan->bone->layer & arm->layer_protected)) {
							bConstraint *con;
							
							/* add proxy-local tags */
							for (con= tmp_constraints.first; con; con= con->next)
								con->flag |= CONSTRAINT_PROXY_LOCAL;
						}
						addlisttolist(&pchan->constraints, &tmp_constraints);
						
						/* update flags (need to add here, not just copy) */
						pchan->constflag |= pchanact->constflag;
						
						if (ob->pose)
							ob->pose->flag |= POSE_RECALC;
					}
						break;
					case 6: /* Transform Locks */
						pchan->protectflag = pchanact->protectflag;
						break;
					case 7: /* IK (DOF) settings */
					{
						pchan->ikflag = pchanact->ikflag;
						VECCOPY(pchan->limitmin, pchanact->limitmin);
						VECCOPY(pchan->limitmax, pchanact->limitmax);
						VECCOPY(pchan->stiffness, pchanact->stiffness);
						pchan->ikstretch= pchanact->ikstretch;
						pchan->ikrotweight= pchanact->ikrotweight;
						pchan->iklinweight= pchanact->iklinweight;
					}
						break;
					case 8: /* Custom Bone Shape */
						pchan->custom = pchanact->custom;
						break;
					case 9: /* Visual Location */
						armature_loc_pose_to_bone(pchan, pchanact->pose_mat[3], pchan->loc);
						break;
					case 10: /* Visual Rotation */
					{
						float delta_mat[4][4];
						
						armature_mat_pose_to_bone(pchan, pchanact->pose_mat, delta_mat);
						
						if (pchan->rotmode == PCHAN_ROT_AXISANGLE) {
							float tmp_quat[4];
							
							/* need to convert to quat first (in temp var)... */
							Mat4ToQuat(delta_mat, tmp_quat);
							QuatToAxisAngle(tmp_quat, &pchan->quat[1], &pchan->quat[0]);
						}
						else if (pchan->rotmode == PCHAN_ROT_QUAT)
							Mat4ToQuat(delta_mat, pchan->quat);
						else
							Mat4ToEulO(delta_mat, pchan->eul, pchan->rotmode);
					}
						break;
					case 11: /* Visual Size */
					{
						float delta_mat[4][4], size[4];
						
						armature_mat_pose_to_bone(pchan, pchanact->pose_mat, delta_mat);
						Mat4ToSize(delta_mat, size);
						VECCOPY(pchan->size, size);
					}
				}
			}
		}
	} 
	else { /* constraints, optional (note: max we can have is 24 constraints) */
		bConstraint *con, *con_back;
		int const_toggle[24];
		ListBase const_copy = {NULL, NULL};
		
		BLI_duplicatelist(&const_copy, &(pchanact->constraints));
		
		/* build the puplist of constraints */
		for (con = pchanact->constraints.first, i=0; con; con=con->next, i++){
			const_toggle[i]= 1;
//			add_numbut(i, TOG|INT, con->name, 0, 0, &(const_toggle[i]), "");
		}
		
//		if (!do_clever_numbuts("Select Constraints", i, REDRAW)) {
//			BLI_freelistN(&const_copy);
//			return;
//		}
		
		/* now build a new listbase from the options selected */
		for (i=0, con=const_copy.first; con; i++) {
			/* if not selected, free/remove it from the list */
			if (!const_toggle[i]) {
				con_back= con->next;
				BLI_freelinkN(&const_copy, con);
				con= con_back;
			} 
			else
				con= con->next;
		}
		
		/* Copy the temo listbase to the selected posebones */
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ( (arm->layer & pchan->bone->layer) &&
				 (pchan->bone->flag & BONE_SELECTED) &&
				 (pchan!=pchanact) ) 
			{
				ListBase tmp_constraints = {NULL, NULL};
				
				/* copy constraints to tmpbase and apply 'local' tags before 
				 * appending to list of constraints for this channel
				 */
				copy_constraints(&tmp_constraints, &const_copy);
				if ((ob->proxy) && (pchan->bone->layer & arm->layer_protected)) {
					bConstraint *con;
					
					/* add proxy-local tags */
					for (con= tmp_constraints.first; con; con= con->next)
						con->flag |= CONSTRAINT_PROXY_LOCAL;
				}
				addlisttolist(&pchan->constraints, &tmp_constraints);
				
				/* update flags (need to add here, not just copy) */
				pchan->constflag |= pchanact->constflag;
			}
		}
		BLI_freelistN(&const_copy);
		update_pose_constraint_flags(ob->pose); /* we could work out the flags but its simpler to do this */
		
		if (ob->pose)
			ob->pose->flag |= POSE_RECALC;
	}
	
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);	// and all its relations
	
	BIF_undo_push("Copy Pose Attributes");
	
}

/* ******************** copy/paste pose ********************** */

/* Global copy/paste buffer for pose - cleared on start/end session + before every copy operation */
static bPose *g_posebuf = NULL;

void free_posebuf(void) 
{
	if (g_posebuf) {
		/* was copied without constraints */
		BLI_freelistN(&g_posebuf->chanbase);
		MEM_freeN(g_posebuf);
	}
	
	g_posebuf=NULL;
}

/* ---- */

static int pose_copy_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	
	/* sanity checking */
	if ELEM(NULL, ob, ob->pose) {
		BKE_report(op->reports, RPT_ERROR, "No Pose to Copy");
		return OPERATOR_CANCELLED;
	}

	/* free existing pose buffer */
	free_posebuf();
	
	/* sets chan->flag to POSE_KEY if bone selected, then copy those bones to the buffer */
	set_pose_keys(ob);  
	copy_pose(&g_posebuf, ob->pose, 0);
	
	
	return OPERATOR_FINISHED;
}

void POSE_OT_copy (wmOperatorType *ot) 
{
	/* identifiers */
	ot->name= "Copy Pose";
	ot->idname= "POSE_OT_copy";
	ot->description= "Copies the current pose of the selected bones to copy/paste buffer.";
	
	/* api callbacks */
	ot->exec= pose_copy_exec;
	ot->poll= ED_operator_posemode;
	
	/* flag */
	ot->flag= OPTYPE_REGISTER;
}

/* ---- */

/* Pointers to the builtin KeyingSets that we want to use */
static KeyingSet *posePaste_ks_locrotscale = NULL;		/* the only keyingset we'll need */

/* ---- */

static int pose_paste_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	bPoseChannel *chan, *pchan;
	char name[32];
	int flip= RNA_boolean_get(op->ptr, "flipped");
	
	bCommonKeySrc cks;
	ListBase dsources = {&cks, &cks};
	
	/* init common-key-source for use by KeyingSets */
	memset(&cks, 0, sizeof(bCommonKeySrc));
	cks.id= &ob->id;
	
	/* sanity checks */
	if ELEM(NULL, ob, ob->pose)
		return OPERATOR_CANCELLED;

	if (g_posebuf == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Copy buffer is empty");
		return OPERATOR_CANCELLED;
	}
	
	/* Safely merge all of the channels in the buffer pose into any existing pose */
	for (chan= g_posebuf->chanbase.first; chan; chan=chan->next) {
		if (chan->flag & POSE_KEY) {
			/* get the name - if flipping, we must flip this first */
			BLI_strncpy(name, chan->name, sizeof(name));
			if (flip)
				bone_flip_name(name, 0);		/* 0 = don't strip off number extensions */
				
			/* only copy when channel exists, poses are not meant to add random channels to anymore */
			pchan= get_pose_channel(ob->pose, name);
			
			if (pchan) {
				/* only loc rot size 
				 *	- only copies transform info for the pose 
				 */
				VECCOPY(pchan->loc, chan->loc);
				VECCOPY(pchan->size, chan->size);
				pchan->flag= chan->flag;
				
				/* check if rotation modes are compatible (i.e. do they need any conversions) */
				if (pchan->rotmode == chan->rotmode) {
					/* copy the type of rotation in use */
					if (pchan->rotmode > 0) {
						VECCOPY(pchan->eul, chan->eul);
					}
					else {
						QUATCOPY(pchan->quat, chan->quat);
					}
				}
				else if (pchan->rotmode > 0) {
					/* quat/axis-angle to euler */
					if (chan->rotmode == PCHAN_ROT_AXISANGLE)
						AxisAngleToEulO(&chan->quat[1], chan->quat[0], pchan->eul, pchan->rotmode);
					else
						QuatToEulO(chan->quat, pchan->eul, pchan->rotmode);
				}
				else if (pchan->rotmode == PCHAN_ROT_AXISANGLE) {
					/* quat/euler to axis angle */
					if (chan->rotmode > 0)
						EulOToAxisAngle(chan->eul, chan->rotmode, &pchan->quat[1], &pchan->quat[0]);
					else	
						QuatToAxisAngle(chan->quat, &pchan->quat[1], &pchan->quat[0]);
				}
				else {
					/* euler/axis-angle to quat */
					if (chan->rotmode > 0)
						EulOToQuat(chan->eul, chan->rotmode, pchan->quat);
					else
						AxisAngleToQuat(pchan->quat, &chan->quat[1], chan->quat[0]);
				}
				
				/* paste flipped pose? */
				if (flip) {
					pchan->loc[0]*= -1;
					
					/* has to be done as eulers... */
					if (pchan->rotmode > 0) {
						pchan->eul[1] *= -1;
						pchan->eul[2] *= -1;
					}
					else if (pchan->rotmode == PCHAN_ROT_AXISANGLE) {
						float eul[3];
						
						AxisAngleToEulO(&pchan->quat[1], pchan->quat[0], eul, EULER_ORDER_DEFAULT);
						eul[1]*= -1;
						eul[2]*= -1;
						EulOToAxisAngle(eul, EULER_ORDER_DEFAULT, &pchan->quat[1], &pchan->quat[0]);
						
						// experimental method (uncomment to test):
#if 0
						/* experimental method: just flip the orientation of the axis on x/y axes */
						pchan->quat[1] *= -1;
						pchan->quat[2] *= -1;
#endif
					}
					else {
						float eul[3];
						
						QuatToEul(pchan->quat, eul);
						eul[1]*= -1;
						eul[2]*= -1;
						EulToQuat(eul, pchan->quat);
					}
				}
				
				if (autokeyframe_cfra_can_key(scene, &ob->id)) {
					/* Set keys on pose
					 *	- KeyingSet to use depends on rotation mode 
					 *	(but that's handled by the templates code)  
					 */
					// TODO: for getting the KeyingSet used, we should really check which channels were affected
					if (posePaste_ks_locrotscale == NULL)
						posePaste_ks_locrotscale= ANIM_builtin_keyingset_get_named(NULL, "LocRotScale");
					
					/* init cks for this PoseChannel, then use the relative KeyingSets to keyframe it */
					cks.pchan= pchan;
					
					modify_keyframes(C, &dsources, NULL, posePaste_ks_locrotscale, MODIFYKEY_MODE_INSERT, (float)CFRA);
					
					/* clear any unkeyed tags */
					if (chan->bone)
						chan->bone->flag &= ~BONE_UNKEYED;
				}
				else {
					/* add unkeyed tags */
					if (chan->bone)
						chan->bone->flag |= BONE_UNKEYED;
				}
			}
		}
	}

	/* Update event for pose and deformation children */
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	
	if (IS_AUTOKEY_ON(scene)) {
// XXX		remake_action_ipos(ob->action);
	}
	else {
		/* need to trick depgraph, action is not allowed to execute on pose */
		where_is_pose(scene, ob);
		ob->recalc= 0;
	}
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE|ND_TRANSFORM, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_paste (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Paste Pose";
	ot->idname= "POSE_OT_paste";
	ot->description= "Pastes the stored pose on to the current pose.";
	
	/* api callbacks */
	ot->exec= pose_paste_exec;
	ot->poll= ED_operator_posemode;
	
	/* flag */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "flipped", 0, "Flipped on X-Axis", "");
}

/* ********************************************** */

/* context weightpaint and deformer in posemode */
void pose_adds_vgroups(Scene *scene, Object *meshobj, int heatweights)
{
// XXX	extern VPaint Gwp;         /* from vpaint */
	Object *poseobj= modifiers_isDeformedByArmature(meshobj);

	if(poseobj==NULL || (poseobj->mode & OB_MODE_POSE)==0) {
		error("The active object must have a deforming armature in pose mode");
		return;
	}

// XXX	add_verts_to_dgroups(meshobj, poseobj, heatweights, (Gwp.flag & VP_MIRROR_X));

	if(heatweights)
		BIF_undo_push("Apply Bone Heat Weights to Vertex Groups");
	else
		BIF_undo_push("Apply Bone Envelopes to Vertex Groups");

	
	// and all its relations
	DAG_id_flush_update(&meshobj->id, OB_RECALC_DATA);
}

/* ********************************************** */


static int pose_group_add_exec (bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
		
	/* only continue if there's an object */
	if (ob == NULL)
		return OPERATOR_CANCELLED;
	
	/* for now, just call the API function for this */
	pose_add_group(ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_group_add (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Bone Group";
	ot->idname= "POSE_OT_group_add";
	ot->description= "Add a new bone group.";
	
	/* api callbacks */
	ot->exec= pose_group_add_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int pose_group_remove_exec (bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
	
	/* only continue if there's an object */
	if (ob == NULL)
		return OPERATOR_CANCELLED;
	
	/* for now, just call the API function for this */
	pose_remove_group(ob);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_group_remove (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Bone Group";
	ot->idname= "POSE_OT_group_remove";
	ot->description= "Removes the active bone group.";
	
	/* api callbacks */
	ot->exec= pose_group_remove_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ------------ */

/* invoke callback which presents a list of bone-groups for the user to choose from */
static int pose_groups_menu_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	bPose *pose;
	
	uiPopupMenu *pup;
	uiLayout *layout;
	bActionGroup *grp;
	int i;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
	
	/* only continue if there's an object, and a pose there too */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	pose= ob->pose;
	
	/* if there's no active group (or active is invalid), create a new menu to find it */
	if (pose->active_group <= 0) {
		/* create a new menu, and start populating it with group names */
		pup= uiPupMenuBegin(C, op->type->name, 0);
		layout= uiPupMenuLayout(pup);
		
		/* special entry - allow to create new group, then use that 
		 *	(not to be used for removing though)
		 */
		if (strstr(op->idname, "assign")) {
			uiItemIntO(layout, "New Group", 0, op->idname, "type", 0);
			uiItemS(layout);
		}
		
		/* add entries for each group */
		for (grp= pose->agroups.first, i=1; grp; grp=grp->next, i++)
			uiItemIntO(layout, grp->name, 0, op->idname, "type", i);
			
		/* finish building the menu, and process it (should result in calling self again) */
		uiPupMenuEnd(C, pup);
		
		return OPERATOR_CANCELLED;
	}
	else {
		/* just use the active group index, and call the exec callback for the calling operator */
		RNA_int_set(op->ptr, "type", pose->active_group);
		return op->type->exec(C, op);
	}
}

/* Assign selected pchans to the bone group that the user selects */
static int pose_group_assign_exec (bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	bArmature *arm;
	bPose *pose;
	bPoseChannel *pchan;
	short done= 0;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
	
	/* only continue if there's an object, and a pose there too */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	arm= ob->data;
	pose= ob->pose;
	
	/* set the active group number to the one from operator props 
	 * 	- if 0 after this, make a new group...
	 */
	pose->active_group= RNA_int_get(op->ptr, "type");
	if (pose->active_group == 0)
		pose_add_group(ob);
	
	/* add selected bones to group then */
	// NOTE: unfortunately, we cannot use the context-iterators here, since they might not be defined...
	// CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pchans) 
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		/* ensure that PoseChannel is on visible layer and is not hidden in PoseMode */
		// NOTE: sync this view3d_context() in space_view3d.c
		if ((pchan->bone) && (arm->layer & pchan->bone->layer) && !(pchan->bone->flag & BONE_HIDDEN_P)) {
			if (pchan->bone->flag & (BONE_SELECTED|BONE_ACTIVE)) {
				pchan->agrp_index= pose->active_group;
				done= 1;
			}
		}
	}
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	/* report done status */
	if (done)
		return OPERATOR_FINISHED;
	else
		return OPERATOR_CANCELLED;
}

void POSE_OT_group_assign (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Selected to Bone Group";
	ot->idname= "POSE_OT_group_assign";
	ot->description= "Add selected bones to the chosen bone group.";
	
	/* api callbacks */
	ot->invoke= pose_groups_menu_invoke;
	ot->exec= pose_group_assign_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_int(ot->srna, "type", 0, 0, 10, "Bone Group Index", "", 0, INT_MAX);
}


static int pose_group_unassign_exec (bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	bArmature *arm;
	bPose *pose;
	bPoseChannel *pchan;
	short done= 0;
	
	/* since this call may also be used from the buttons window, we need to check for where to get the object */
	if (sa->spacetype == SPACE_BUTS) 
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
	
	/* only continue if there's an object, and a pose there too */
	if (ELEM(NULL, ob, ob->pose))
		return OPERATOR_CANCELLED;
	pose= ob->pose;
	arm= ob->data;
	
	/* add selected bones to ungroup then */
	// NOTE: unfortunately, we cannot use the context-iterators here, since they might not be defined...
	// CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pchans) 
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		/* ensure that PoseChannel is on visible layer and is not hidden in PoseMode */
		// NOTE: sync this view3d_context() in space_view3d.c
		if ((pchan->bone) && (arm->layer & pchan->bone->layer) && !(pchan->bone->flag & BONE_HIDDEN_P)) {
			if (pchan->bone->flag & (BONE_SELECTED|BONE_ACTIVE)) {
				if (pchan->agrp_index) {
					pchan->agrp_index= 0;
					done= 1;
				}
			}
		}
	}
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	/* report done status */
	if (done)
		return OPERATOR_FINISHED;
	else
		return OPERATOR_CANCELLED;
}

void POSE_OT_group_unassign (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove Selected from Bone Groups";
	ot->idname= "POSE_OT_group_unassign";
	ot->description= "Add selected bones from all bone groups";
	
	/* api callbacks */
	ot->exec= pose_group_unassign_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ----------------- */

static int pose_groupOps_menu_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	Object *ob= CTX_data_active_object(C);
	uiPopupMenu *pup= uiPupMenuBegin(C, op->type->name, 0);
	uiLayout *layout= uiPupMenuLayout(pup);
	
	/* sanity check - must have object with pose */
	if ELEM(NULL, ob, ob->pose)
		return OPERATOR_CANCELLED;
	
	/* get mode of action */
	if (CTX_DATA_COUNT(C, selected_pchans)) {
		/* if selected bone(s), include options to add/remove to active group */
		uiItemO(layout, "Add Selected to Active Group", 0, "POSE_OT_group_assign");
		
		uiItemS(layout);
		
		uiItemO(layout, "Remove Selected from All Groups", 0, "POSE_OT_group_unassign");
		uiItemO(layout, "Remove Active Group", 0, "POSE_OT_group_remove");
	}
	else {
		/* no selected bones - so just options for groups management */
		uiItemO(layout, "Add New Group", 0, "POSE_OT_group_add");
		uiItemO(layout, "Remove Active Group", 0, "POSE_OT_group_remove");
	}
		
	return OPERATOR_CANCELLED;
}

void POSE_OT_groups_menu (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Bone Group Tools";
	ot->idname= "POSE_OT_groups_menu";
	ot->description= "Menu displaying available tools for Bone Groups.";
	
	/* api callbacks (only invoke needed) */
	ot->invoke= pose_groupOps_menu_invoke;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER;
}

/* ********************************************** */

static short pose_select_same_group (Object *ob)
{
	bPose *pose= (ob)? ob->pose : NULL;
	bArmature *arm= (ob)? ob->data : NULL;
	bPoseChannel *pchan, *chan;
	short changed= 0;
	
	if (ELEM3(NULL, ob, pose, arm))
		return 0;
	
	/* loop in loop... bad and slow! */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (arm->layer & pchan->bone->layer) {
			if (pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
				
				/* only if group matches (and is not selected or current bone) */
				for (chan= ob->pose->chanbase.first; chan; chan= chan->next) {
					if (arm->layer & chan->bone->layer) {
						if (pchan->agrp_index == chan->agrp_index) {
							chan->bone->flag |= BONE_SELECTED;
							changed= 1;
						}
					}
				}
				
			}
		}
	}
	
	return changed;
}

static short pose_select_same_layer (Object *ob)
{
	bPose *pose= (ob)? ob->pose : NULL;
	bArmature *arm= (ob)? ob->data : NULL;
	bPoseChannel *pchan;
	short layers= 0, changed= 0;
	
	if (ELEM3(NULL, ob, pose, arm))
		return 0;
	
	/* figure out what bones are selected */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (arm->layer & pchan->bone->layer) {
			if (pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
				layers |= pchan->bone->layer;
			}
		}
	}
	if (layers == 0) 
		return 0;
		
	/* select bones that are on same layers as layers flag */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (arm->layer & pchan->bone->layer) {
			if (layers & pchan->bone->layer) {
				pchan->bone->flag |= BONE_SELECTED;
				changed= 1;
			}
		}
	}
	
	return changed;
}

void pose_select_grouped (Scene *scene, short nr)
{
	short changed = 0;
	
	if (nr == 1) 		changed= pose_select_same_group(OBACT);
	else if (nr == 2)	changed= pose_select_same_layer(OBACT);
	
	if (changed) {
		countall();
		BIF_undo_push("Select Grouped");
	}
}

/* Shift-G in 3D-View while in PoseMode */
void pose_select_grouped_menu (Scene *scene)
{
	short nr;
	
	/* here we go */
	nr= pupmenu("Select Grouped%t|In Same Group%x1|In Same Layer%x2");
	pose_select_grouped(scene, nr);
}

/* ********************************************** */

static int pose_flip_names_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm;
	char newname[32];
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm= ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pchans)
	{
		BLI_strncpy(newname, pchan->name, sizeof(newname));
		bone_flip_name(newname, 1);	// 1 = do strip off number extensions
		ED_armature_bone_rename(arm, pchan->name, newname);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_flip_names (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Flip Names";
	ot->idname= "POSE_OT_flip_names";
	ot->description= "Flips (and corrects) the names of selected bones.";
	
	/* api callbacks */
	ot->exec= pose_flip_names_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ------------------ */

static int pose_autoside_names_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm;
	char newname[32];
	short axis= RNA_enum_get(op->ptr, "axis");
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm= ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pchans)
	{
		BLI_strncpy(newname, pchan->name, sizeof(newname));
		bone_autoside_name(newname, 1, axis, pchan->bone->head[axis], pchan->bone->tail[axis]);
		ED_armature_bone_rename(arm, pchan->name, newname);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_autoside_names (wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[]= {
 		{0, "XAXIS", 0, "X-Axis", "Left/Right"},
		{1, "YAXIS", 0, "Y-Axis", "Front/Back"},
		{2, "ZAXIS", 0, "Z-Axis", "Top/Bottom"},
		{0, NULL, 0, NULL, NULL}};
	
	/* identifiers */
	ot->name= "AutoName by Axis";
	ot->idname= "POSE_OT_autoside_names";
	ot->description= "Automatically renames the selected bones according to which side of the target axis they fall on.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= pose_autoside_names_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* settings */
	RNA_def_enum(ot->srna, "axis", axis_items, 0, "Axis", "Axis tag names with.");
}

/* ********************************************** */

/* context active object, or weightpainted object with armature in posemode */
void pose_activate_flipped_bone(Scene *scene)
{
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	
	if(ob==NULL) return;

	if(ob->mode && OB_MODE_WEIGHT_PAINT) {
		ob= modifiers_isDeformedByArmature(ob);
	}
	if(ob && (ob->mode & OB_MODE_POSE)) {
		bPoseChannel *pchan, *pchanf;
		
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if(arm->layer & pchan->bone->layer) {
				if(pchan->bone->flag & BONE_ACTIVE)
					break;
			}
		}
		if(pchan) {
			char name[32];
			
			BLI_strncpy(name, pchan->name, 32);
			bone_flip_name(name, 1);	// 0 = do not strip off number extensions
			
			pchanf= get_pose_channel(ob->pose, name);
			if(pchanf && pchanf!=pchan) {
				pchan->bone->flag &= ~(BONE_SELECTED|BONE_ACTIVE);
				pchanf->bone->flag |= (BONE_SELECTED|BONE_ACTIVE);
			
				/* in weightpaint we select the associated vertex group too */
				if(ob->mode & OB_MODE_WEIGHT_PAINT) {
					ED_vgroup_select_by_name(OBACT, name);
					DAG_id_flush_update(&OBACT->id, OB_RECALC_DATA);
				}
				
				// XXX notifiers need to be sent to other editors to update
				
			}			
		}
	}
}


/* ********************************************** */

/* Present a popup to get the layers that should be used */
// TODO: move to wm?
static uiBlock *wm_layers_select_create_menu(bContext *C, ARegion *ar, void *arg_op)
{
	wmOperator *op= arg_op;
	uiBlock *block;
	uiLayout *layout;
	uiStyle *style= U.uistyles.first;
	
	block= uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockClearFlag(block, UI_BLOCK_LOOP);
	uiBlockSetFlag(block, UI_BLOCK_KEEP_OPEN);
	
	layout= uiBlockLayout(block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, 150, 20, style);
		uiItemL(layout, op->type->name, 0);
		uiTemplateLayers(layout, op->ptr, "layers"); /* must have a property named layers setup */
		
	uiPopupBoundsBlock(block, 4.0f, 0, 0);
	uiEndBlock(C, block);
	
	return block;
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int pose_armature_layers_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm= (ob)? ob->data : NULL;
	PointerRNA ptr;
	int layers[16]; /* hardcoded for now - we can only have 16 armature layers, so this should be fine... */
	
	/* sanity checking */
	if (arm == NULL)
		return OPERATOR_CANCELLED;
		
	/* get RNA pointer to armature data to use that to retrieve the layers as ints to init the operator */
	RNA_id_pointer_create((ID *)arm, &ptr);
	RNA_boolean_get_array(&ptr, "layer", layers);
	RNA_boolean_set_array(op->ptr, "layers", layers);
	
		/* part to sync with other similar operators... */
	/* pass on operator, so return modal */
	uiPupBlockOperator(C, wm_layers_select_create_menu, op, WM_OP_EXEC_DEFAULT);
	return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int pose_armature_layers_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm= (ob)? ob->data : NULL;
	PointerRNA ptr;
	int layers[16]; /* hardcoded for now - we can only have 16 armature layers, so this should be fine... */
	
	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);
	
	/* get pointer for armature, and write data there... */
	RNA_id_pointer_create((ID *)arm, &ptr);
	RNA_boolean_set_array(&ptr, "layer", layers);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}


void POSE_OT_armature_layers (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Change Armature Layers";
	ot->idname= "POSE_OT_armature_layers";
	ot->description= "Change the visible armature layers.";
	
	/* callbacks */
	ot->invoke= pose_armature_layers_invoke;
	ot->exec= pose_armature_layers_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_array(ot->srna, "layers", 16, NULL, "Layers", "Armature layers to make visible.");
}

void ARMATURE_OT_armature_layers (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Change Armature Layers";
	ot->idname= "ARMATURE_OT_armature_layers";
	ot->description= "Change the visible armature layers.";
	
	/* callbacks */
	ot->invoke= pose_armature_layers_invoke;
	ot->exec= pose_armature_layers_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_array(ot->srna, "layers", 16, NULL, "Layers", "Armature layers to make visible.");
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int pose_bone_layers_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	int layers[16]; /* hardcoded for now - we can only have 16 armature layers, so this should be fine... */
	
	/* get layers that are active already */
	memset(&layers, 0, sizeof(layers)); /* set all layers to be off by default */
	
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, selected_pchans) 
	{
		short bit;
		
		/* loop over the bits for this pchan's layers, adding layers where they're needed */
		for (bit= 0; bit < 16; bit++) {
			if (pchan->bone->layer & (1<<bit))
				layers[bit]= 1;
		}
	}
	CTX_DATA_END;
	
	/* copy layers to operator */
	RNA_boolean_set_array(op->ptr, "layers", layers);
	
		/* part to sync with other similar operators... */
	/* pass on operator, so return modal */
	uiPupBlockOperator(C, wm_layers_select_create_menu, op, WM_OP_EXEC_DEFAULT);
	return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int pose_bone_layers_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm= (ob)? ob->data : NULL;
	PointerRNA ptr;
	int layers[16]; /* hardcoded for now - we can only have 16 armature layers, so this should be fine... */
	
	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);
	
	/* set layers of pchans based on the values set in the operator props */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, selected_pchans) 
	{
		/* get pointer for pchan, and write flags this way */
		RNA_pointer_create((ID *)arm, &RNA_Bone, pchan->bone, &ptr);
		RNA_boolean_set_array(&ptr, "layer", layers);
	}
	CTX_DATA_END;
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_bone_layers (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Change Bone Layers";
	ot->idname= "POSE_OT_bone_layers";
	ot->description= "Change the layers that the selected bones belong to.";
	
	/* callbacks */
	ot->invoke= pose_bone_layers_invoke;
	ot->exec= pose_bone_layers_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_array(ot->srna, "layers", 16, NULL, "Layers", "Armature layers that bone belongs to.");
}

/* ------------------- */

/* Present a popup to get the layers that should be used */
static int armature_bone_layers_invoke (bContext *C, wmOperator *op, wmEvent *evt)
{
	int layers[16]; /* hardcoded for now - we can only have 16 armature layers, so this should be fine... */
	
	/* get layers that are active already */
	memset(&layers, 0, sizeof(layers)); /* set all layers to be off by default */
	
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) 
	{
		short bit;
		
		/* loop over the bits for this pchan's layers, adding layers where they're needed */
		for (bit= 0; bit < 16; bit++) {
			if (ebone->layer & (1<<bit))
				layers[bit]= 1;
		}
	}
	CTX_DATA_END;
	
	/* copy layers to operator */
	RNA_boolean_set_array(op->ptr, "layers", layers);
	
		/* part to sync with other similar operators... */
	/* pass on operator, so return modal */
	uiPupBlockOperator(C, wm_layers_select_create_menu, op, WM_OP_EXEC_DEFAULT);
	return OPERATOR_RUNNING_MODAL|OPERATOR_PASS_THROUGH;
}

/* Set the visible layers for the active armature (edit and pose modes) */
static int armature_bone_layers_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_edit_object(C);
	bArmature *arm= (ob)? ob->data : NULL;
	PointerRNA ptr;
	int layers[16]; /* hardcoded for now - we can only have 16 armature layers, so this should be fine... */
	
	/* get the values set in the operator properties */
	RNA_boolean_get_array(op->ptr, "layers", layers);
	
	/* set layers of pchans based on the values set in the operator props */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) 
	{
		/* get pointer for pchan, and write flags this way */
		RNA_pointer_create((ID *)arm, &RNA_EditBone, ebone, &ptr);
		RNA_boolean_set_array(&ptr, "layer", layers);
	}
	CTX_DATA_END;
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_bone_layers (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Change Bone Layers";
	ot->idname= "ARMATURE_OT_bone_layers";
	ot->description= "Change the layers that the selected bones belong to.";
	
	/* callbacks */
	ot->invoke= armature_bone_layers_invoke;
	ot->exec= armature_bone_layers_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean_array(ot->srna, "layers", 16, NULL, "Layers", "Armature layers that bone belongs to.");
}

/* ********************************************** */

/* for use in insertkey, ensure rotation goes other way around */
void pose_flipquats(Scene *scene)
{
	Object *ob = OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	
	if(ob->pose==NULL)
		return;
	
	/* find sel bones */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone && (pchan->bone->flag & BONE_SELECTED) && (pchan->bone->layer & arm->layer)) {
			/* quaternions have 720 degree range */
			pchan->quat[0]= -pchan->quat[0];
			pchan->quat[1]= -pchan->quat[1];
			pchan->quat[2]= -pchan->quat[2];
			pchan->quat[3]= -pchan->quat[3];
		}
	}
	
	/* do autokey */
	autokeyframe_pose_cb_func(ob, TFM_ROTATION, 0);
}

/* context: active channel */
void pose_special_editmenu(Scene *scene)
{
#if 0
	Object *obedit= scene->obedit; // XXX context
	Object *ob= OBACT;
	short nr;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==obedit || (ob->mode & OB_MODE_POSE)==0) return;
	
	nr= pupmenu("Specials%t|Select Constraint Target%x1|Flip Left-Right Names%x2|Calculate Paths%x3|Clear Paths%x4|Clear User Transform %x5|Relax Pose %x6|%l|AutoName Left-Right%x7|AutoName Front-Back%x8|AutoName Top-Bottom%x9");
	if(nr==1) {
		pose_select_constraint_target(scene);
	}
	else if(nr==2) {
		pose_flip_names();
	}
	else if(nr==3) {
		pose_calculate_path(C, ob);
	}
	else if(nr==4) {
		pose_clear_paths(ob);
	}
	else if(nr==5) {
		pose_clear_user_transforms(scene, ob);
	}
	else if(nr==6) {
		pose_relax();
	}
	else if(ELEM3(nr, 7, 8, 9)) {
		pose_autoside_names(nr-7);
	}
#endif
}

/* Restore selected pose-bones to 'action'-defined pose */
void pose_clear_user_transforms(Scene *scene, Object *ob)
{
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	
	if (ob->pose == NULL)
		return;
	
	/* if the object has an action, restore pose to the pose defined by the action by clearing pose on selected bones */
	if (ob->action) {
		/* find selected bones */
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if (pchan->bone && (pchan->bone->flag & BONE_SELECTED) && (pchan->bone->layer & arm->layer)) {
				/* just clear the BONE_UNKEYED flag, allowing this bone to get overwritten by actions again */
				pchan->bone->flag &= ~BONE_UNKEYED;
			}
		}
		
		/* clear pose locking flag 
		 *	- this will only clear the user-defined pose in the selected bones, where BONE_UNKEYED has been cleared
		 */
		ob->pose->flag |= POSE_DO_UNLOCK;
	}
	else {
		/* no action, so restore entire pose to rest pose (cannot restore only selected bones) */
		rest_pose(ob->pose);
	}
	
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	BIF_undo_push("Clear User Transform");
}

