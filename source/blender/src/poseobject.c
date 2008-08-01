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

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_ipo.h"

#include "BIF_editarmature.h"
#include "BIF_editaction.h"
#include "BIF_editconstraint.h"
#include "BIF_editdeform.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_poseobject.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_screen.h"

#include "BDR_editobject.h"

#include "BSE_edit.h"
#include "BSE_editipo.h"
#include "BSE_trans_types.h"

#include "mydevice.h"
#include "blendef.h"
#include "transform.h"

#include "BIF_transform.h" /* for autokey TFM_TRANSLATION, etc */

void enter_posemode(void)
{
	Base *base;
	Object *ob;
	bArmature *arm;
	
	if(G.scene->id.lib) return;
	base= BASACT;
	if(base==NULL) return;
	
	ob= base->object;
	
	if (ob->id.lib){
		error ("Can't pose libdata");
		return;
	}

	switch (ob->type){
	case OB_ARMATURE:
		arm= get_armature(ob);
		if( arm==NULL ) return;
		
		ob->flag |= OB_POSEMODE;
		base->flag= ob->flag;
		
		allqueue(REDRAWHEADERS, 0);	
		allqueue(REDRAWBUTSALL, 0);	
		allqueue(REDRAWOOPS, 0);
		allqueue(REDRAWVIEW3D, 0);
		break;
	default:
		return;
	}

	if (G.obedit) exit_editmode(EM_FREEDATA|EM_WAITCURSOR);
	G.f &= ~(G_VERTEXPAINT | G_TEXTUREPAINT | G_WEIGHTPAINT | G_SCULPTMODE);
}

void set_pose_keys (Object *ob)
{
	bArmature *arm= ob->data;
	bPoseChannel *chan;

	if (ob->pose){
		for (chan=ob->pose->chanbase.first; chan; chan=chan->next){
			Bone *bone= chan->bone;
			if(bone && (bone->flag & BONE_SELECTED) && (arm->layer & bone->layer)) {
				chan->flag |= POSE_KEY;		
			}
			else {
				chan->flag &= ~POSE_KEY;
			}
		}
	}
}


void exit_posemode(void)
{
	Object *ob= OBACT;
	Base *base= BASACT;

	if(ob==NULL) return;
	
	ob->flag &= ~OB_POSEMODE;
	base->flag= ob->flag;
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWHEADERS, 0);	
	allqueue(REDRAWBUTSALL, 0);	

	scrarea_queue_headredraw(curarea);
}

/* called by buttons to find a bone to display/edit values for */
bPoseChannel *get_active_posechannel (Object *ob)
{
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	
	if ELEM(NULL, ob, ob->pose)
		return NULL;
	
	/* find active */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone && (pchan->bone->flag & BONE_ACTIVE) && (pchan->bone->layer & arm->layer))
			return pchan;
	}
	
	return NULL;
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
int pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan)
{
	bConstraint *con;
	Bone *bone;
	
	for(con= pchan->constraints.first; con; con= con->next) {
		if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
			bKinematicConstraint *data= con->data;
			if((data->flag & CONSTRAINT_IK_AUTO)==0)
				return 1;
		}
	}
	for(bone= pchan->bone->childbase.first; bone; bone= bone->next) {
		pchan= get_pose_channel(ob->pose, bone->name);
		if(pchan && pose_channel_in_IK_chain(ob, pchan))
			return 1;
	}
	return 0;
}

/* ********************************************** */

/* For the object with pose/action: create path curves for selected bones 
 * This recalculates the WHOLE path within the pchan->pathsf and pchan->pathef range
 */
void pose_calculate_path(Object *ob)
{
	bArmature *arm;
	bPoseChannel *pchan;
	Base *base;
	float *fp;
	int cfra;
	int sfra, efra;
	
	if (ob==NULL || ob->pose==NULL)
		return;
	arm= ob->data;
	
	/* version patch for older files here (do_versions patch too complicated) */
	if ((arm->pathsf == 0) || (arm->pathef == 0)) {
		arm->pathsf = SFRA;
		arm->pathef = EFRA;
	}
	if (arm->pathsize == 0) {
		arm->pathsize = 1;
	}
	
	/* set frame values */
	cfra= CFRA;
	sfra = arm->pathsf;
	efra = arm->pathef;
	if (efra <= sfra) {
		error("Can't calculate paths when pathlen <= 0");
		return;
	}
	
	waitcursor(1);
	
	/* hack: for unsaved files, set OB_RECALC so that paths can get calculated */
	if ((ob->recalc & OB_RECALC)==0) {
		ob->recalc |= OB_RECALC;
		DAG_object_update_flags(G.scene, ob, screen_view3d_layers());
	}
	else
		DAG_object_update_flags(G.scene, ob, screen_view3d_layers());
	
	
	/* malloc the path blocks */
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
	
	for (CFRA=sfra; CFRA<=efra; CFRA++) {
		/* do all updates */
		for (base= FIRSTBASE; base; base= base->next) {
			if (base->object->recalc) {
				int temp= base->object->recalc;
				object_handle_update(base->object);
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
	
	waitcursor(0);
	
	CFRA= cfra;
	allqueue(REDRAWVIEW3D, 0);	/* recalc tags are still there */
	allqueue(REDRAWBUTSEDIT, 0);
}

/* For the object with pose/action: update paths for those that have got them
 * This should selectively update paths that exist...
 */
void pose_recalculate_paths(Object *ob)
{
	bArmature *arm;
	bPoseChannel *pchan;
	Base *base;
	float *fp;
	int cfra;
	int sfra, efra;
	
	if (ob==NULL || ob->pose==NULL)
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
	
	waitcursor(1);
	
	/* hack: for unsaved files, set OB_RECALC so that paths can get calculated */
	if ((ob->recalc & OB_RECALC)==0) {
		ob->recalc |= OB_RECALC;
		DAG_object_update_flags(G.scene, ob, screen_view3d_layers());
	}
	else
		DAG_object_update_flags(G.scene, ob, screen_view3d_layers());
	
	for (CFRA=sfra; CFRA<=efra; CFRA++) {
		/* do all updates */
		for (base= FIRSTBASE; base; base= base->next) {
			if (base->object->recalc) {
				int temp= base->object->recalc;
				object_handle_update(base->object);
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
	
	waitcursor(0);
	
	CFRA= cfra;
	allqueue(REDRAWVIEW3D, 0);	/* recalc tags are still there */
	allqueue(REDRAWBUTSEDIT, 0);
}

/* for the object with pose/action: clear path curves for selected bones only */
void pose_clear_paths(Object *ob)
{
	bPoseChannel *pchan;
	
	if (ob==NULL || ob->pose==NULL)
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
	
	allqueue(REDRAWVIEW3D, 0);
}



void pose_select_constraint_target(void)
{
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	bConstraint *con;
	
	/* paranoia checks */
	if (!ob && !ob->pose) return;
	if (ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
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
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWOOPS, 0);
	
	BIF_undo_push("Select constraint target");

}

/* context: active channel */
void pose_special_editmenu(void)
{
	Object *ob= OBACT;
	short nr;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	nr= pupmenu("Specials%t|Select Constraint Target%x1|Flip Left-Right Names%x2|Calculate Paths%x3|Clear Paths%x4|Clear User Transform %x5|Relax Pose %x6|%l|AutoName Left-Right%x7|AutoName Front-Back%x8|AutoName Top-Bottom%x9");
	if(nr==1) {
		pose_select_constraint_target();
	}
	else if(nr==2) {
		pose_flip_names();
	}
	else if(nr==3) {
		pose_calculate_path(ob);
	}
	else if(nr==4) {
		pose_clear_paths(ob);
	}
	else if(nr==5) {
		rest_pose(ob->pose);
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		BIF_undo_push("Clear User Transform Pose");
	}
	else if(nr==6) {
		pose_relax();
	}
	else if(ELEM3(nr, 7, 8, 9)) {
		pose_autoside_names(nr-7);
	}
}

void pose_add_IK(void)
{
	Object *ob= OBACT;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	add_constraint(1);	/* 1 means only IK */
}

/* context: all selected channels */
void pose_clear_IK(void)
{
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	bConstraint *con;
	bConstraint *next;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	if(pose_has_protected_selected(ob, 0, 1))
		return;
	
	if(okee("Remove IK constraint(s)")==0) return;

	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(arm->layer & pchan->bone->layer) {
			if(pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
				
				for(con= pchan->constraints.first; con; con= next) {
					next= con->next;
					if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
						BLI_remlink(&pchan->constraints, con);
						free_constraint_data(con);
						MEM_freeN(con);
					}
				}
				pchan->constflag &= ~(PCHAN_HAS_IK|PCHAN_HAS_TARGET);
			}
		}
	}
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);	// and all its relations
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWOOPS, 0);
	
	BIF_undo_push("Remove IK constraint(s)");
}

void pose_clear_constraints(void)
{
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	if(pose_has_protected_selected(ob, 0, 1))
		return;
	
	if(okee("Remove Constraints")==0) return;
	
	/* find active */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(arm->layer & pchan->bone->layer) {
			if(pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
				free_constraints(&pchan->constraints);
				pchan->constflag= 0;
			}
		}
	}
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);	// and all its relations
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWOOPS, 0);
	
	BIF_undo_push("Remove Constraint(s)");
	
}


void pose_copy_menu(void)
{
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan, *pchanact;
	short nr=0;
	int i=0;
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) return;
	if ((ob==G.obedit) || (ob->flag & OB_POSEMODE)==0) return;
	
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
						float delta_mat[4][4], quat[4];
						
						armature_mat_pose_to_bone(pchan, pchanact->pose_mat, delta_mat);
						Mat4ToQuat(delta_mat, quat);
						QUATCOPY(pchan->quat, quat);
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
		
		duplicatelist(&const_copy, &(pchanact->constraints));
		
		/* build the puplist of constraints */
		for (con = pchanact->constraints.first, i=0; con; con=con->next, i++){
			const_toggle[i]= 1;
			add_numbut(i, TOG|INT, con->name, 0, 0, &(const_toggle[i]), "");
		}
		
		if (!do_clever_numbuts("Select Constraints", i, REDRAW)) {
			BLI_freelistN(&const_copy);
			return;
		}
		
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
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);	// and all its relations
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWOOPS, 0);
	
	BIF_undo_push("Copy Pose Attributes");
	
}

/* ******************** copy/paste pose ********************** */

static bPose	*g_posebuf=NULL;

void free_posebuf(void) 
{
	if (g_posebuf) {
		// was copied without constraints
		BLI_freelistN (&g_posebuf->chanbase);
		MEM_freeN (g_posebuf);
	}
	g_posebuf=NULL;
}

void copy_posebuf (void)
{
	Object *ob= OBACT;

	if (!ob || !ob->pose){
		error ("No Pose");
		return;
	}

	free_posebuf();
	
	set_pose_keys(ob);  // sets chan->flag to POSE_KEY if bone selected
	copy_pose(&g_posebuf, ob->pose, 0);

}

void paste_posebuf (int flip)
{
	Object *ob= OBACT;
	bPoseChannel *chan, *pchan;
	float eul[4];
	char name[32];
	
	if (!ob || !ob->pose)
		return;

	if (!g_posebuf){
		error ("Copy buffer is empty");
		return;
	}
	
	/*
	// disabled until protected bones in proxies follow the rules everywhere else!
	if(pose_has_protected_selected(ob, 1, 1))
		return;
	*/
	
	/* Safely merge all of the channels in this pose into
	any existing pose */
	for (chan=g_posebuf->chanbase.first; chan; chan=chan->next) {
		if (chan->flag & POSE_KEY) {
			BLI_strncpy(name, chan->name, sizeof(name));
			if (flip)
				bone_flip_name (name, 0);		// 0 = don't strip off number extensions
				
			/* only copy when channel exists, poses are not meant to add random channels to anymore */
			pchan= get_pose_channel(ob->pose, name);
			
			if (pchan) {
				/* only loc rot size */
				/* only copies transform info for the pose */
				VECCOPY(pchan->loc, chan->loc);
				VECCOPY(pchan->size, chan->size);
				QUATCOPY(pchan->quat, chan->quat);
				pchan->flag= chan->flag;
				
				if (flip) {
					pchan->loc[0]*= -1;
					
					QuatToEul(pchan->quat, eul);
					eul[1]*= -1;
					eul[2]*= -1;
					EulToQuat(eul, pchan->quat);
				}
				
				if (autokeyframe_cfra_can_key(ob)) {
					ID *id= &ob->id;
					
					/* Set keys on pose */
					if (chan->flag & POSE_ROT) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_Z, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_QUAT_W, 0);
					}
					if (chan->flag & POSE_SIZE) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_SIZE_Z, 0);
					}
					if (chan->flag & POSE_LOC) {
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_X, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Y, 0);
						insertkey(id, ID_PO, pchan->name, NULL, AC_LOC_Z, 0);
					}
					
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
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	if (IS_AUTOKEY_ON) {
		remake_action_ipos(ob->action);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);		
		allqueue(REDRAWNLA, 0);
	}
	else {
		/* need to trick depgraph, action is not allowed to execute on pose */
		where_is_pose(ob);
		ob->recalc= 0;
	}

	BIF_undo_push("Paste Action Pose");
}

/* ********************************************** */

/* context weightpaint and deformer in posemode */
void pose_adds_vgroups(Object *meshobj, int heatweights)
{
	extern VPaint Gwp;         /* from vpaint */
	Object *poseobj= modifiers_isDeformedByArmature(meshobj);

	if(poseobj==NULL || (poseobj->flag & OB_POSEMODE)==0) {
		error("The active object must have a deforming armature in pose mode");
		return;
	}

	add_verts_to_dgroups(meshobj, poseobj, heatweights, (Gwp.flag & VP_MIRROR_X));

	if(heatweights)
		BIF_undo_push("Apply Bone Heat Weights to Vertex Groups");
	else
		BIF_undo_push("Apply Bone Envelopes to Vertex Groups");

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	
	// and all its relations
	DAG_object_flush_update(G.scene, meshobj, OB_RECALC_DATA);
}

/* ********************************************** */

/* adds a new pose-group */
void pose_add_posegroup ()
{
	Object *ob= OBACT;
	bPose *pose= (ob) ? ob->pose : NULL;
	bActionGroup *grp;
	
	if (ELEM(NULL, ob, ob->pose))
		return;
	
	grp= MEM_callocN(sizeof(bActionGroup), "PoseGroup");
	strcpy(grp->name, "Group");
	BLI_addtail(&pose->agroups, grp);
	BLI_uniquename(&pose->agroups, grp, "Group", offsetof(bActionGroup, name), 32);
	
	pose->active_group= BLI_countlist(&pose->agroups);
	
	BIF_undo_push("Add Bone Group");
	
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
}

/* Remove the active bone-group */
void pose_remove_posegroup ()
{
	Object *ob= OBACT;
	bPose *pose= (ob) ? ob->pose : NULL;
	bActionGroup *grp = NULL;
	bPoseChannel *pchan;
	
	/* sanity checks */
	if (ELEM(NULL, ob, pose))
		return;
	if (pose->active_group <= 0)
		return;
	
	/* get group to remove */
	grp= BLI_findlink(&pose->agroups, pose->active_group-1);
	if (grp) {
		/* firstly, make sure nothing references it */
		for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
			if (pchan->agrp_index == pose->active_group)
				pchan->agrp_index= 0;
		}
		
		/* now, remove it from the pose */
		BLI_freelinkN(&pose->agroups, grp);
		pose->active_group= 0;
		
		BIF_undo_push("Remove Bone Group");
	}
	
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
}

char *build_posegroups_menustr (bPose *pose, short for_pupmenu)
{
	DynStr *pupds= BLI_dynstr_new();
	bActionGroup *grp;
	char *str;
	char buf[16];
	int i;
	
	/* add title first (and the "none" entry) */
	BLI_dynstr_append(pupds, "Bone Group%t|");
	if (for_pupmenu)
		BLI_dynstr_append(pupds, "Add New%x0|");
	else
		BLI_dynstr_append(pupds, "BG: [None]%x0|");
	
	/* loop through groups, adding them */
	for (grp= pose->agroups.first, i=1; grp; grp=grp->next, i++) {
		if (for_pupmenu == 0)
			BLI_dynstr_append(pupds, "BG: ");
		BLI_dynstr_append(pupds, grp->name);
		
		sprintf(buf, "%%x%d", i);
		BLI_dynstr_append(pupds, buf);
		
		if (grp->next)
			BLI_dynstr_append(pupds, "|");
	}
	
	/* convert to normal MEM_malloc'd string */
	str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
	
	return str;
}

/* Assign selected pchans to the bone group that the user selects */
void pose_assign_to_posegroup (short active)
{
	Object *ob= OBACT;
	bArmature *arm= (ob) ? ob->data : NULL;
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan;
	char *menustr;
	int nr;
	short done= 0;
	
	/* sanity checks */
	if (ELEM3(NULL, ob, pose, arm))
		return;

	/* get group to affect */
	if ((active==0) || (pose->active_group <= 0)) {
		menustr= build_posegroups_menustr(pose, 1);
		nr= pupmenu_col(menustr, 20);
		MEM_freeN(menustr);
		
		if (nr < 0) 
			return;
		else if (nr == 0) {
			/* add new - note: this does an undo push and sets active group */
			pose_add_posegroup();
		}
		else
			pose->active_group= nr;
	}
	
	/* add selected bones to group then */
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		if ((pchan->bone->flag & BONE_SELECTED) && (pchan->bone->layer & arm->layer)) {
			pchan->agrp_index= pose->active_group;
			done= 1;
		}
	}
	
	if (done)
		BIF_undo_push("Add Bones To Group");
		
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
}

/* Remove selected pchans from their bone groups */
void pose_remove_from_posegroups ()
{
	Object *ob= OBACT;
	bArmature *arm= (ob) ? ob->data : NULL;
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan;
	short done= 0;
	
	/* sanity checks */
	if (ELEM3(NULL, ob, pose, arm))
		return;
	
	/* remove selected bones from their groups */
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		if ((pchan->bone->flag & BONE_SELECTED) && (pchan->bone->layer & arm->layer)) {
			if (pchan->agrp_index) {
				pchan->agrp_index= 0;
				done= 1;
			}
		}
	}
	
	if (done)
		BIF_undo_push("Remove Bones From Groups");
		
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWVIEW3D, 0);
}

/* Ctrl-G in 3D-View while in PoseMode */
void pgroup_operation_with_menu (void)
{
	Object *ob= OBACT;
	bArmature *arm= (ob) ? ob->data : NULL;
	bPose *pose= (ob) ? ob->pose : NULL;
	bPoseChannel *pchan= NULL;
	int mode;
	
	/* sanity checks */
	if (ELEM3(NULL, ob, pose, arm))
		return;
	
	/* check that something is selected */
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		if ((pchan->bone->flag & BONE_SELECTED) && (pchan->bone->layer & arm->layer)) 
			break;
	}
	if (pchan == NULL)
		return;
	
	/* get mode of action */
	if (pchan)
		mode= pupmenu("Bone Groups%t|Add Selected to Active Group%x1|Add Selected to Group%x2|%|Remove Selected From Groups%x3|Remove Active Group%x4");
	else
		mode= pupmenu("Bone Groups%t|Add New Group%x5|Remove Active Group%x4");
		
	/* handle mode */
	switch (mode) {
		case 1:
			pose_assign_to_posegroup(1);
			break;
		case 2:
			pose_assign_to_posegroup(0);
			break;
		case 5:
			pose_add_posegroup();
			break;
		case 3:
			pose_remove_from_posegroups();
			break;
		case 4:
			pose_remove_posegroup();
			break;
	}
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


void pose_select_grouped (short nr)
{
	short changed = 0;
	
	if (nr == 1) 		changed= pose_select_same_group(OBACT);
	else if (nr == 2)	changed= pose_select_same_layer(OBACT);
	
	if (changed) {
		countall();
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		allspace(REMAKEIPO, 0);
		allqueue(REDRAWIPO, 0);
		allqueue(REDRAWACTION, 0);
		BIF_undo_push("Select Grouped");
	}
}

/* Shift-G in 3D-View while in PoseMode */
void pose_select_grouped_menu (void)
{
	short nr;
	
	/* here we go */
	nr= pupmenu("Select Grouped%t|In Same Group%x1|In Same Layer%x2");
	pose_select_grouped(nr);
}

/* ********************************************** */

/* context active object */
void pose_flip_names(void)
{
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	char newname[32];
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	if(pose_has_protected_selected(ob, 0, 1))
		return;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(arm->layer & pchan->bone->layer) {
			if(pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
				BLI_strncpy(newname, pchan->name, sizeof(newname));
				bone_flip_name(newname, 1);	// 1 = do strip off number extensions
				armature_bone_rename(ob->data, pchan->name, newname);
			}
		}
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWOOPS, 0);
	BIF_undo_push("Flip names");
}

/* context active object */
void pose_autoside_names(short axis)
{
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	char newname[32];
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) return;
	if (ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	if (pose_has_protected_selected(ob, 0, 1))
		return;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(arm->layer & pchan->bone->layer) {
			if(pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
				BLI_strncpy(newname, pchan->name, sizeof(newname));
				bone_autoside_name(newname, 1, axis, pchan->bone->head[axis], pchan->bone->tail[axis]);
				armature_bone_rename(ob->data, pchan->name, newname);
			}
		}
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWOOPS, 0);
	BIF_undo_push("Flip names");
}

/* context active object, or weightpainted object with armature in posemode */
void pose_activate_flipped_bone(void)
{
	Object *ob= OBACT;
	bArmature *arm= ob->data;
	
	if(ob==NULL) return;

	if(G.f & G_WEIGHTPAINT) {
		ob= modifiers_isDeformedByArmature(ob);
	}
	if(ob && (ob->flag & OB_POSEMODE)) {
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
				if(G.f & G_WEIGHTPAINT) {
					vertexgroup_select_by_name(OBACT, name);
					DAG_object_flush_update(G.scene, OBACT, OB_RECALC_DATA);
				}
				
				select_actionchannel_by_name(ob->action, name, 1);
				
				allqueue(REDRAWVIEW3D, 0);
				allqueue(REDRAWACTION, 0);
				allqueue(REDRAWIPO, 0);		/* To force action/constraint ipo update */
				allqueue(REDRAWBUTSEDIT, 0);
				allqueue(REDRAWBUTSOBJECT, 0);
				allqueue(REDRAWOOPS, 0);
			}			
		}
	}
}

/* This function pops up the move-to-layer popup widgets when the user
 * presses either SHIFT-MKEY or MKEY in PoseMode OR EditMode (for Armatures)
 */
void pose_movetolayer(void)
{
	Object *ob= OBACT;
	bArmature *arm;
	short lay= 0;
	
	if (ob==NULL) return;
	arm= ob->data;
	
	if (G.qual & LR_SHIFTKEY) {
		/* armature layers */
		lay= arm->layer;
		if ( movetolayer_short_buts(&lay, "Armature Layers")==0 ) return;
		if (lay==0) return;
		arm->layer= lay;
		if(ob->pose)
			ob->pose->proxy_layer= lay;
		
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWBUTSEDIT, 0);
	}
	else if (G.obedit) {
		/* the check for editbone layer moving needs to occur before posemode one to work */
		EditBone *ebo;
		EditBone *flipBone;
		
		for (ebo= G.edbo.first; ebo; ebo= ebo->next) {
			if (arm->layer & ebo->layer) {
				if (ebo->flag & BONE_SELECTED)
					lay |= ebo->layer;
			}
		}
		if (lay==0) return;
		
		if ( movetolayer_short_buts(&lay, "Bone Layers")==0 ) return;
		if (lay==0) return;
		
		for (ebo= G.edbo.first; ebo; ebo= ebo->next) {
			if (arm->layer & ebo->layer) {
				if (ebo->flag & BONE_SELECTED) {
					ebo->layer= lay;
					if (arm->flag & ARM_MIRROR_EDIT) {
						flipBone = armature_bone_get_mirrored(ebo);
						if (flipBone)
							flipBone->layer = lay;
					}
				}
			}
		}
		
		BIF_undo_push("Move Bone Layer");
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
	}
	else if (ob->flag & OB_POSEMODE) {
		/* pose-channel layers */
		bPoseChannel *pchan;
		
		if (pose_has_protected_selected(ob, 0, 1))
			return;
		
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if (arm->layer & pchan->bone->layer) {
				if (pchan->bone->flag & BONE_SELECTED)
					lay |= pchan->bone->layer;
			}
		}
		if (lay==0) return;
		
		if ( movetolayer_short_buts(&lay, "Bone Layers")==0 ) return;
		if (lay==0) return;
		
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if (arm->layer & pchan->bone->layer) {
				if (pchan->bone->flag & BONE_SELECTED)
					pchan->bone->layer= lay;
			}
		}
		
		BIF_undo_push("Move Bone Layer");
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWBUTSEDIT, 0);
	}
}


/* for use with pose_relax only */
static int pose_relax_icu(struct IpoCurve *icu, float framef, float *val, float *frame_prev, float *frame_next)
{
	if (!icu) {
		return 0;
	} 
	else {
		BezTriple *bezt = icu->bezt;
		
		BezTriple *bezt_prev=NULL, *bezt_next=NULL;
		float w1, w2, wtot;
		int i;
		
		for (i=0; i < icu->totvert; i++, bezt++) {
			if (bezt->vec[1][0] < framef - 0.5) {
				bezt_prev = bezt;
			} else {
				break;
			}
		}
		
		if (bezt_prev==NULL) return 0;
		
		/* advance to the next, dont need to advance i */
		bezt = bezt_prev+1;
		
		for (; i < icu->totvert; i++, bezt++) {
			if (bezt->vec[1][0] > framef + 0.5) {
				bezt_next = bezt;
						break;
			}
		}
		
		if (bezt_next==NULL) return 0;
	
		if (val) {
			w1 = framef - bezt_prev->vec[1][0];
			w2 = bezt_next->vec[1][0] - framef;
			wtot = w1 + w2;
			w1=w1/wtot;
			w2=w2/wtot;
#if 0
			val = (bezt_prev->vec[1][1] * w2) + (bezt_next->vec[1][1] * w1);
#else
			/* apply the value with a hard coded 6th */
			*val = (((bezt_prev->vec[1][1] * w2) + (bezt_next->vec[1][1] * w1)) + (*val * 5.0f)) / 6.0f;
#endif
		}
		
		if (frame_prev)	*frame_prev = bezt_prev->vec[1][0];
		if (frame_next)	*frame_next = bezt_next->vec[1][0];
		
		return 1;
	}
}

void pose_relax()
{
	Object *ob = OBACT;
	bPose *pose;
	bAction *act;
	bArmature *arm;
	
	IpoCurve *icu_w, *icu_x, *icu_y, *icu_z;
	
	bPoseChannel *pchan;
	bActionChannel *achan;
	float framef = F_CFRA;
	float frame_prev, frame_next;
	float quat_prev[4], quat_next[4], quat_interp[4], quat_orig[4];
	
	int do_scale = 0;
	int do_loc = 0;
	int do_quat = 0;
	int flag = 0;
	int do_x, do_y, do_z;
	
	if (!ob) return;
	
	pose = ob->pose;
	act = ob->action;
	arm = (bArmature *)ob->data;
	
	if (!pose || !act || !arm) return;
	
	for (pchan=pose->chanbase.first; pchan; pchan= pchan->next) {
		
		pchan->bone->flag &= ~BONE_TRANSFORM;
		
		if (pchan->bone->layer & arm->layer) {
			if (pchan->bone->flag & BONE_SELECTED) {
				/* do we have an ipo curve? */
				achan= get_action_channel(act, pchan->name);
				
				if (achan && achan->ipo) {
					/*calc_ipo(achan->ipo, ctime);*/
					
					do_x = pose_relax_icu(find_ipocurve(achan->ipo, AC_LOC_X), framef, &pchan->loc[0], NULL, NULL);
					do_y = pose_relax_icu(find_ipocurve(achan->ipo, AC_LOC_Y), framef, &pchan->loc[1], NULL, NULL);
					do_z = pose_relax_icu(find_ipocurve(achan->ipo, AC_LOC_Z), framef, &pchan->loc[2], NULL, NULL);
					do_loc += do_x + do_y + do_z;
					
					do_x = pose_relax_icu(find_ipocurve(achan->ipo, AC_SIZE_X), framef, &pchan->size[0], NULL, NULL);
					do_y = pose_relax_icu(find_ipocurve(achan->ipo, AC_SIZE_Y), framef, &pchan->size[1], NULL, NULL);
					do_z = pose_relax_icu(find_ipocurve(achan->ipo, AC_SIZE_Z), framef, &pchan->size[2], NULL, NULL);
					do_scale += do_x + do_y + do_z;
						
					if(	((icu_w = find_ipocurve(achan->ipo, AC_QUAT_W))) &&
						((icu_x = find_ipocurve(achan->ipo, AC_QUAT_X))) &&
						((icu_y = find_ipocurve(achan->ipo, AC_QUAT_Y))) &&
						((icu_z = find_ipocurve(achan->ipo, AC_QUAT_Z))) )
					{
						/* use the quatw keyframe as a basis for others */
						if (pose_relax_icu(icu_w, framef, NULL, &frame_prev, &frame_next)) {
							/* get 2 quats */
							quat_prev[0] = eval_icu(icu_w, frame_prev);
							quat_prev[1] = eval_icu(icu_x, frame_prev);
							quat_prev[2] = eval_icu(icu_y, frame_prev);
							quat_prev[3] = eval_icu(icu_z, frame_prev);
							
							quat_next[0] = eval_icu(icu_w, frame_next);
							quat_next[1] = eval_icu(icu_x, frame_next);
							quat_next[2] = eval_icu(icu_y, frame_next);
							quat_next[3] = eval_icu(icu_z, frame_next);
							
#if 0
							/* apply the setting, completely smooth */
							QuatInterpol(pchan->quat, quat_prev, quat_next, (framef-frame_prev) / (frame_next-frame_prev) );
#else
							/* tricky interpolation */
							QuatInterpol(quat_interp, quat_prev, quat_next, (framef-frame_prev) / (frame_next-frame_prev) );
							QUATCOPY(quat_orig, pchan->quat);
							QuatInterpol(pchan->quat, quat_orig, quat_interp, 1.0f/6.0f);
							/* done */
#endif
							do_quat++;
						}
					}
					
					/* apply BONE_TRANSFORM tag so that autokeying will pick it up */
					pchan->bone->flag |= BONE_TRANSFORM;
				}
			}
		}
	}
	
	ob->pose->flag |= (POSE_LOCKED|POSE_DO_UNLOCK);
	
	/* do auto-keying */
	if (do_loc)		flag |= TFM_TRANSLATION;
	if (do_scale)	flag |= TFM_RESIZE;
	if (do_quat)	flag |= TFM_ROTATION;
	autokeyframe_pose_cb_func(ob, flag, 0);
	 
	/* clear BONE_TRANSFORM flags */
	for (pchan=pose->chanbase.first; pchan; pchan= pchan->next)
		pchan->bone->flag &= ~ BONE_TRANSFORM;
	
	/* do depsgraph flush */
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	BIF_undo_push("Relax Pose");
}

/* for use in insertkey, ensure rotation goes other way around */
void pose_flipquats(void)
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

