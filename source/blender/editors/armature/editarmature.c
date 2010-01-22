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
 * Contributor(s): Blender Foundation, 2002-2009 full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 
#include <float.h> 

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"
#include "DNA_modifier_types.h"
#include "DNA_ipo_types.h"
#include "DNA_curve_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_idprop.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_subsurf.h"
#include "BKE_utildefines.h"
#include "BKE_modifier.h"
#include "PIL_time.h"

#include "BIF_gl.h"
#include "BIF_generate.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_screen.h"
#include "ED_transform.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "UI_interface.h"

#include "armature_intern.h"
#include "meshlaplacian.h"

#if 0
#include "reeb.h"
#endif

/* ************* XXX *************** */
static int okee() {return 0;}
static void BIF_undo_push(const char *msg) {}
/* ************* XXX *************** */

/* **************** tools on Editmode Armature **************** */

/* Sync selection to parent for connected children */
void ED_armature_sync_selection(ListBase *edbo)
{
	EditBone *ebo;
	
	for (ebo=edbo->first; ebo; ebo= ebo->next) {
		/* if bone is not selectable, we shouldn't alter this setting... */
		if ((ebo->flag & BONE_UNSELECTABLE) == 0) {
			if ((ebo->flag & BONE_CONNECTED) && (ebo->parent)) {
				if (ebo->parent->flag & BONE_TIPSEL)
					ebo->flag |= BONE_ROOTSEL;
				else
					ebo->flag &= ~BONE_ROOTSEL;
			}
			
			if ((ebo->flag & BONE_TIPSEL) && (ebo->flag & BONE_ROOTSEL))
				ebo->flag |= BONE_SELECTED;
			else
				ebo->flag &= ~BONE_SELECTED;
		}
	}				
}

void ED_armature_validate_active(struct bArmature *arm)
{
	EditBone *ebone= arm->act_edbone;

	if(ebone) { 
		if(ebone->flag & BONE_HIDDEN_A || (ebone->flag & BONE_SELECTED)==0)
			arm->act_edbone= NULL;
	}
}

static void bone_free(bArmature *arm, EditBone *bone)
{
	if(arm->act_edbone==bone)
		arm->act_edbone= NULL;

	if(bone->prop) {
		IDP_FreeProperty(bone->prop);
		MEM_freeN(bone->prop);
	}

	BLI_freelinkN(arm->edbo, bone);
}

void ED_armature_edit_bone_remove(bArmature *arm, EditBone* exBone)
{
	EditBone *curBone;

	/* Find any bones that refer to this bone */
	for (curBone=arm->edbo->first; curBone; curBone=curBone->next) {
		if (curBone->parent==exBone) {
			curBone->parent=exBone->parent;
			curBone->flag &= ~BONE_CONNECTED;
		}
	}

	bone_free(arm, exBone);
}


/* converts Bones to EditBone list, used for tools as well */
EditBone *make_boneList(ListBase *edbo, ListBase *bones, EditBone *parent, Bone *actBone)
{
	EditBone	*eBone;
	EditBone	*eBoneAct= NULL;
	EditBone	*eBoneTest= NULL;
	Bone		*curBone;
	float delta[3];
	float premat[3][3];
	float postmat[3][3];
	float imat[3][3];
	float difmat[3][3];
		
	for (curBone=bones->first; curBone; curBone=curBone->next) {
		eBone= MEM_callocN(sizeof(EditBone), "make_editbone");
		
		/*	Copy relevant data from bone to eBone */
		eBone->parent= parent;
		BLI_strncpy(eBone->name, curBone->name, 32);
		eBone->flag = curBone->flag;
		
		/* fix selection flags */
		if (eBone->flag & BONE_SELECTED) {
			eBone->flag |= BONE_TIPSEL;
			if (eBone->parent && (eBone->flag & BONE_CONNECTED))
				eBone->parent->flag |= BONE_TIPSEL;
			else 
				eBone->flag |= BONE_ROOTSEL;
		}
		else 
			eBone->flag &= ~BONE_ROOTSEL;
		
		VECCOPY(eBone->head, curBone->arm_head);
		VECCOPY(eBone->tail, curBone->arm_tail);		
		
		eBone->roll= 0.0f;
		
		/* roll fixing */
		sub_v3_v3v3(delta, eBone->tail, eBone->head);
		vec_roll_to_mat3(delta, 0.0f, postmat);
		
		copy_m3_m4(premat, curBone->arm_mat);
		
		invert_m3_m3(imat, postmat);
		mul_m3_m3m3(difmat, imat, premat);
		
		eBone->roll = (float)atan2(difmat[2][0], difmat[2][2]);
		
		/* rest of stuff copy */
		eBone->length= curBone->length;
		eBone->dist= curBone->dist;
		eBone->weight= curBone->weight;
		eBone->xwidth= curBone->xwidth;
		eBone->zwidth= curBone->zwidth;
		eBone->ease1= curBone->ease1;
		eBone->ease2= curBone->ease2;
		eBone->rad_head= curBone->rad_head;
		eBone->rad_tail= curBone->rad_tail;
		eBone->segments = curBone->segments;		
		eBone->layer = curBone->layer;

		if(curBone->prop)
			eBone->prop= IDP_CopyProperty(curBone->prop);
		
		BLI_addtail(edbo, eBone);
		
		/*	Add children if necessary */
		if (curBone->childbase.first) {
			eBoneTest= make_boneList(edbo, &curBone->childbase, eBone, actBone);
			if(eBoneTest)
				eBoneAct= eBoneTest;
		}

		if(curBone==actBone)
			eBoneAct= eBone;
	}

	return eBoneAct;
}

/* nasty stuff for converting roll in editbones into bones */
/* also sets restposition in armature (arm_mat) */
static void fix_bonelist_roll (ListBase *bonelist, ListBase *editbonelist)
{
	Bone *curBone;
	EditBone *ebone;
	float premat[3][3];
	float postmat[3][3];
	float difmat[3][3];
	float imat[3][3];
	float delta[3];
	
	for (curBone=bonelist->first; curBone; curBone=curBone->next) {
		/* sets local matrix and arm_mat (restpos) */
		where_is_armature_bone(curBone, curBone->parent);
		
		/* Find the associated editbone */
		for (ebone = editbonelist->first; ebone; ebone=ebone->next)
			if ((Bone*)ebone->temp == curBone)
				break;
		
		if (ebone) {
			/* Get the ebone premat */
			sub_v3_v3v3(delta, ebone->tail, ebone->head);
			vec_roll_to_mat3(delta, ebone->roll, premat);
			
			/* Get the bone postmat */
			copy_m3_m4(postmat, curBone->arm_mat);
			
			invert_m3_m3(imat, premat);
			mul_m3_m3m3(difmat, imat, postmat);
#if 0
			printf ("Bone %s\n", curBone->name);
			print_m4("premat", premat);
			print_m4("postmat", postmat);
			print_m4("difmat", difmat);
			printf ("Roll = %f\n",  (-atan2(difmat[2][0], difmat[2][2]) * (180.0/M_PI)));
#endif
			curBone->roll = (float)-atan2(difmat[2][0], difmat[2][2]);
			
			/* and set restposition again */
			where_is_armature_bone(curBone, curBone->parent);
		}
		fix_bonelist_roll(&curBone->childbase, editbonelist);
	}
}

/* put EditMode back in Object */
void ED_armature_from_edit(Object *obedit)
{
	bArmature *arm= obedit->data;
	EditBone *eBone, *neBone;
	Bone	*newBone;
	Object *obt;
	
	/* armature bones */
	free_bonelist(&arm->bonebase);
	
	/* remove zero sized bones, this gives instable restposes */
	for (eBone=arm->edbo->first; eBone; eBone= neBone) {
		float len= len_v3v3(eBone->head, eBone->tail);
		neBone= eBone->next;
		if (len <= 0.000001f) {		/* FLT_EPSILON is too large? */
			EditBone *fBone;
			
			/*	Find any bones that refer to this bone	*/
			for (fBone=arm->edbo->first; fBone; fBone= fBone->next) {
				if (fBone->parent==eBone)
					fBone->parent= eBone->parent;
			}
			if (G.f & G_DEBUG)
				printf("Warning: removed zero sized bone: %s\n", eBone->name);
			bone_free(arm, eBone);
		}
	}
	
	/*	Copy the bones from the editData into the armature */
	for (eBone=arm->edbo->first; eBone; eBone=eBone->next) {
		newBone= MEM_callocN(sizeof(Bone), "bone");
		eBone->temp= newBone;	/* Associate the real Bones with the EditBones */
		
		BLI_strncpy(newBone->name, eBone->name, 32);
		memcpy(newBone->head, eBone->head, sizeof(float)*3);
		memcpy(newBone->tail, eBone->tail, sizeof(float)*3);
		newBone->flag= eBone->flag;
		
		if (eBone == arm->act_edbone) {
			newBone->flag |= BONE_SELECTED;	/* important, editbones can be active with only 1 point selected */
			arm->act_edbone= NULL;
			arm->act_bone= newBone;
		}
		newBone->roll = 0.0f;
		
		newBone->weight = eBone->weight;
		newBone->dist = eBone->dist;
		
		newBone->xwidth = eBone->xwidth;
		newBone->zwidth = eBone->zwidth;
		newBone->ease1= eBone->ease1;
		newBone->ease2= eBone->ease2;
		newBone->rad_head= eBone->rad_head;
		newBone->rad_tail= eBone->rad_tail;
		newBone->segments= eBone->segments;
		newBone->layer = eBone->layer;
		
		if(eBone->prop)
			newBone->prop= IDP_CopyProperty(eBone->prop);
	}
	
	/*	Fix parenting in a separate pass to ensure ebone->bone connections
		are valid at this point */
	for (eBone=arm->edbo->first;eBone;eBone=eBone->next) {
		newBone= (Bone *)eBone->temp;
		if (eBone->parent) {
			newBone->parent= (Bone *)eBone->parent->temp;
			BLI_addtail(&newBone->parent->childbase, newBone);
			
			{
				float M_boneRest[3][3];
				float M_parentRest[3][3];
				float iM_parentRest[3][3];
				float	delta[3];
				
				/* Get the parent's  matrix (rotation only) */
				sub_v3_v3v3(delta, eBone->parent->tail, eBone->parent->head);
				vec_roll_to_mat3(delta, eBone->parent->roll, M_parentRest);
				
				/* Get this bone's  matrix (rotation only) */
				sub_v3_v3v3(delta, eBone->tail, eBone->head);
				vec_roll_to_mat3(delta, eBone->roll, M_boneRest);
				
				/* Invert the parent matrix */
				invert_m3_m3(iM_parentRest, M_parentRest);
				
				/* Get the new head and tail */
				sub_v3_v3v3(newBone->head, eBone->head, eBone->parent->tail);
				sub_v3_v3v3(newBone->tail, eBone->tail, eBone->parent->tail);
				
				mul_m3_v3(iM_parentRest, newBone->head);
				mul_m3_v3(iM_parentRest, newBone->tail);
			}
		}
		/*	...otherwise add this bone to the armature's bonebase */
		else
			BLI_addtail(&arm->bonebase, newBone);
	}
	
	/* Make a pass through the new armature to fix rolling */
	/* also builds restposition again (like where_is_armature) */
	fix_bonelist_roll(&arm->bonebase, arm->edbo);
	
	/* so all users of this armature should get rebuilt */
	for (obt= G.main->object.first; obt; obt= obt->id.next) {
		if (obt->data==arm)
			armature_rebuild_pose(obt, arm);
	}
	
	DAG_id_flush_update(&obedit->id, OB_RECALC_DATA);
}

void ED_armature_apply_transform(Object *ob, float mat[4][4])
{
	EditBone *ebone;
	bArmature *arm= ob->data;
	float scale = mat4_to_scale(mat);	/* store the scale of the matrix here to use on envelopes */
	
	/* Put the armature into editmode */
	ED_armature_to_edit(ob);

	/* Do the rotations */
	for (ebone = arm->edbo->first; ebone; ebone=ebone->next){
		mul_m4_v3(mat, ebone->head);
		mul_m4_v3(mat, ebone->tail);
		
		ebone->rad_head	*= scale;
		ebone->rad_tail	*= scale;
		ebone->dist		*= scale;
	}
	
	/* Turn the list into an armature */
	ED_armature_from_edit(ob);
	ED_armature_edit_free(ob);
}

/* exported for use in editors/object/ */
/* 0 == do center, 1 == center new, 2 == center cursor */
void docenter_armature (Scene *scene, View3D *v3d, Object *ob, int centermode)
{
	Object *obedit= scene->obedit; // XXX get from context
	EditBone *ebone;
	bArmature *arm= ob->data;
	float cent[3] = {0.0f, 0.0f, 0.0f};
	float min[3], max[3];
	float omat[3][3];

	/* Put the armature into editmode */
	if(ob!=obedit)
		ED_armature_to_edit(ob);

	/* Find the centerpoint */
	if (centermode == 2) {
		float *fp= give_cursor(scene, v3d);
		VECCOPY(cent, fp);
		invert_m4_m4(ob->imat, ob->obmat);
		mul_m4_v3(ob->imat, cent);
	}
	else {
		INIT_MINMAX(min, max);
		
		for (ebone= arm->edbo->first; ebone; ebone=ebone->next) {
			DO_MINMAX(ebone->head, min, max);
			DO_MINMAX(ebone->tail, min, max);
		}
		
		cent[0]= (min[0] + max[0]) / 2.0f;
		cent[1]= (min[1] + max[1]) / 2.0f;
		cent[2]= (min[2] + max[2]) / 2.0f;
	}
	
	/* Do the adjustments */
	for (ebone= arm->edbo->first; ebone; ebone=ebone->next) {
		sub_v3_v3v3(ebone->head, ebone->head, cent);
		sub_v3_v3v3(ebone->tail, ebone->tail, cent);
	}
	
	/* Turn the list into an armature */
	ED_armature_from_edit(ob);
	
	/* Adjust object location for new centerpoint */
	if(centermode && obedit==NULL) {
		copy_m3_m4(omat, ob->obmat);
		
		mul_m3_v3(omat, cent);
		ob->loc[0] += cent[0];
		ob->loc[1] += cent[1];
		ob->loc[2] += cent[2];
	}
	else 
		ED_armature_edit_free(ob);
}

/* ---------------------- */

static EditBone *editbone_name_exists (ListBase *edbo, char *name)
{
	EditBone	*eBone;
	
	for (eBone=edbo->first; eBone; eBone=eBone->next) {
		if (!strcmp(name, eBone->name))
			return eBone;
	}
	return NULL;
}

/* note: there's a unique_bone_name() too! */
void unique_editbone_name (ListBase *edbo, char *name, EditBone *bone)
{
	EditBone *dupli;
	char		tempname[64];
	int			number;
	char		*dot;

	dupli = editbone_name_exists(edbo, name);
	
	if (dupli && bone != dupli) {
		/*	Strip off the suffix, if it's a number */
		number= strlen(name);
		if (number && isdigit(name[number-1])) {
			dot= strrchr(name, '.');	// last occurrence
			if (dot)
				*dot=0;
		}
		
		for (number = 1; number <= 999; number++) {
			sprintf(tempname, "%s.%03d", name, number);
			if (!editbone_name_exists(edbo, tempname)) {
				BLI_strncpy(name, tempname, 32);
				return;
			}
		}
	}
}

/* helper for apply_armature_pose2bones - fixes parenting of objects that are bone-parented to armature */
static void applyarmature_fix_boneparents (Scene *scene, Object *armob)
{
	Object workob, *ob;
	
	/* go through all objects in database */
	for (ob= G.main->object.first; ob; ob= ob->id.next) {
		/* if parent is bone in this armature, apply corrections */
		if ((ob->parent == armob) && (ob->partype == PARBONE)) {
			/* apply current transform from parent (not yet destroyed), 
			 * then calculate new parent inverse matrix
			 */
			ED_object_apply_obmat(ob);
			
			what_does_parent(scene, ob, &workob);
			invert_m4_m4(ob->parentinv, workob.obmat);
		}
	}
}

/* set the current pose as the restpose */
static int apply_armature_pose2bones_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C); // must be active object, not edit-object
	bArmature *arm= get_armature(ob);
	bPose *pose;
	bPoseChannel *pchan;
	EditBone *curbone;
	
	/* don't check if editmode (should be done by caller) */
	if (ob->type!=OB_ARMATURE)
		return OPERATOR_CANCELLED;
	if (object_data_is_libdata(ob)) {
		BKE_report(op->reports, RPT_ERROR, "Cannot apply pose to lib-linked armature."); //error_libdata();
		return OPERATOR_CANCELLED;
	}
	
	/* helpful warnings... */
	// TODO: add warnings to be careful about actions, applying deforms first, etc.
	
	/* Get editbones of active armature to alter */
	ED_armature_to_edit(ob);	
	
	/* get pose of active object and move it out of posemode */
	pose= ob->pose;
	
	for (pchan=pose->chanbase.first; pchan; pchan=pchan->next) {
		curbone= editbone_name_exists(arm->edbo, pchan->name);
		
		/* simply copy the head/tail values from pchan over to curbone */
		VECCOPY(curbone->head, pchan->pose_head);
		VECCOPY(curbone->tail, pchan->pose_tail);
		
		/* fix roll:
		 *	1. find auto-calculated roll value for this bone now
		 *	2. remove this from the 'visual' y-rotation
		 */
		{
			float premat[3][3], imat[3][3],pmat[3][3], tmat[3][3];
			float delta[3], eul[3];
			
			/* obtain new auto y-rotation */
			sub_v3_v3v3(delta, curbone->tail, curbone->head);
			vec_roll_to_mat3(delta, 0.0f, premat);
			invert_m3_m3(imat, premat);
			
			/* get pchan 'visual' matrix */
			copy_m3_m4(pmat, pchan->pose_mat);
			
			/* remove auto from visual and get euler rotation */
			mul_m3_m3m3(tmat, imat, pmat);
			mat3_to_eul( eul,tmat);
			
			/* just use this euler-y as new roll value */
			curbone->roll= eul[1];
		}
		
		/* clear transform values for pchan */
		pchan->loc[0]= pchan->loc[1]= pchan->loc[2]= 0.0f;
		pchan->eul[0]= pchan->eul[1]= pchan->eul[2]= 0.0f;
		pchan->quat[1]= pchan->quat[2]= pchan->quat[3]= 0.0f;
		pchan->quat[0]= pchan->size[0]= pchan->size[1]= pchan->size[2]= 1.0f;
		
		/* set anim lock */
		curbone->flag |= BONE_UNKEYED;
	}
	
	/* convert editbones back to bones, and then free the edit-data */
	ED_armature_from_edit(ob);
	ED_armature_edit_free(ob);
	
	/* flush positions of posebones */
	where_is_pose(scene, ob);
	
	/* fix parenting of objects which are bone-parented */
	applyarmature_fix_boneparents(scene, ob);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_apply (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Apply Pose as Rest Pose";
	ot->idname= "POSE_OT_apply";
	ot->description= "Apply the current pose as the new rest pose.";
	
	/* callbacks */
	ot->exec= apply_armature_pose2bones_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ---------------------- */

/* Helper function for armature joining - link fixing */
static void joined_armature_fix_links(Object *tarArm, Object *srcArm, bPoseChannel *pchan, EditBone *curbone)
{
	Object *ob;
	bPose *pose;
	bPoseChannel *pchant;
	bConstraint *con;
	
	/* let's go through all objects in database */
	for (ob= G.main->object.first; ob; ob= ob->id.next) {
		/* do some object-type specific things */
		if (ob->type == OB_ARMATURE) {
			pose= ob->pose;
			for (pchant= pose->chanbase.first; pchant; pchant= pchant->next) {
				for (con= pchant->constraints.first; con; con= con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					/* constraint targets */
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct= targets.first; ct; ct= ct->next) {
							if (ct->tar == srcArm) {
								if (strcmp(ct->subtarget, "")==0) {
									ct->tar = tarArm;
								}
								else if (strcmp(ct->subtarget, pchan->name)==0) {
									ct->tar = tarArm;
									strcpy(ct->subtarget, curbone->name);
								}
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 0);
					}
					
					/* action constraint? */
					if (con->type == CONSTRAINT_TYPE_ACTION) {
						bActionConstraint *data= con->data; // XXX old animation system
						bAction *act;
						bActionChannel *achan;
						
						if (data->act) {
							act= data->act;
							
							for (achan= act->chanbase.first; achan; achan= achan->next) {
								if (strcmp(achan->name, pchan->name)==0)
									BLI_strncpy(achan->name, curbone->name, 32);
							}
						}
					}
					
				}
			}
		}
			
		/* fix object-level constraints */
		if (ob != srcArm) {
			for (con= ob->constraints.first; con; con= con->next) {
				bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				/* constraint targets */
				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);
					
					for (ct= targets.first; ct; ct= ct->next) {
						if (ct->tar == srcArm) {
							if (strcmp(ct->subtarget, "")==0) {
								ct->tar = tarArm;
							}
							else if (strcmp(ct->subtarget, pchan->name)==0) {
								ct->tar = tarArm;
								strcpy(ct->subtarget, curbone->name);
							}
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(con, &targets, 0);
				}
			}
		}
		
		/* See if an object is parented to this armature */
		if (ob->parent && (ob->parent == srcArm)) {
			/* Is object parented to a bone of this src armature? */
			if (ob->partype==PARBONE) {
				/* bone name in object */
				if (!strcmp(ob->parsubstr, pchan->name))
					BLI_strncpy(ob->parsubstr, curbone->name, 32);
			}
			
			/* make tar armature be new parent */
			ob->parent = tarArm;
		}
	}	
}

/* join armature exec is exported for use in object->join objects operator... */
int join_armature_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object	*ob= CTX_data_active_object(C);
	bArmature *arm= (ob)? ob->data: NULL;
	bPose *pose, *opose;
	bPoseChannel *pchan, *pchann;
	EditBone *curbone;
	float	mat[4][4], oimat[4][4];
	
	/*	Ensure we're not in editmode and that the active object is an armature*/
	if (!ob || ob->type!=OB_ARMATURE)
		return OPERATOR_CANCELLED;
	if (!arm || arm->edbo)
		return OPERATOR_CANCELLED;
	
	/* Get editbones of active armature to add editbones to */
	ED_armature_to_edit(ob);
	
	/* get pose of active object and move it out of posemode */
	pose= ob->pose;
	ob->mode &= ~OB_MODE_POSE;

	CTX_DATA_BEGIN(C, Base*, base, selected_editable_bases) {
		if ((base->object->type==OB_ARMATURE) && (base->object!=ob)) {
			bArmature *curarm= base->object->data;
			
			/* Make a list of editbones in current armature */
			ED_armature_to_edit(base->object);
			
			/* Get Pose of current armature */
			opose= base->object->pose;
			base->object->mode &= ~OB_MODE_POSE;
			//BASACT->flag &= ~OB_MODE_POSE;
			
			/* Find the difference matrix */
			invert_m4_m4(oimat, ob->obmat);
			mul_m4_m4m4(mat, base->object->obmat, oimat);
			
			/* Copy bones and posechannels from the object to the edit armature */
			for (pchan=opose->chanbase.first; pchan; pchan=pchann) {
				pchann= pchan->next;
				curbone= editbone_name_exists(curarm->edbo, pchan->name);
				
				/* Get new name */
				unique_editbone_name(arm->edbo, curbone->name, NULL);
				
				/* Transform the bone */
				{
					float premat[4][4];
					float postmat[4][4];
					float difmat[4][4];
					float imat[4][4];
					float temp[3][3];
					float delta[3];
					
					/* Get the premat */
					sub_v3_v3v3(delta, curbone->tail, curbone->head);
					vec_roll_to_mat3(delta, curbone->roll, temp);
					
					unit_m4(premat); /* Mat4MulMat34 only sets 3x3 part */
					mul_m4_m3m4(premat, temp, mat);
					
					mul_m4_v3(mat, curbone->head);
					mul_m4_v3(mat, curbone->tail);
					
					/* Get the postmat */
					sub_v3_v3v3(delta, curbone->tail, curbone->head);
					vec_roll_to_mat3(delta, curbone->roll, temp);
					copy_m4_m3(postmat, temp);
					
					/* Find the roll */
					invert_m4_m4(imat, premat);
					mul_m4_m4m4(difmat, postmat, imat);
					
					curbone->roll -= (float)atan2(difmat[2][0], difmat[2][2]);
				}
				
				/* Fix Constraints and Other Links to this Bone and Armature */
				joined_armature_fix_links(ob, base->object, pchan, curbone);
				
				/* Rename pchan */
				BLI_strncpy(pchan->name, curbone->name, sizeof(pchan->name));
				
				/* Jump Ship! */
				BLI_remlink(curarm->edbo, curbone);
				BLI_addtail(arm->edbo, curbone);
				
				BLI_remlink(&opose->chanbase, pchan);
				BLI_addtail(&pose->chanbase, pchan);
			}
			
			ED_base_object_free_and_unlink(scene, base);
		}
	}
	CTX_DATA_END;
	
	DAG_scene_sort(scene);	// because we removed object(s)

	ED_armature_from_edit(ob);
	ED_armature_edit_free(ob);

	WM_event_add_notifier(C, NC_SCENE|ND_OB_ACTIVE, scene);
	
	return OPERATOR_FINISHED;
}

/* ---------------------- */

/* Helper function for armature separating - link fixing */
static void separated_armature_fix_links(Object *origArm, Object *newArm)
{
	Object *ob;
	bPoseChannel *pchan, *pcha, *pchb;
	bConstraint *con;
	ListBase *opchans, *npchans;
	
	/* get reference to list of bones in original and new armatures  */
	opchans= &origArm->pose->chanbase;
	npchans= &newArm->pose->chanbase;
	
	/* let's go through all objects in database */
	for (ob= G.main->object.first; ob; ob= ob->id.next) {
		/* do some object-type specific things */
		if (ob->type == OB_ARMATURE) {
			for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				for (con= pchan->constraints.first; con; con= con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					/* constraint targets */
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct= targets.first; ct; ct= ct->next) {
							/* any targets which point to original armature are redirected to the new one only if:
							 *	- the target isn't origArm/newArm itself
							 *	- the target is one that can be found in newArm/origArm
							 */
							if ((ct->tar == origArm) && (ct->subtarget[0] != 0)) {
								for (pcha=npchans->first, pchb=npchans->last; pcha && pchb; pcha=pcha->next, pchb=pchb->prev) {
									/* check if either one matches */
									if ( (strcmp(pcha->name, ct->subtarget)==0) ||
										 (strcmp(pchb->name, ct->subtarget)==0) )
									{
										ct->tar= newArm;
										break;
									}
									
									/* check if both ends have met (to stop checking) */
									if (pcha == pchb) break;
								}								
							}
							else if ((ct->tar == newArm) && (ct->subtarget[0] != 0)) {
								for (pcha=opchans->first, pchb=opchans->last; pcha && pchb; pcha=pcha->next, pchb=pchb->prev) {
									/* check if either one matches */
									if ( (strcmp(pcha->name, ct->subtarget)==0) ||
										 (strcmp(pchb->name, ct->subtarget)==0) )
									{
										ct->tar= origArm;
										break;
									}
									
									/* check if both ends have met (to stop checking) */
									if (pcha == pchb) break;
								}								
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 0);
					}
				}
			}
		}
			
		/* fix object-level constraints */
		if (ob != origArm) {
			for (con= ob->constraints.first; con; con= con->next) {
				bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				/* constraint targets */
				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);
					
					for (ct= targets.first; ct; ct= ct->next) {
						/* any targets which point to original armature are redirected to the new one only if:
						 *	- the target isn't origArm/newArm itself
						 *	- the target is one that can be found in newArm/origArm
						 */
						if ((ct->tar == origArm) && (ct->subtarget[0] != 0)) {
							for (pcha=npchans->first, pchb=npchans->last; pcha && pchb; pcha=pcha->next, pchb=pchb->prev) {
								/* check if either one matches */
								if ( (strcmp(pcha->name, ct->subtarget)==0) ||
									 (strcmp(pchb->name, ct->subtarget)==0) )
								{
									ct->tar= newArm;
									break;
								}
								
								/* check if both ends have met (to stop checking) */
								if (pcha == pchb) break;
							}								
						}
						else if ((ct->tar == newArm) && (ct->subtarget[0] != 0)) {
							for (pcha=opchans->first, pchb=opchans->last; pcha && pchb; pcha=pcha->next, pchb=pchb->prev) {
								/* check if either one matches */
								if ( (strcmp(pcha->name, ct->subtarget)==0) ||
									 (strcmp(pchb->name, ct->subtarget)==0) )
								{
									ct->tar= origArm;
									break;
								}
								
								/* check if both ends have met (to stop checking) */
								if (pcha == pchb) break;
							}								
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(con, &targets, 0);
				}
			}
		}
		
		/* See if an object is parented to this armature */
		if ((ob->parent) && (ob->parent == origArm)) {
			/* Is object parented to a bone of this src armature? */
			if (ob->partype==PARBONE) {
				/* bone name in object */
				for (pcha=npchans->first, pchb=npchans->last; pcha && pchb; pcha=pcha->next, pchb=pchb->prev) {
					/* check if either one matches */
					if ( (strcmp(pcha->name, ob->parsubstr)==0) ||
						 (strcmp(pchb->name, ob->parsubstr)==0) )
					{
						ob->parent= newArm;
						break;
					}
					
					/* check if both ends have met (to stop checking) */
					if (pcha == pchb) break;
				}
			}
		}
	}	
}

/* Helper function for armature separating - remove certain bones from the given armature 
 *	sel: remove selected bones from the armature, otherwise the unselected bones are removed
 *  (ob is not in editmode)
 */
static void separate_armature_bones (Scene *scene, Object *ob, short sel) 
{
	bArmature *arm= (bArmature *)ob->data;
	bPoseChannel *pchan, *pchann;
	EditBone *curbone;
	
	/* make local set of editbones to manipulate here */
	ED_armature_to_edit(ob);
	
	/* go through pose-channels, checking if a bone should be removed */
	for (pchan=ob->pose->chanbase.first; pchan; pchan=pchann) {
		pchann= pchan->next;
		curbone= editbone_name_exists(arm->edbo, pchan->name);
		
		/* check if bone needs to be removed */
		if ( (sel && (curbone->flag & BONE_SELECTED)) ||
			 (!sel && !(curbone->flag & BONE_SELECTED)) )
		{
			EditBone *ebo;
			bPoseChannel *pchn;
			
			/* clear the bone->parent var of any bone that had this as its parent  */
			for (ebo= arm->edbo->first; ebo; ebo= ebo->next) {
				if (ebo->parent == curbone) {
					ebo->parent= NULL;
					ebo->temp= NULL; /* this is needed to prevent random crashes with in ED_armature_from_edit */
					ebo->flag &= ~BONE_CONNECTED;
				}
			}
			
			/* clear the pchan->parent var of any pchan that had this as its parent */
			for (pchn= ob->pose->chanbase.first; pchn; pchn=pchn->next) {
				if (pchn->parent == pchan)
					pchn->parent= NULL;
			}
			
			/* free any of the extra-data this pchan might have */
			free_pose_channel(pchan);
			
			/* get rid of unneeded bone */
			bone_free(arm, curbone);
			BLI_freelinkN(&ob->pose->chanbase, pchan);
		}
	}
	
	/* exit editmode (recalculates pchans too) */
	ED_armature_from_edit(ob);
	ED_armature_edit_free(ob);
}

/* separate selected bones into their armature */
static int separate_armature_exec (bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	Object *oldob, *newob;
	Base *oldbase, *newbase;
	bArmature *arm;
	
	/* sanity checks */
	if (obedit == NULL)
		return OPERATOR_CANCELLED;
	arm= obedit->data;
	
	/* set wait cursor in case this takes a while */
	WM_cursor_wait(1);
	
	/* we are going to do this as follows (unlike every other instance of separate):
	 *	1. exit editmode +posemode for active armature/base. Take note of what this is.
	 *	2. duplicate base - BASACT is the new one now
	 *	3. for each of the two armatures, enter editmode -> remove appropriate bones -> exit editmode + recalc
	 *	4. fix constraint links
	 *	5. make original armature active and enter editmode
	 */
	
	/* 1) only edit-base selected */
	// TODO: use context iterators for this?
	CTX_DATA_BEGIN(C, Base *, base, visible_bases) {
		if (base->object==obedit) base->flag |= 1;
		else base->flag &= ~1;
	}
	CTX_DATA_END;
	
	/* 1) store starting settings and exit editmode */
	oldob= obedit;
	oldbase= BASACT;
	oldob->mode &= ~OB_MODE_POSE;
	//oldbase->flag &= ~OB_POSEMODE;
	
	ED_armature_from_edit(obedit);
	ED_armature_edit_free(obedit);
	
	/* 2) duplicate base */
	newbase= ED_object_add_duplicate(scene, oldbase, USER_DUP_ARM); /* only duplicate linked armature */
	newob= newbase->object;		
	newbase->flag &= ~SELECT;
	
	
	/* 3) remove bones that shouldn't still be around on both armatures */
	separate_armature_bones(scene, oldob, 1);
	separate_armature_bones(scene, newob, 0);
	
	
	/* 4) fix links before depsgraph flushes */ // err... or after?
	separated_armature_fix_links(oldob, newob);
	
	DAG_id_flush_update(&oldob->id, OB_RECALC_DATA);	/* this is the original one */
	DAG_id_flush_update(&newob->id, OB_RECALC_DATA);	/* this is the separated one */
	
	
	/* 5) restore original conditions */
	obedit= oldob;
	
	ED_armature_to_edit(obedit);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, obedit);
	
	/* recalc/redraw + cleanup */
	WM_cursor_wait(0);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_separate (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Separate Bones";
	ot->idname= "ARMATURE_OT_separate";
	ot->description= "Isolate selected bones into a separate armature.";
	
	/* callbacks */
	ot->invoke= WM_operator_confirm;
	ot->exec= separate_armature_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* **************** END tools on Editmode Armature **************** */
/* **************** PoseMode & EditMode *************************** */

/* only for opengl selection indices */
Bone *get_indexed_bone (Object *ob, int index)
{
	bPoseChannel *pchan;
	int a= 0;
	
	if(ob->pose==NULL) return NULL;
	index>>=16;		// bone selection codes use left 2 bytes
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next, a++) {
		if(a==index) return pchan->bone;
	}
	return NULL;
}

/* See if there are any selected bones in this buffer */
/* only bones from base are checked on */
static void *get_bone_from_selectbuffer(Scene *scene, Base *base, unsigned int *buffer, short hits, short findunsel)
{
	Object *obedit= scene->obedit; // XXX get from context
	Bone *bone;
	EditBone *ebone;
	void *firstunSel=NULL, *firstSel=NULL, *data;
	unsigned int hitresult;
	short i, takeNext=0, sel;
	
	for (i=0; i< hits; i++){
		hitresult = buffer[3+(i*4)];
		
		if (!(hitresult & BONESEL_NOSEL)) {	// -1
			if(hitresult & BONESEL_ANY) {	// to avoid including objects in selection
				
				hitresult &= ~(BONESEL_ANY);
				/* Determine what the current bone is */
				if (obedit==NULL || base->object!=obedit) {
					/* no singular posemode, so check for correct object */
					if(base->selcol == (hitresult & 0xFFFF)) {
						bone = get_indexed_bone(base->object, hitresult);
						
						if (findunsel)
							sel = (bone->flag & BONE_SELECTED);
						else
							sel = !(bone->flag & BONE_SELECTED);
						
						data = bone;
					}
					else {
						data= NULL;
						sel= 0;
					}
				}
				else{
					bArmature *arm= obedit->data;
					
					ebone = BLI_findlink(arm->edbo, hitresult);
					if (findunsel)
						sel = (ebone->flag & BONE_SELECTED);
					else
						sel = !(ebone->flag & BONE_SELECTED);
					
					data = ebone;
				}
				
				if(data) {
					if (sel) {
						if(!firstSel) firstSel= data;
						takeNext=1;
					}
					else {
						if (!firstunSel)
							firstunSel=data;
						if (takeNext)
							return data;
					}
				}
			}
		}
	}
	
	if (firstunSel)
		return firstunSel;
	else 
		return firstSel;
}



/* used by posemode as well editmode */
/* only checks scene->basact! */
/* x and y are mouse coords (area space) */
static void *get_nearest_bone (bContext *C, short findunsel, int x, int y)
{
	ViewContext vc;
	rcti rect;
	unsigned int buffer[MAXPICKBUF];
	short hits;
	
	view3d_set_viewcontext(C, &vc);
	
	// rect.xmin= ... mouseco!
	rect.xmin= rect.xmax= x;
	rect.ymin= rect.ymax= y;
	
	glInitNames();
	hits= view3d_opengl_select(&vc, buffer, MAXPICKBUF, &rect);

	if (hits>0)
		return get_bone_from_selectbuffer(vc.scene, vc.scene->basact, buffer, hits, findunsel);
	
	return NULL;
}

/* helper for setflag_sel_bone() */
static void bone_setflag (int *bone, int flag, short mode)
{
	if (bone && flag) {
		/* exception for inverse flags */
		if (flag == BONE_NO_DEFORM) {
			if (mode == 2)
				*bone |= flag;
			else if (mode == 1)
				*bone &= ~flag;
			else
				*bone ^= flag;
		}
		else {
			if (mode == 2)
				*bone &= ~flag;
			else if (mode == 1)
				*bone |= flag;
			else
				*bone ^= flag;
		}
	}
}

/* Get the first available child of an editbone */
static EditBone *editbone_get_child(bArmature *arm, EditBone *pabone, short use_visibility)
{
	EditBone *curbone, *chbone=NULL;
	
	for (curbone= arm->edbo->first; curbone; curbone= curbone->next) {
		if (curbone->parent == pabone) {
			if (use_visibility) {
				if ((arm->layer & curbone->layer) && !(pabone->flag & BONE_HIDDEN_A))
					chbone = curbone;
			}
			else
				chbone = curbone;
		}
	}
	
	return chbone;
}

/* callback for posemode setflag */
static int pose_setflag_exec (bContext *C, wmOperator *op)
{
	int flag= RNA_enum_get(op->ptr, "type");
	int mode= RNA_enum_get(op->ptr, "mode");
	
	/* loop over all selected pchans */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, selected_pose_bones) 
	{
		bone_setflag(&pchan->bone->flag, flag, mode);
	}
	CTX_DATA_END;
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, CTX_data_active_object(C));
	
	return OPERATOR_FINISHED;
}

/* callback for editbones setflag */
static int armature_bones_setflag_exec (bContext *C, wmOperator *op)
{
	int flag= RNA_enum_get(op->ptr, "type");
	int mode= RNA_enum_get(op->ptr, "mode");
	
	/* loop over all selected pchans */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_bones) 
	{
		bone_setflag(&ebone->flag, flag, mode);
	}
	CTX_DATA_END;
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, CTX_data_edit_object(C));
	
	return OPERATOR_FINISHED;
}

/* settings that can be changed */
static EnumPropertyItem prop_bone_setting_types[] = {
	{BONE_DRAWWIRE, "DRAWWIRE", 0, "Draw Wire", ""},
	{BONE_NO_DEFORM, "DEFORM", 0, "Deform", ""},
	{BONE_MULT_VG_ENV, "MULT_VG", 0, "Multiply Vertex Groups", ""},
	{BONE_HINGE, "HINGE", 0, "Hinge", ""},
	{BONE_NO_SCALE, "NO_SCALE", 0, "No Scale", ""},
	{BONE_EDITMODE_LOCKED, "LOCKED", 0, "Locked", "(For EditMode only)"},
	{0, NULL, 0, NULL, NULL}
};

/* ways that settings can be changed */
static EnumPropertyItem prop_bone_setting_modes[] = {
	{0, "CLEAR", 0, "Clear", ""},
	{1, "ENABLE", 0, "Enable", ""},
	{2, "TOGGLE", 0, "Toggle", ""},
	{0, NULL, 0, NULL, NULL}
};


void ARMATURE_OT_flags_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Bone Flags";
	ot->idname= "ARMATURE_OT_flags_set";
	ot->description= "Set flags for armature bones.";
	
	/* callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= armature_bones_setflag_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", prop_bone_setting_types, 0, "Type", "");
	RNA_def_enum(ot->srna, "mode", prop_bone_setting_modes, 0, "Mode", "");
}

void POSE_OT_flags_set (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Bone Flags";
	ot->idname= "POSE_OT_flags_set";
	ot->description= "Set flags for armature bones.";
	
	/* callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= pose_setflag_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", prop_bone_setting_types, 0, "Type", "");
	RNA_def_enum(ot->srna, "mode", prop_bone_setting_modes, 0, "Mode", "");
}


/* **************** END PoseMode & EditMode *************************** */
/* **************** Posemode stuff ********************** */


static void selectconnected_posebonechildren (Object *ob, Bone *bone, int extend)
{
	Bone *curBone;
	
	/* stop when unconnected child is encontered, or when unselectable bone is encountered */
	if (!(bone->flag & BONE_CONNECTED) || (bone->flag & BONE_UNSELECTABLE))
		return;
	
		// XXX old cruft! use notifiers instead
	//select_actionchannel_by_name (ob->action, bone->name, !(shift));
	
	if (extend)
		bone->flag &= ~BONE_SELECTED;
	else
		bone->flag |= BONE_SELECTED;
	
	for (curBone=bone->childbase.first; curBone; curBone=curBone->next)
		selectconnected_posebonechildren(ob, curBone, extend);
}

/* within active object context */
/* previously known as "selectconnected_posearmature" */
static int pose_select_connected_invoke(bContext *C, wmOperator *op, wmEvent *event)
{  
	ARegion *ar= CTX_wm_region(C);
	Object *ob= CTX_data_edit_object(C);
	Bone *bone, *curBone, *next= NULL;
	int extend= RNA_boolean_get(op->ptr, "extend");
	int x, y;
	
	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;

	view3d_operator_needs_opengl(C);
	
	if (extend)
		bone= get_nearest_bone(C, 0, x, y);
	else
		bone= get_nearest_bone(C, 1, x, y);
	
	if (!bone)
		return OPERATOR_CANCELLED;
	
	/* Select parents */
	for (curBone=bone; curBone; curBone=next){
		/* ignore bone if cannot be selected */
		if ((curBone->flag & BONE_UNSELECTABLE) == 0) { 
				// XXX old cruft! use notifiers instead
			//select_actionchannel_by_name (ob->action, curBone->name, !(shift));
			
			if (extend)
				curBone->flag &= ~BONE_SELECTED;
			else
				curBone->flag |= BONE_SELECTED;
			
			if (curBone->flag & BONE_CONNECTED)
				next=curBone->parent;
			else
				next=NULL;
		}
		else
			next= NULL;
	}
	
	/* Select children */
	for (curBone=bone->childbase.first; curBone; curBone=next)
		selectconnected_posebonechildren(ob, curBone, extend);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

static int pose_select_linked_poll(bContext *C)
{
	return ( ED_operator_view3d_active(C) && ED_operator_posemode(C) );
}

void POSE_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Connected";
	ot->idname= "POSE_OT_select_linked";
	
	/* api callbacks */
	ot->exec= NULL;
	ot->invoke= pose_select_connected_invoke;
	ot->poll= pose_select_linked_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */	
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend", "Extend selection instead of deselecting everything first.");
}

/* **************** END Posemode stuff ********************** */
/* **************** EditMode stuff ********************** */

/* called in space.c */
/* previously "selectconnected_armature" */
static int armature_select_linked_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bArmature *arm;
	EditBone *bone, *curBone, *next;
	int extend= RNA_boolean_get(op->ptr, "extend");
	int x, y;
	ARegion *ar;
	Object *obedit= CTX_data_edit_object(C);
	arm= obedit->data;
	ar= CTX_wm_region(C);

	x= event->x - ar->winrct.xmin;
	y= event->y - ar->winrct.ymin;

	view3d_operator_needs_opengl(C);

	if (extend)
		bone= get_nearest_bone(C, 0, x, y);
	else
		bone= get_nearest_bone(C, 1, x, y);

	if (!bone)
		return OPERATOR_CANCELLED;

	/* Select parents */
	for (curBone=bone; curBone; curBone=next) {
		if ((curBone->flag & BONE_UNSELECTABLE) == 0) {
			if (extend) {
				curBone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
			}
			else{
				curBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
			}
		}
		
		if (curBone->flag & BONE_CONNECTED)
			next=curBone->parent;
		else
			next=NULL;
	}

	/* Select children */
	while (bone) {
		for (curBone=arm->edbo->first; curBone; curBone=next) {
			next = curBone->next;
			if ((curBone->parent == bone) && (curBone->flag & BONE_UNSELECTABLE)==0) {
				if (curBone->flag & BONE_CONNECTED) {
					if (extend)
						curBone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
					else
						curBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
					bone=curBone;
					break;
				}
				else { 
					bone=NULL;
					break;
				}
			}
		}
		if (!curBone)
			bone=NULL;
	}
	
	ED_armature_sync_selection(arm->edbo);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

static int armature_select_linked_poll(bContext *C)
{
	return ( ED_operator_view3d_active(C) && ED_operator_editarmature(C) );
}

void ARMATURE_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Connected";
	ot->idname= "ARMATURE_OT_select_linked";
	
	/* api callbacks */
	ot->exec= NULL;
	ot->invoke= armature_select_linked_invoke;
	ot->poll= armature_select_linked_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties s*/
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend", "Extend selection instead of deselecting everything first.");
}

/* does bones and points */
/* note that BONE ROOT only gets drawn for root bones (or without IK) */
static EditBone *get_nearest_editbonepoint (ViewContext *vc, short mval[2], ListBase *edbo, int findunsel, int *selmask)
{
	EditBone *ebone;
	rcti rect;
	unsigned int buffer[MAXPICKBUF];
	unsigned int hitresult, besthitresult=BONESEL_NOSEL;
	int i, mindep= 4;
	short hits;

	glInitNames();
	
	rect.xmin= mval[0]-5;
	rect.xmax= mval[0]+5;
	rect.ymin= mval[1]-5;
	rect.ymax= mval[1]+5;
	
	hits= view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect);
	if(hits==0) {
		rect.xmin= mval[0]-12;
		rect.xmax= mval[0]+12;
		rect.ymin= mval[1]-12;
		rect.ymax= mval[1]+12;
		hits= view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect);
	}
	/* See if there are any selected bones in this group */
	if (hits>0) {
		
		if(hits==1) {
			if (!(buffer[3] & BONESEL_NOSEL)) 
				besthitresult= buffer[3];
		}
		else {
			for (i=0; i< hits; i++) {
				hitresult= buffer[3+(i*4)];
				if (!(hitresult & BONESEL_NOSEL)) {
					int dep;
					
					ebone = BLI_findlink(edbo, hitresult & ~BONESEL_ANY);
					
					/* clicks on bone points get advantage */
					if( hitresult & (BONESEL_ROOT|BONESEL_TIP)) {
						/* but also the unselected one */
						if(findunsel) {
							if( (hitresult & BONESEL_ROOT) && (ebone->flag & BONE_ROOTSEL)==0) 
								dep= 1;
							else if( (hitresult & BONESEL_TIP) && (ebone->flag & BONE_TIPSEL)==0) 
								dep= 1;
							else 
								dep= 2;
						}
						else dep= 2;
					}
					else {
						/* bone found */
						if(findunsel) {
							if((ebone->flag & BONE_SELECTED)==0)
								dep= 2;
							else
								dep= 3;
						}
						else dep= 3;
					}
					if(dep < mindep) {
						mindep= dep;
						besthitresult= hitresult;
					}
				}
			}
		}
		
		if (!(besthitresult & BONESEL_NOSEL)) {
			
			ebone= BLI_findlink(edbo, besthitresult & ~BONESEL_ANY);
			
			*selmask = 0;
			if (besthitresult & BONESEL_ROOT)
				*selmask |= BONE_ROOTSEL;
			if (besthitresult & BONESEL_TIP)
				*selmask |= BONE_TIPSEL;
			if (besthitresult & BONESEL_BONE)
				*selmask |= BONE_SELECTED;
			return ebone;
		}
	}
	*selmask = 0;
	return NULL;
}

/* context: editmode armature */
EditBone *ED_armature_bone_get_mirrored(ListBase *edbo, EditBone *ebo)
{
	EditBone *eboflip= NULL;
	char name[32];
	
	if (ebo == NULL)
		return NULL;
	
	BLI_strncpy(name, ebo->name, sizeof(name));
	bone_flip_name(name, 0);		// 0 = don't strip off number extensions
	
	for (eboflip= edbo->first; eboflip; eboflip=eboflip->next) {
		if (ebo != eboflip) {
			if (!strcmp (name, eboflip->name)) 
				break;
		}
	}
	
	return eboflip;
}


/* previously delete_armature */
/* only editmode! */
static int armature_delete_selected_exec(bContext *C, wmOperator *op)
{
	bArmature *arm;
	EditBone	*curBone, *next;
	bConstraint *con;
	Object *obedit= CTX_data_edit_object(C); // XXX get from context
	arm = obedit->data;

	/* cancel if nothing selected */
	if (CTX_DATA_COUNT(C, selected_bones) == 0)
		return OPERATOR_CANCELLED;
	
	/* Select mirrored bones */
	if (arm->flag & ARM_MIRROR_EDIT) {
		for (curBone=arm->edbo->first; curBone; curBone=curBone->next) {
			if (arm->layer & curBone->layer) {
				if (curBone->flag & BONE_SELECTED) {
					next = ED_armature_bone_get_mirrored(arm->edbo, curBone);
					if (next)
						next->flag |= BONE_SELECTED;
				}
			}
		}
	}
	
	/*  First erase any associated pose channel */
	if (obedit->pose) {
		bPoseChannel *pchan, *next;
		for (pchan=obedit->pose->chanbase.first; pchan; pchan=next) {
			next= pchan->next;
			curBone = editbone_name_exists(arm->edbo, pchan->name);
			
			if (curBone && (curBone->flag & BONE_SELECTED) && (arm->layer & curBone->layer)) {
				free_pose_channel(pchan);
				BLI_freelinkN (&obedit->pose->chanbase, pchan);
			}
			else {
				for (con= pchan->constraints.first; con; con= con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct= targets.first; ct; ct= ct->next) {
							if (ct->tar == obedit) {
								if (ct->subtarget[0]) {
									curBone = editbone_name_exists(arm->edbo, ct->subtarget);
									if (curBone && (curBone->flag & BONE_SELECTED) && (arm->layer & curBone->layer)) {
										con->flag |= CONSTRAINT_DISABLE;
										ct->subtarget[0]= 0;
									}
								}
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 0);
					}
				}
			}
		}
	}
	
	
	for (curBone=arm->edbo->first;curBone;curBone=next) {
		next=curBone->next;
		if (arm->layer & curBone->layer) {
			if (curBone->flag & BONE_SELECTED) {
				if(curBone==arm->act_edbone) arm->act_edbone= NULL;
				ED_armature_edit_bone_remove(arm, curBone);
			}
		}
	}
	
	
	ED_armature_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Selected Bone(s)";
	ot->idname= "ARMATURE_OT_delete";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = armature_delete_selected_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* toggle==0: deselect
 * toggle==1: swap (based on test)
 * toggle==2: only active tag
 * toggle==3: swap (no test)
 */
void ED_armature_deselectall(Object *obedit, int toggle, int doundo)
{
	bArmature *arm= obedit->data;
	EditBone	*eBone;
	int			sel=1;
	
	if(toggle==1) {
		/*	Determine if there are any selected bones
		And therefore whether we are selecting or deselecting */
		for (eBone=arm->edbo->first;eBone;eBone=eBone->next){
			//			if(arm->layer & eBone->layer) {
			if (eBone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)){
				sel=0;
				break;
			}
			//			}
		}
	}
	else sel= toggle;
	
	if(sel==2) {
		arm->act_edbone= NULL;
	} else {
		/*	Set the flags */
		for (eBone=arm->edbo->first;eBone;eBone=eBone->next) {
			if (sel==3) {
				/* invert selection of bone */
				if ((arm->layer & eBone->layer) && (eBone->flag & BONE_HIDDEN_A)==0) {
					eBone->flag ^= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					if(arm->act_edbone==eBone)
						arm->act_edbone= NULL;
				}
			}
			else if (sel==1) {
				/* select bone */
				if(arm->layer & eBone->layer && (eBone->flag & BONE_HIDDEN_A)==0) {
					eBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					if(eBone->parent)
						eBone->parent->flag |= (BONE_TIPSEL);
				}
			}
			else {
				/* deselect bone */
				eBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				if(arm->act_edbone==eBone)
					arm->act_edbone= NULL;
			}
		}
	}
	
	ED_armature_sync_selection(arm->edbo);
	if (doundo) {
		if (sel==1) BIF_undo_push("Select All");
		else BIF_undo_push("Deselect All");
	}
}


/* context: editmode armature in view3d */
int mouse_armature(bContext *C, short mval[2], int extend)
{
	Object *obedit= CTX_data_edit_object(C);
	bArmature *arm= obedit->data;
	ViewContext vc;
	EditBone *nearBone = NULL;
	int	selmask;

	view3d_set_viewcontext(C, &vc);
	
	BIF_sk_selectStroke(C, mval, extend);
	
	nearBone= get_nearest_editbonepoint(&vc, mval, arm->edbo, 1, &selmask);
	if (nearBone) {

		if (!extend)
			ED_armature_deselectall(obedit, 0, 0);
		
		/* by definition the non-root connected bones have no root point drawn,
	       so a root selection needs to be delivered to the parent tip */
		
		if(selmask & BONE_SELECTED) {
			if(nearBone->parent && (nearBone->flag & BONE_CONNECTED)) {
				/* click in a chain */
				if(extend) {
					/* hold shift inverts this bone's selection */
					if(nearBone->flag & BONE_SELECTED) {
						/* deselect this bone */
						nearBone->flag &= ~(BONE_TIPSEL|BONE_SELECTED);
						/* only deselect parent tip if it is not selected */
						if(!(nearBone->parent->flag & BONE_SELECTED))
							nearBone->parent->flag &= ~BONE_TIPSEL;
					}
					else {
						/* select this bone */
						nearBone->flag |= BONE_TIPSEL;
						nearBone->parent->flag |= BONE_TIPSEL;
					}
				}
				else {
					/* select this bone */
					nearBone->flag |= BONE_TIPSEL;
					nearBone->parent->flag |= BONE_TIPSEL;
				}
			}
			else {
				if(extend) {
					/* hold shift inverts this bone's selection */
					if(nearBone->flag & BONE_SELECTED)
					   nearBone->flag &= ~(BONE_TIPSEL|BONE_ROOTSEL);
					else
						nearBone->flag |= (BONE_TIPSEL|BONE_ROOTSEL);
				}
				else nearBone->flag |= (BONE_TIPSEL|BONE_ROOTSEL);
			}
		}
		else {
			if (extend && (nearBone->flag & selmask))
				nearBone->flag &= ~selmask;
			else
				nearBone->flag |= selmask;
		}
		
		ED_armature_sync_selection(arm->edbo);
		
		if(nearBone) {
			/* then now check for active status */
			if(nearBone->flag & BONE_SELECTED) arm->act_edbone= nearBone;
		}
		
		WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, vc.obedit);
		return 1;
	}

	return 0;
}

void ED_armature_edit_free(struct Object *ob)
{
	bArmature *arm= ob->data;
	EditBone *eBone;
	
	/*	Clear the editbones list */
	if (arm->edbo) {
		if (arm->edbo->first) {
			for (eBone=arm->edbo->first; eBone; eBone=eBone->next) {
				if (eBone->prop) {
					IDP_FreeProperty(eBone->prop);
					MEM_freeN(eBone->prop);
				}
			}

			BLI_freelistN(arm->edbo);
		}

		MEM_freeN(arm->edbo);
		arm->edbo= NULL;
	}
}

void ED_armature_edit_remake(Object *obedit)
{
	if(okee("Reload original data")==0) return;
	
	ED_armature_to_edit(obedit);
	
//	BIF_undo_push("Delete bone");
}

/* Put armature in EditMode */
void ED_armature_to_edit(Object *ob)
{
	bArmature *arm= ob->data;
	
	ED_armature_edit_free(ob);
	arm->edbo= MEM_callocN(sizeof(ListBase), "edbo armature");
	arm->act_edbone= make_boneList(arm->edbo, &arm->bonebase, NULL, arm->act_bone);
	arm->act_bone= NULL;

//	BIF_freeTemplates(); /* force template update when entering editmode */
}


/* adjust bone roll to align Z axis with vector
 * vec is in local space and is normalized
 */
float ED_rollBoneToVector(EditBone *bone, float new_up_axis[3])
{
	float mat[3][3], nor[3], up_axis[3], vec[3];
	float roll;

	sub_v3_v3v3(nor, bone->tail, bone->head);
	
	vec_roll_to_mat3(nor, 0, mat);
	VECCOPY(up_axis, mat[2]);
	
	roll = angle_normalized_v3v3(new_up_axis, up_axis);
	
	cross_v3_v3v3(vec, up_axis, new_up_axis);
	
	if (dot_v3v3(vec, nor) < 0)
	{
		roll = -roll;
	}
	
	return roll;
}


/* Set roll value for given bone -> Z-Axis Point up (original method) */
static void auto_align_ebone_zaxisup(Scene *scene, View3D *v3d, EditBone *ebone)
{
	float	delta[3], curmat[3][3];
	float	xaxis[3]={1.0f, 0.0f, 0.0f}, yaxis[3], zaxis[3]={0.0f, 0.0f, 1.0f};
	float	targetmat[3][3], imat[3][3], diffmat[3][3];
	
	/* Find the current bone matrix */
	sub_v3_v3v3(delta, ebone->tail, ebone->head);
	vec_roll_to_mat3(delta, 0.0f, curmat);
	
	/* Make new matrix based on y axis & z-up */
	VECCOPY(yaxis, curmat[1]);
	
	unit_m3(targetmat);
	VECCOPY(targetmat[0], xaxis);
	VECCOPY(targetmat[1], yaxis);
	VECCOPY(targetmat[2], zaxis);
	normalize_m3(targetmat);
	
	/* Find the difference between the two matrices */
	invert_m3_m3(imat, targetmat);
	mul_m3_m3m3(diffmat, imat, curmat);
	
	// old-method... let's see if using mat3_to_vec_roll is more accurate
	//ebone->roll = atan2(diffmat[2][0], diffmat[2][2]);  
	mat3_to_vec_roll(diffmat, delta, &ebone->roll);
}

void auto_align_ebone_topoint(EditBone *ebone, float *cursor)
{
	float	delta[3], curmat[3][3];
	float	mat[4][4], tmat[4][4], imat[4][4];
	float 	rmat[4][4], rot[3];
	float	vec[3];

	/* find the current bone matrix as a 4x4 matrix (in Armature Space) */
	sub_v3_v3v3(delta, ebone->tail, ebone->head);
	vec_roll_to_mat3(delta, ebone->roll, curmat);
	copy_m4_m3(mat, curmat);
	VECCOPY(mat[3], ebone->head);
	
	/* multiply bone-matrix by object matrix (so that bone-matrix is in WorldSpace) */
	invert_m4_m4(imat, mat);
	
	/* find position of cursor relative to bone */
	mul_v3_m4v3(vec, imat, cursor);
	
	/* check that cursor is in usable position */
	if ((IS_EQ(vec[0], 0)==0) && (IS_EQ(vec[2], 0)==0)) {
		/* Compute a rotation matrix around y */
		rot[1] = (float)atan2(vec[0], vec[2]);
		rot[0] = rot[2] = 0.0f;
		eul_to_mat4( rmat,rot);
		
		/* Multiply the bone matrix by rotation matrix. This should be new bone-matrix */
		mul_m4_m4m4(tmat, rmat, mat);
		copy_m3_m4(curmat, tmat);
		
		/* Now convert from new bone-matrix, back to a roll value (in radians) */
		mat3_to_vec_roll(curmat, delta, &ebone->roll);
	}
}

static void auto_align_ebone_tocursor(Scene *scene, View3D *v3d, EditBone *ebone)
{
	float cursor_local[3];
	float  	*cursor= give_cursor(scene, v3d);
	float imat[3][3];

	copy_m3_m4(imat, scene->obedit->obmat);
	invert_m3(imat);
	copy_v3_v3(cursor_local, cursor);
	mul_m3_v3(imat, cursor_local);
	auto_align_ebone_topoint(ebone, cursor_local);
}

static EnumPropertyItem prop_calc_roll_types[] = {
	{0, "GLOBALUP", 0, "Z-Axis Up", ""},
	{1, "CURSOR", 0, "Z-Axis to Cursor", ""},
	{0, NULL, 0, NULL, NULL}
};

static int armature_calc_roll_exec(bContext *C, wmOperator *op) 
{
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	Object *ob= CTX_data_edit_object(C);
	void (*roll_func)(Scene *, View3D *, EditBone *) = NULL;
	
	/* specific method used to calculate roll depends on mode */
	switch (RNA_enum_get(op->ptr, "type")) {
		case 1:  /* Z-Axis point towards cursor */
			roll_func= auto_align_ebone_tocursor;
			break;
		default: /* Z-Axis Point Up */
			roll_func= auto_align_ebone_zaxisup;
			break;
	}
	
	/* recalculate roll on selected bones */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) {
		/* roll func is a callback which assumes that all is well */
		roll_func(scene, v3d, ebone);
	}
	CTX_DATA_END;
	

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_calculate_roll(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Recalculate Roll";
	ot->idname= "ARMATURE_OT_calculate_roll";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = armature_calc_roll_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", prop_calc_roll_types, 0, "Type", "");
}

/* **************** undo for armatures ************** */

typedef struct UndoArmature {
	EditBone *act_edbone;
	ListBase lb;
} UndoArmature;

static void undoBones_to_editBones(void *uarmv, void *armv)
{
	UndoArmature *uarm= uarmv;
	bArmature *arm= armv;
	EditBone *ebo, *newebo;
	
	BLI_freelistN(arm->edbo);
	
	/* copy  */
	for(ebo= uarm->lb.first; ebo; ebo= ebo->next) {
		newebo= MEM_dupallocN(ebo);
		ebo->temp= newebo;
		BLI_addtail(arm->edbo, newebo);
	}
	
	/* active bone */
	if(uarm->act_edbone) {
		ebo= uarm->act_edbone;
		arm->act_edbone= ebo->temp;
	}

	/* set pointers */
	for(newebo= arm->edbo->first; newebo; newebo= newebo->next) {
		if(newebo->parent) newebo->parent= newebo->parent->temp;
	}
	/* be sure they dont hang ever */
	for(newebo= arm->edbo->first; newebo; newebo= newebo->next) {
		newebo->temp= NULL;
	}
}

static void *editBones_to_undoBones(void *armv)
{
	bArmature *arm= armv;
	UndoArmature *uarm;
	EditBone *ebo, *newebo;
	
	uarm= MEM_callocN(sizeof(UndoArmature), "listbase undo");
	
	/* copy */
	for(ebo= arm->edbo->first; ebo; ebo= ebo->next) {
		newebo= MEM_dupallocN(ebo);
		ebo->temp= newebo;
		BLI_addtail(&uarm->lb, newebo);
	}
	
	/* active bone */
	if(arm->act_edbone) {
		ebo= arm->act_edbone;
		uarm->act_edbone= ebo->temp;
	}

	/* set pointers */
	for(newebo= uarm->lb.first; newebo; newebo= newebo->next) {
		if(newebo->parent) newebo->parent= newebo->parent->temp;
	}
	
	return uarm;
}

static void free_undoBones(void *uarmv)
{
	UndoArmature *uarm= uarmv;
	
	BLI_freelistN(&uarm->lb);
	MEM_freeN(uarm);
}

static void *get_armature_edit(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_ARMATURE) {
		return obedit->data;
	}
	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_armature(bContext *C, char *name)
{
	// XXX solve getdata()
	undo_editmode_push(C, name, get_armature_edit, free_undoBones, undoBones_to_editBones, editBones_to_undoBones, NULL);
}



/* **************** END EditMode stuff ********************** */
/* *************** Adding stuff in editmode *************** */

/* default bone add, returns it selected, but without tail set */
EditBone *ED_armature_edit_bone_add(bArmature *arm, char *name)
{
	EditBone *bone= MEM_callocN(sizeof(EditBone), "eBone");
	
	BLI_strncpy(bone->name, name, 32);
	unique_editbone_name(arm->edbo, bone->name, NULL);
	
	BLI_addtail(arm->edbo, bone);
	
	bone->flag |= BONE_TIPSEL;
	bone->weight= 1.0f;
	bone->dist= 0.25f;
	bone->xwidth= 0.1f;
	bone->zwidth= 0.1f;
	bone->ease1= 1.0f;
	bone->ease2= 1.0f;
	bone->rad_head= 0.10f;
	bone->rad_tail= 0.05f;
	bone->segments= 1;
	bone->layer= arm->layer;
	
	return bone;
}

/* v3d and rv3d are allowed to be NULL */
void add_primitive_bone(Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	Object *obedit= scene->obedit; // XXX get from context
	float		obmat[3][3], curs[3], viewmat[3][3], totmat[3][3], imat[3][3];
	EditBone	*bone;
	
	VECCOPY(curs, give_cursor(scene, v3d));	

	/* Get inverse point for head and orientation for tail */
	invert_m4_m4(obedit->imat, obedit->obmat);
	mul_m4_v3(obedit->imat, curs);

	if (rv3d && (U.flag & USER_ADD_VIEWALIGNED))
		copy_m3_m4(obmat, rv3d->viewmat);
	else unit_m3(obmat);
	
	copy_m3_m4(viewmat, obedit->obmat);
	mul_m3_m3m3(totmat, obmat, viewmat);
	invert_m3_m3(imat, totmat);
	
	ED_armature_deselectall(obedit, 0, 0);
	
	/*	Create a bone	*/
	bone= ED_armature_edit_bone_add(obedit->data, "Bone");

	VECCOPY(bone->head, curs);
	
	if (rv3d && (U.flag & USER_ADD_VIEWALIGNED))
		add_v3_v3v3(bone->tail, bone->head, imat[1]);	// bone with unit length 1
	else
		add_v3_v3v3(bone->tail, bone->head, imat[2]);	// bone with unit length 1, pointing up Z
	
}


/* previously addvert_armature */
/* the ctrl-click method */
static int armature_click_extrude_exec(bContext *C, wmOperator *op)
{
	View3D *v3d;
	bArmature *arm;
	EditBone *ebone, *newbone, *flipbone;
	float *curs, mat[3][3],imat[3][3];
	int a, to_root= 0;
	Object *obedit;
	Scene *scene;

	scene = CTX_data_scene(C);
	v3d= CTX_wm_view3d(C);
	obedit= CTX_data_edit_object(C);
	arm= obedit->data;
	
	/* find the active or selected bone */
	for (ebone = arm->edbo->first; ebone; ebone=ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			if (ebone->flag & BONE_TIPSEL || arm->act_edbone == ebone)
				break;
		}
	}
	
	if (ebone==NULL) {
		for (ebone = arm->edbo->first; ebone; ebone=ebone->next) {
			if (EBONE_VISIBLE(arm, ebone)) {
				if (ebone->flag & BONE_ROOTSEL || arm->act_edbone == ebone)
					break;
			}
		}
		if (ebone == NULL) 
			return OPERATOR_CANCELLED;
		
		to_root= 1;
	}
	
	ED_armature_deselectall(obedit, 0, 0);
	
	/* we re-use code for mirror editing... */
	flipbone= NULL;
	if (arm->flag & ARM_MIRROR_EDIT)
		flipbone= ED_armature_bone_get_mirrored(arm->edbo, ebone);

	for (a=0; a<2; a++) {
		if (a==1) {
			if (flipbone==NULL)
				break;
			else {
				SWAP(EditBone *, flipbone, ebone);
			}
		}
		
		newbone= ED_armature_edit_bone_add(arm, ebone->name);
		arm->act_edbone= newbone;
		
		if (to_root) {
			VECCOPY(newbone->head, ebone->head);
			newbone->rad_head= ebone->rad_tail;
			newbone->parent= ebone->parent;
		}
		else {
			VECCOPY(newbone->head, ebone->tail);
			newbone->rad_head= ebone->rad_tail;
			newbone->parent= ebone;
			newbone->flag |= BONE_CONNECTED;
		}
		
		curs= give_cursor(scene, v3d);
		VECCOPY(newbone->tail, curs);
		sub_v3_v3v3(newbone->tail, newbone->tail, obedit->obmat[3]);
		
		if (a==1) 
			newbone->tail[0]= -newbone->tail[0];
		
		copy_m3_m4(mat, obedit->obmat);
		invert_m3_m3(imat, mat);
		mul_m3_v3(imat, newbone->tail);
		
		newbone->length= len_v3v3(newbone->head, newbone->tail);
		newbone->rad_tail= newbone->length*0.05f;
		newbone->dist= newbone->length*0.25f;
		
	}
	
	ED_armature_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

static int armature_click_extrude_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	/* TODO most of this code is copied from set3dcursor_invoke,
	   it would be better to reuse code in set3dcursor_invoke */

	/* temporarily change 3d cursor position */
	Scene *scene;
	ARegion *ar;
	View3D *v3d;
	RegionView3D *rv3d;
	float dx, dy, fz, *fp = NULL, dvec[3], oldcurs[3];
	short mx, my, mval[2];
	int retv;

	scene= CTX_data_scene(C);
	ar= CTX_wm_region(C);
	v3d = CTX_wm_view3d(C);
	rv3d= CTX_wm_region_view3d(C);
	
	fp= give_cursor(scene, v3d);
	
	VECCOPY(oldcurs, fp);
	
	mx= event->x - ar->winrct.xmin;
	my= event->y - ar->winrct.ymin;
	project_short_noclip(ar, fp, mval);
	
	initgrabz(rv3d, fp[0], fp[1], fp[2]);
	
	if(mval[0]!=IS_CLIPPED) {
		
		window_to_3d_delta(ar, dvec, mval[0]-mx, mval[1]-my);
		sub_v3_v3v3(fp, fp, dvec);
	}
	else {
		
		dx= ((float)(mx-(ar->winx/2)))*rv3d->zfac/(ar->winx/2);
		dy= ((float)(my-(ar->winy/2)))*rv3d->zfac/(ar->winy/2);
		
		fz= rv3d->persmat[0][3]*fp[0]+ rv3d->persmat[1][3]*fp[1]+ rv3d->persmat[2][3]*fp[2]+ rv3d->persmat[3][3];
		fz= fz/rv3d->zfac;
		
		fp[0]= (rv3d->persinv[0][0]*dx + rv3d->persinv[1][0]*dy+ rv3d->persinv[2][0]*fz)-rv3d->ofs[0];
		fp[1]= (rv3d->persinv[0][1]*dx + rv3d->persinv[1][1]*dy+ rv3d->persinv[2][1]*fz)-rv3d->ofs[1];
		fp[2]= (rv3d->persinv[0][2]*dx + rv3d->persinv[1][2]*dy+ rv3d->persinv[2][2]*fz)-rv3d->ofs[2];
	}

	/* extrude to the where new cursor is and store the operation result */
	retv= armature_click_extrude_exec(C, op);

	/* restore previous 3d cursor position */
	VECCOPY(fp, oldcurs);

	return retv;
}

void ARMATURE_OT_click_extrude(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Click-Extrude";
	ot->idname= "ARMATURE_OT_click_extrude";
	
	/* api callbacks */
	ot->invoke = armature_click_extrude_invoke;
	ot->exec = armature_click_extrude_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
}

/* adds an EditBone between the nominated locations (should be in the right space) */
static EditBone *add_points_bone (Object *obedit, float head[], float tail[]) 
{
	EditBone *ebo;
	
	ebo= ED_armature_edit_bone_add(obedit->data, "Bone");
	
	VECCOPY(ebo->head, head);
	VECCOPY(ebo->tail, tail);
	
	return ebo;
}


static EditBone *get_named_editbone(ListBase *edbo, char *name)
{
	EditBone  *eBone;

	if (name) {
		for (eBone=edbo->first; eBone; eBone=eBone->next) {
			if (!strcmp(name, eBone->name))
				return eBone;
		}
	}

	return NULL;
}

/* Call this before doing any duplications
 * */
void preEditBoneDuplicate(ListBase *editbones)
{
	EditBone *eBone;
	
	/* clear temp */
	for (eBone = editbones->first; eBone; eBone = eBone->next)
	{
		eBone->temp = NULL;
	}
}

/*
 * Note: When duplicating cross objects, editbones here is the list of bones
 * from the SOURCE object but ob is the DESTINATION object
 * */
void updateDuplicateSubtargetObjects(EditBone *dupBone, ListBase *editbones, Object *src_ob, Object *dst_ob)
{
	/* If an edit bone has been duplicated, lets
	 * update it's constraints if the subtarget
	 * they point to has also been duplicated
	 */
	EditBone     *oldtarget, *newtarget;
	bPoseChannel *pchan;
	bConstraint  *curcon;
	ListBase     *conlist;
	
	if ( (pchan = verify_pose_channel(dst_ob->pose, dupBone->name)) ) {
		if ( (conlist = &pchan->constraints) ) {
			for (curcon = conlist->first; curcon; curcon=curcon->next) {
				/* does this constraint have a subtarget in
				 * this armature?
				 */
				bConstraintTypeInfo *cti= constraint_get_typeinfo(curcon);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(curcon, &targets);
					
					for (ct= targets.first; ct; ct= ct->next) {
						if ((ct->tar == src_ob) && (ct->subtarget[0])) {
							ct->tar = dst_ob; /* update target */ 
							oldtarget = get_named_editbone(editbones, ct->subtarget);
							if (oldtarget) {
								/* was the subtarget bone duplicated too? If
								 * so, update the constraint to point at the 
								 * duplicate of the old subtarget.
								 */
								if (oldtarget->temp) {
									newtarget = (EditBone *) oldtarget->temp;
									strcpy(ct->subtarget, newtarget->name);
								}
							}
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(curcon, &targets, 0);
				}
			}
		}
	}
}

void updateDuplicateSubtarget(EditBone *dupBone, ListBase *editbones, Object *ob)
{
	updateDuplicateSubtargetObjects(dupBone, editbones, ob, ob);
}


EditBone *duplicateEditBoneObjects(EditBone *curBone, char *name, ListBase *editbones, Object *src_ob, Object *dst_ob)
{
	EditBone *eBone = MEM_mallocN(sizeof(EditBone), "addup_editbone");
	
	/*	Copy data from old bone to new bone */
	memcpy(eBone, curBone, sizeof(EditBone));
	
	curBone->temp = eBone;
	eBone->temp = curBone;
	
	if (name != NULL)
	{
		BLI_strncpy(eBone->name, name, 32);
	}

	unique_editbone_name(editbones, eBone->name, NULL);
	BLI_addtail(editbones, eBone);
	
	/* copy the ID property */
	if(curBone->prop)
		eBone->prop= IDP_CopyProperty(curBone->prop);

	/* Lets duplicate the list of constraints that the
	 * current bone has.
	 */
	if (src_ob->pose) {
		bPoseChannel *chanold, *channew;
		
		chanold = verify_pose_channel(src_ob->pose, curBone->name);
		if (chanold) {
			/* WARNING: this creates a new posechannel, but there will not be an attached bone
			 *		yet as the new bones created here are still 'EditBones' not 'Bones'.
			 */
			channew= verify_pose_channel(dst_ob->pose, eBone->name);

			if(channew) {
				duplicate_pose_channel_data(channew, chanold);
			}
		}
	}
	
	return eBone;
}

EditBone *duplicateEditBone(EditBone *curBone, char *name, ListBase *editbones, Object *ob)
{
	return duplicateEditBoneObjects(curBone, name, editbones, ob, ob);
}

/* previously adduplicate_armature */
static int armature_duplicate_selected_exec(bContext *C, wmOperator *op)
{
	bArmature *arm;
	EditBone	*eBone = NULL;
	EditBone	*curBone;
	EditBone	*firstDup=NULL;	/*	The beginning of the duplicated bones in the edbo list */

	Object *obedit= CTX_data_edit_object(C);
	arm= obedit->data;

	/* cancel if nothing selected */
	if (CTX_DATA_COUNT(C, selected_bones) == 0)
	  return OPERATOR_CANCELLED;
	
	ED_armature_sync_selection(arm->edbo); // XXX why is this needed?

	preEditBoneDuplicate(arm->edbo);

	/* Select mirrored bones */
	if (arm->flag & ARM_MIRROR_EDIT) {
		for (curBone=arm->edbo->first; curBone; curBone=curBone->next) {
			if (EBONE_VISIBLE(arm, curBone)) {
				if (curBone->flag & BONE_SELECTED) {
					eBone = ED_armature_bone_get_mirrored(arm->edbo, curBone);
					if (eBone)
						eBone->flag |= BONE_SELECTED;
				}
			}
		}
	}

	
	/*	Find the selected bones and duplicate them as needed */
	for (curBone=arm->edbo->first; curBone && curBone!=firstDup; curBone=curBone->next) {
		if (EBONE_VISIBLE(arm, curBone)) {
			if (curBone->flag & BONE_SELECTED) {
				
				eBone= duplicateEditBone(curBone, curBone->name, arm->edbo, obedit);
				
				if (!firstDup)
					firstDup=eBone;

			}
		}
	}

	/*	Run though the list and fix the pointers */
	for (curBone=arm->edbo->first; curBone && curBone!=firstDup; curBone=curBone->next) {
		if (EBONE_VISIBLE(arm, curBone)) {
			if (curBone->flag & BONE_SELECTED) {
				eBone=(EditBone*) curBone->temp;
				
				if (!curBone->parent) {
					/* If this bone has no parent,
					 * Set the duplicate->parent to NULL
					 */
					eBone->parent = NULL;
				}
				else if (curBone->parent->temp) {
					/* If this bone has a parent that was duplicated,
					 * Set the duplicate->parent to the curBone->parent->temp
					 */
					eBone->parent= (EditBone *)curBone->parent->temp;
				}
				else {
					/* If this bone has a parent that IS not selected,
					 * Set the duplicate->parent to the curBone->parent
					 */
					eBone->parent=(EditBone*) curBone->parent; 
					eBone->flag &= ~BONE_CONNECTED;
				}
				
				/* Lets try to fix any constraint subtargets that might
				 * have been duplicated 
				 */
				updateDuplicateSubtarget(eBone, arm->edbo, obedit);
			}
		}
	} 
	
	/* correct the active bone */
	if(arm->act_edbone) {
		eBone= arm->act_edbone;
		if(eBone->temp)
			arm->act_edbone= eBone->temp;
	}

	/*	Deselect the old bones and select the new ones */
	for (curBone=arm->edbo->first; curBone && curBone!=firstDup; curBone=curBone->next) {
		if (EBONE_VISIBLE(arm, curBone))
			curBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
	}

	ED_armature_validate_active(arm);

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}


void ARMATURE_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Duplicate Selected Bone(s)";
	ot->idname= "ARMATURE_OT_duplicate";
	
	/* api callbacks */
	ot->exec = armature_duplicate_selected_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


/* *************** END Adding stuff in editmode *************** */
/* ************** Add/Remove stuff in editmode **************** */

/* temporary data-structure for merge/fill bones */
typedef struct EditBonePoint {
	struct EditBonePoint *next, *prev;
	
	EditBone *head_owner;		/* EditBone which uses this point as a 'head' point */
	EditBone *tail_owner;		/* EditBone which uses this point as a 'tail' point */
	
	float vec[3];				/* the actual location of the point in local/EditMode space */
} EditBonePoint;

/* find chain-tips (i.e. bones without children) */
static void chains_find_tips (ListBase *edbo, ListBase *list)
{
	EditBone *curBone, *ebo;
	LinkData *ld;
	
	/* note: this is potentially very slow ... there's got to be a better way */
	for (curBone= edbo->first; curBone; curBone= curBone->next) {
		short stop= 0;
		
		/* is this bone contained within any existing chain? (skip if so) */
		for (ld= list->first; ld; ld= ld->next) {
			for (ebo= ld->data; ebo; ebo= ebo->parent) {
				if (ebo == curBone) {
					stop= 1;
					break;
				}
			}
			
			if (stop) break;
		}
		/* skip current bone if it is part of an existing chain */
		if (stop) continue;
		
		/* is any existing chain part of the chain formed by this bone? */
		stop= 0;
		for (ebo= curBone->parent; ebo; ebo= ebo->parent) {
			for (ld= list->first; ld; ld= ld->next) {
				if (ld->data == ebo) {
					ld->data= curBone;
					stop= 1;
					break;
				}
			}
			
			if (stop) break;
		}
		/* current bone has already been added to a chain? */
		if (stop) continue;
		
		/* add current bone to a new chain */
		ld= MEM_callocN(sizeof(LinkData), "BoneChain");
		ld->data= curBone;
		BLI_addtail(list, ld);
	}
}

/* --------------------- */

static void fill_add_joint (EditBone *ebo, short eb_tail, ListBase *points)
{
	EditBonePoint *ebp;
	float vec[3];
	short found= 0;
	
	if (eb_tail) {
		VECCOPY(vec, ebo->tail);
	}
	else {
		VECCOPY(vec, ebo->head);
	}
	
	for (ebp= points->first; ebp; ebp= ebp->next) {
		if (equals_v3v3(ebp->vec, vec)) {			
			if (eb_tail) {
				if ((ebp->head_owner) && (ebp->head_owner->parent == ebo)) {
					/* so this bone's tail owner is this bone */
					ebp->tail_owner= ebo;
					found= 1;
					break;
				}
			}
			else {
				if ((ebp->tail_owner) && (ebo->parent == ebp->tail_owner)) {
					/* so this bone's head owner is this bone */
					ebp->head_owner= ebo;
					found = 1;
					break;
				}
			}
		}
	}
	
	/* allocate a new point if no existing point was related */
	if (found == 0) {
		ebp= MEM_callocN(sizeof(EditBonePoint), "EditBonePoint");
		
		if (eb_tail) {
			VECCOPY(ebp->vec, ebo->tail);
			ebp->tail_owner= ebo;
		}
		else {
			VECCOPY(ebp->vec, ebo->head);
			ebp->head_owner= ebo;
		}
		
		BLI_addtail(points, ebp);
	}
}

/* bone adding between selected joints */
static int armature_fill_bones_exec (bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	bArmature *arm= (obedit) ? obedit->data : NULL;
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= CTX_wm_view3d(C);
	EditBone *newbone=NULL;
	ListBase points = {NULL, NULL};
	int count;
	
	/* sanity checks */
	if ELEM(NULL, obedit, arm)
		return OPERATOR_CANCELLED;
	
	/* loop over all bones, and only consider if visible */
	CTX_DATA_BEGIN(C, EditBone *, ebone, visible_bones)
	{
		if (!(ebone->flag & BONE_CONNECTED) && (ebone->flag & BONE_ROOTSEL))
			fill_add_joint(ebone, 0, &points);
		if (ebone->flag & BONE_TIPSEL) 
			fill_add_joint(ebone, 1, &points);
	}
	CTX_DATA_END;
	
	/* the number of joints determines how we fill:
	 *	1) between joint and cursor (joint=head, cursor=tail)
	 *	2) between the two joints (order is dependent on active-bone/hierachy)
	 * 	3+) error (a smarter method involving finding chains needs to be worked out
	 */
	count= BLI_countlist(&points);
	
	if (count == 0) {
		BKE_report(op->reports, RPT_ERROR, "No joints selected");
		return OPERATOR_CANCELLED;
	}
	else if (count == 1) {
		EditBonePoint *ebp;
		float *fp, curs[3];
		
		/* Get Points - selected joint */
		ebp= (EditBonePoint *)points.first;
		
		/* Get points - cursor (tail) */
		fp= give_cursor(scene, v3d);
		VECCOPY (curs, fp);	
		
		invert_m4_m4(obedit->imat, obedit->obmat);
		mul_m4_v3(obedit->imat, curs);
		
		/* Create a bone */
		newbone= add_points_bone(obedit, ebp->vec, curs);
	}
	else if (count == 2) {
		EditBonePoint *ebp, *ebp2;
		float head[3], tail[3];
		short headtail = 0;
		
		/* check that the points don't belong to the same bone */
		ebp= (EditBonePoint *)points.first;
		ebp2= ebp->next;
		
		if ((ebp->head_owner==ebp2->tail_owner) && (ebp->head_owner!=NULL)) {
			BKE_report(op->reports, RPT_ERROR, "Same bone selected...");
			BLI_freelistN(&points);
			return OPERATOR_CANCELLED;
		}
		if ((ebp->tail_owner==ebp2->head_owner) && (ebp->tail_owner!=NULL)) {
			BKE_report(op->reports, RPT_ERROR, "Same bone selected...");
			BLI_freelistN(&points);
			return OPERATOR_CANCELLED;
		}
		
		/* find which one should be the 'head' */
		if ((ebp->head_owner && ebp2->head_owner) || (ebp->tail_owner && ebp2->tail_owner)) {
			/* rule: whichever one is closer to 3d-cursor */
			float curs[3], *fp= give_cursor(scene, v3d);
			float vecA[3], vecB[3];
			float distA, distB;
			
			/* get cursor location */
			VECCOPY(curs, fp);	
			
			invert_m4_m4(obedit->imat, obedit->obmat);
			mul_m4_v3(obedit->imat, curs);
			
			/* get distances */
			sub_v3_v3v3(vecA, ebp->vec, curs);
			sub_v3_v3v3(vecB, ebp2->vec, curs);
			distA= len_v3(vecA);
			distB= len_v3(vecB);
			
			/* compare distances - closer one therefore acts as direction for bone to go */
			headtail= (distA < distB) ? 2 : 1;
		}
		else if (ebp->head_owner) {
			headtail = 1;
		}
		else if (ebp2->head_owner) {
			headtail = 2;
		}
		
		/* assign head/tail combinations */
		if (headtail == 2) {
			VECCOPY(head, ebp->vec);
			VECCOPY(tail, ebp2->vec);
		}
		else if (headtail == 1) {
			VECCOPY(head, ebp2->vec);
			VECCOPY(tail, ebp->vec);
		}
		
		/* add new bone and parent it to the appropriate end */
		if (headtail) {
			newbone= add_points_bone(obedit, head, tail);
			
			/* do parenting (will need to set connected flag too) */
			if (headtail == 2) {
				/* ebp tail or head - tail gets priority */
				if (ebp->tail_owner)
					newbone->parent= ebp->tail_owner;
				else
					newbone->parent= ebp->head_owner;
			}
			else {
				/* ebp2 tail or head - tail gets priority */
				if (ebp2->tail_owner)
					newbone->parent= ebp2->tail_owner;
				else
					newbone->parent= ebp2->head_owner;
			}
			
			newbone->flag |= BONE_CONNECTED;
		}
	}
	else {
		// FIXME.. figure out a method for multiple bones
		BKE_reportf(op->reports, RPT_ERROR, "Too many points selected: %d \n", count); 
		BLI_freelistN(&points);
		return OPERATOR_CANCELLED;
	}
	
	/* free points */
	BLI_freelistN(&points);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_fill (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Fill Between Joints";
	ot->idname= "ARMATURE_OT_fill";
	ot->description= "Add bone between selected joint(s) and/or 3D-Cursor.";
	
	/* callbacks */
	ot->exec= armature_fill_bones_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* --------------------- */

/* this function merges between two bones, removes them and those in-between, 
 * and adjusts the parent relationships for those in-between
 */
static void bones_merge(Object *obedit, EditBone *start, EditBone *end, EditBone *endchild, ListBase *chains)
{
	bArmature *arm= obedit->data;
	EditBone *ebo, *ebone, *newbone;
	LinkData *chain;
	float head[3], tail[3];
	
	/* check if same bone */
	if (start == end) {
		if (G.f & G_DEBUG) {
			printf("Error: same bone! \n");
			printf("\tstart = %s, end = %s \n", start->name, end->name);
		}
	}
	
	/* step 1: add a new bone
	 *	- head = head/tail of start (default head)
	 *	- tail = head/tail of end (default tail)
	 *	- parent = parent of start
	 */
	if ((start->flag & BONE_TIPSEL) && ((start->flag & BONE_SELECTED) || start==arm->act_edbone)==0) {
		VECCOPY(head, start->tail);
	}
	else {
		VECCOPY(head, start->head);
	}
	if ((end->flag & BONE_ROOTSEL) && ((end->flag & BONE_SELECTED) || end==arm->act_edbone)==0) {
		VECCOPY(tail, end->head);
	}
	else {
		VECCOPY(tail, end->tail);
	}
	newbone= add_points_bone(obedit, head, tail);
	newbone->parent = start->parent;
	
	/* step 2a: parent children of in-between bones to newbone */
	for (chain= chains->first; chain; chain= chain->next) {
		/* ick: we need to check if parent of each bone in chain is one of the bones in the */
		for (ebo= chain->data; ebo; ebo= ebo->parent) {
			short found= 0;
			
			/* try to find which bone from the list to be removed, is the parent */
			for (ebone= end; ebone; ebone= ebone->parent) {
				if (ebo->parent == ebone) {
					found= 1;
					break;
				}
			}
			
			/* adjust this bone's parent to newbone then */
			if (found) {
				ebo->parent= newbone;
				break;
			}
		}
	}
	
	/* step 2b: parent child of end to newbone (child from this chain) */
	if (endchild)
		endchild->parent= newbone;
	
	/* step 3: delete all bones between and including start and end */
	for (ebo= end; ebo; ebo= ebone) {
		ebone= (ebo == start) ? (NULL) : (ebo->parent);
		bone_free(arm, ebo);
	}
}


static int armature_merge_exec (bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	bArmature *arm= (obedit) ? obedit->data : NULL;
	short type= RNA_enum_get(op->ptr, "type");
	
	/* sanity checks */
	if ELEM(NULL, obedit, arm)
		return OPERATOR_CANCELLED;
	
	/* for now, there's only really one type of merging that's performed... */
	if (type == 1) {
		/* go down chains, merging bones */
		ListBase chains = {NULL, NULL};
		LinkData *chain, *nchain;
		EditBone *ebo;
		
		/* get chains (ends on chains) */
		chains_find_tips(arm->edbo, &chains);
		if (chains.first == NULL) return OPERATOR_CANCELLED;
		
		/* each 'chain' is the last bone in the chain (with no children) */
		for (chain= chains.first; chain; chain= nchain) {
			EditBone *bstart= NULL, *bend= NULL;
			EditBone *bchild= NULL, *child=NULL;
			
			/* temporarily remove chain from list of chains */
			nchain= chain->next;
			BLI_remlink(&chains, chain);
			
			/* only consider bones that are visible and selected */
			for (ebo=chain->data; ebo; child=ebo, ebo=ebo->parent) {
				/* check if visible + selected */
				if ( EBONE_VISIBLE(arm, ebo) &&
					 ((ebo->flag & BONE_CONNECTED) || (ebo->parent==NULL)) &&
					 ((ebo->flag & BONE_SELECTED) || (ebo==arm->act_edbone)) )
				{
					/* set either end or start (end gets priority, unless it is already set) */
					if (bend == NULL)  {
						bend= ebo;
						bchild= child;
					}
					else 
						bstart= ebo;
				}
				else {
					/* chain is broken... merge any continous segments then clear */
					if (bstart && bend)
						bones_merge(obedit, bstart, bend, bchild, &chains);
					
					bstart = NULL;
					bend = NULL;
					bchild = NULL;
				}
			}
			
			/* merge from bstart to bend if something not merged */
			if (bstart && bend)
				bones_merge(obedit, bstart, bend, bchild, &chains);
			
			/* put back link */
			BLI_insertlinkbefore(&chains, nchain, chain);
		}		
		
		BLI_freelistN(&chains);
	}
	
	/* updates */
	ED_armature_sync_selection(arm->edbo);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, obedit);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_merge (wmOperatorType *ot)
{
	static EnumPropertyItem merge_types[] = {
		{1, "WITHIN_CHAIN", 0, "Within Chains", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name= "Merge Bones";
	ot->idname= "ARMATURE_OT_merge";
	ot->description= "Merge continuous chains of selected bones.";
	
	/* callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= armature_merge_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", merge_types, 0, "Type", "");
}

/* ************** END Add/Remove stuff in editmode ************ */
/* *************** Tools in editmode *********** */


void hide_selected_armature_bones(Scene *scene)
{
	Object *obedit= scene->obedit; // XXX get from context
	bArmature *arm= obedit->data;
	EditBone *ebone;
	
	for (ebone = arm->edbo->first; ebone; ebone=ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			if (ebone->flag & BONE_SELECTED) {
				ebone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL);
				ebone->flag |= BONE_HIDDEN_A;
			}
		}
	}
	ED_armature_validate_active(arm);
	ED_armature_sync_selection(arm->edbo);
	BIF_undo_push("Hide Bones");
}


#if 0 // remove this?
static void hide_unselected_armature_bones(Scene *scene)
{
	Object *obedit= scene->obedit; // XXX get from context
	bArmature *arm= obedit->data;
	EditBone *ebone;
	
	for (ebone = arm->edbo->first; ebone; ebone=ebone->next) {
		bArmature *arm= obedit->data;
		if (EBONE_VISIBLE(arm, ebone)) {
			if (ebone->flag & (BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL));
			else {
				ebone->flag |= BONE_HIDDEN_A;
			}
		}
	}

	ED_armature_validate_active(arm);
	ED_armature_sync_selection(arm->edbo);
	BIF_undo_push("Hide Unselected Bones");
}
#endif

#if 0 // remove this?
void show_all_armature_bones(Scene *scene)
{
	Object *obedit= scene->obedit; // XXX get from context
	bArmature *arm= obedit->data;
	EditBone *ebone;
	
	for (ebone = arm->edbo->first; ebone; ebone=ebone->next) {
		if(arm->layer & ebone->layer) {
			if (ebone->flag & BONE_HIDDEN_A) {
				ebone->flag |= (BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL);
				ebone->flag &= ~BONE_HIDDEN_A;
			}
		}
	}
	ED_armature_validate_active(arm);
	ED_armature_sync_selection(arm->edbo);
	BIF_undo_push("Reveal Bones");
}
#endif

/* previously extrude_armature */
/* context; editmode armature */
/* if forked && mirror-edit: makes two bones with flipped names */
static int armature_extrude_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	bArmature *arm;
	EditBone *newbone, *ebone, *flipbone, *first=NULL;
	int a, totbone= 0, do_extrude;
	int forked = RNA_boolean_get(op->ptr, "forked");

	obedit= CTX_data_edit_object(C);
	arm= obedit->data;

	/* since we allow root extrude too, we have to make sure selection is OK */
	for (ebone = arm->edbo->first; ebone; ebone=ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			if (ebone->flag & BONE_ROOTSEL) {
				if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
					if (ebone->parent->flag & BONE_TIPSEL)
						ebone->flag &= ~BONE_ROOTSEL;
				}
			}
		}
	}
	
	/* Duplicate the necessary bones */
	for (ebone = arm->edbo->first; ((ebone) && (ebone!=first)); ebone=ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			/* we extrude per definition the tip */
			do_extrude= 0;
			if (ebone->flag & (BONE_TIPSEL|BONE_SELECTED))
				do_extrude= 1;
			else if (ebone->flag & BONE_ROOTSEL) {
				/* but, a bone with parent deselected we do the root... */
				if (ebone->parent && (ebone->parent->flag & BONE_TIPSEL));
				else do_extrude= 2;
			}
			
			if (do_extrude) {
				/* we re-use code for mirror editing... */
				flipbone= NULL;
				if (arm->flag & ARM_MIRROR_EDIT) {
					flipbone= ED_armature_bone_get_mirrored(arm->edbo, ebone);
					if (flipbone) {
						forked= 0;	// we extrude 2 different bones
						if (flipbone->flag & (BONE_TIPSEL|BONE_ROOTSEL|BONE_SELECTED))
							/* don't want this bone to be selected... */
							flipbone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL);
					}
					if ((flipbone==NULL) && (forked))
						flipbone= ebone;
				}
				
				for (a=0; a<2; a++) {
					if (a==1) {
						if (flipbone==NULL)
							break;
						else {
							SWAP(EditBone *, flipbone, ebone);
						}
					}
					
					totbone++;
					newbone = MEM_callocN(sizeof(EditBone), "extrudebone");
					
					if (do_extrude==1) {
						VECCOPY (newbone->head, ebone->tail);
						VECCOPY (newbone->tail, newbone->head);
						newbone->parent = ebone;
						
						newbone->flag = ebone->flag & BONE_TIPSEL;	// copies it, in case mirrored bone
						
						if (newbone->parent) newbone->flag |= BONE_CONNECTED;
					}
					else {
						VECCOPY(newbone->head, ebone->head);
						VECCOPY(newbone->tail, ebone->head);
						newbone->parent= ebone->parent;
						
						newbone->flag= BONE_TIPSEL;
						
						if (newbone->parent && (ebone->flag & BONE_CONNECTED)) {
							newbone->flag |= BONE_CONNECTED;
						}
					}
					
					newbone->weight= ebone->weight;
					newbone->dist= ebone->dist;
					newbone->xwidth= ebone->xwidth;
					newbone->zwidth= ebone->zwidth;
					newbone->ease1= ebone->ease1;
					newbone->ease2= ebone->ease2;
					newbone->rad_head= ebone->rad_tail;	// dont copy entire bone...
					newbone->rad_tail= ebone->rad_tail;
					newbone->segments= 1;
					newbone->layer= ebone->layer;
					
					BLI_strncpy (newbone->name, ebone->name, 32);
					
					if (flipbone && forked) {	// only set if mirror edit
						if (strlen(newbone->name)<30) {
							if (a==0) strcat(newbone->name, "_L");
							else strcat(newbone->name, "_R");
						}
					}
					unique_editbone_name(arm->edbo, newbone->name, NULL);
					
					/* Add the new bone to the list */
					BLI_addtail(arm->edbo, newbone);
					if (!first)
						first = newbone;
					
					/* restore ebone if we were flipping */
					if (a==1 && flipbone) 
						SWAP(EditBone *, flipbone, ebone);
				}
			}
			
			/* Deselect the old bone */
			ebone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL);
		}		
	}
	/* if only one bone, make this one active */
	if (totbone==1 && first) arm->act_edbone= first;

	if (totbone==0) return OPERATOR_CANCELLED;
	
	if(arm->act_edbone && (((EditBone *)arm->act_edbone)->flag & BONE_SELECTED)==0)
		arm->act_edbone= NULL;

	/* Transform the endpoints */
	ED_armature_sync_selection(arm->edbo);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_extrude(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Extrude";
	ot->idname= "ARMATURE_OT_extrude";
	
	/* api callbacks */
	ot->exec= armature_extrude_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "forked", 0, "Forked", "");
}
/* ********************** Bone Add ********************/

/*op makes a new bone and returns it with its tip selected */

static int armature_bone_primitive_add_exec(bContext *C, wmOperator *op) 
{
	RegionView3D *rv3d= CTX_wm_region_view3d(C);
	Object *obedit = CTX_data_edit_object(C);
	EditBone *bone;
	float obmat[3][3], curs[3], viewmat[3][3], totmat[3][3], imat[3][3];
	char name[32];
	
	RNA_string_get(op->ptr, "name", name);
	
	VECCOPY(curs, give_cursor(CTX_data_scene(C),CTX_wm_view3d(C)));	

	/* Get inverse point for head and orientation for tail */
	invert_m4_m4(obedit->imat, obedit->obmat);
	mul_m4_v3(obedit->imat, curs);

	if (rv3d && (U.flag & USER_ADD_VIEWALIGNED))
		copy_m3_m4(obmat, rv3d->viewmat);
	else unit_m3(obmat);
	
	copy_m3_m4(viewmat, obedit->obmat);
	mul_m3_m3m3(totmat, obmat, viewmat);
	invert_m3_m3(imat, totmat);
	
	ED_armature_deselectall(obedit, 0, 0);
	
	/*	Create a bone	*/
	bone= ED_armature_edit_bone_add(obedit->data, name);

	VECCOPY(bone->head, curs);
	
	if(rv3d && (U.flag & USER_ADD_VIEWALIGNED))
		add_v3_v3v3(bone->tail, bone->head, imat[1]);	// bone with unit length 1
	else
		add_v3_v3v3(bone->tail, bone->head, imat[2]);	// bone with unit length 1, pointing up Z

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, obedit);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_bone_primitive_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Bone";
	ot->idname= "ARMATURE_OT_bone_primitive_add";
	
	/* api callbacks */
	ot->exec = armature_bone_primitive_add_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_string(ot->srna, "name", "Bone", 32, "Name", "Name of the newly created bone");
	
}


/* ----------- */

/* Subdivide Operators:
 * This group of operators all use the same 'exec' callback, but they are called
 * through several different operators - a combined menu (which just calls the exec in the 
 * appropriate ways), and two separate ones.
 */

static int armature_subdivide_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	bArmature *arm= obedit->data;
	EditBone *newbone, *tbone;
	int numcuts, i;
	
	/* there may not be a number_cuts property defined (for 'simple' subdivide) */
	if (RNA_property_is_set(op->ptr, "number_cuts"))
		numcuts= RNA_int_get(op->ptr, "number_cuts");
	else
		numcuts= 1;
	
	/* loop over all editable bones */
	// XXX the old code did this in reverse order though!
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) 
	{
		for (i=numcuts+1; i>1; i--) {
			/* compute cut ratio first */
			float cutratio= 1.0f / (float)i;
			float cutratioI= 1.0f - cutratio;
			
			float val1[3];
			float val2[3];
			float val3[3];
			
			newbone= MEM_mallocN(sizeof(EditBone), "ebone subdiv");
			*newbone = *ebone;
			BLI_addtail(arm->edbo, newbone);
			
			/* calculate location of newbone->head */
			VECCOPY(val1, ebone->head);
			VECCOPY(val2, ebone->tail);
			VECCOPY(val3, newbone->head);
			
			val3[0]= val1[0]*cutratio + val2[0]*cutratioI;
			val3[1]= val1[1]*cutratio + val2[1]*cutratioI;
			val3[2]= val1[2]*cutratio + val2[2]*cutratioI;
			
			VECCOPY(newbone->head, val3);
			VECCOPY(newbone->tail, ebone->tail);
			VECCOPY(ebone->tail, newbone->head);
			
			newbone->rad_head= 0.5f * (ebone->rad_head + ebone->rad_tail);
			ebone->rad_tail= newbone->rad_head;
			
			newbone->flag |= BONE_CONNECTED;
			
			unique_editbone_name(arm->edbo, newbone->name, NULL);
			
			/* correct parent bones */
			for (tbone = arm->edbo->first; tbone; tbone=tbone->next) {
				if (tbone->parent==ebone)
					tbone->parent= newbone;
			}
			newbone->parent= ebone;
		}
	}
	CTX_DATA_END;
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, obedit);
	
	return OPERATOR_FINISHED;
}


void ARMATURE_OT_subdivide_simple(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide Simple";
	ot->idname= "ARMATURE_OT_subdivide_simple";
	
	/* api callbacks */
	ot->exec = armature_subdivide_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void ARMATURE_OT_subdivide_multi(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Subdivide Multi";
	ot->idname= "ARMATURE_OT_subdivide_multi";
	
	/* api callbacks */
	ot->exec = armature_subdivide_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* Properties */
	RNA_def_int(ot->srna, "number_cuts", 2, 1, INT_MAX, "Number of Cuts", "", 1, 10);
}



static int armature_subdivs_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Subdivision Type", 0);
	layout= uiPupMenuLayout(pup);
	uiItemsEnumO(layout, "ARMATURE_OT_subdivs", "type");
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

static int armature_subdivs_exec(bContext *C, wmOperator *op)
{	
	switch (RNA_int_get(op->ptr, "type"))
	{
		case 0: /* simple */
			RNA_int_set(op->ptr, "number_cuts", 1);
			armature_subdivide_exec(C, op);
			break;
		case 1: /* multi */
			armature_subdivide_exec(C, op);
			break;
	}
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_subdivs(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[]= {
 		{0, "SIMPLE", 0, "Simple", ""},
		{1, "MULTI", 0, "Multi", ""},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "subdivs";
	ot->idname= "ARMATURE_OT_subdivs";
	
	/* api callbacks */
	ot->invoke= armature_subdivs_invoke;
	ot->exec= armature_subdivs_exec;
	
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "");
	
	/* this is temp, the ops are different, but they are called from subdivs, so all the possible props should be here as well*/
	RNA_def_int(ot->srna, "number_cuts", 2, 1, INT_MAX, "Number of Cuts", "", 1, 10); 
}

/* ----------- */

/* Switch Direction operator:
 * Currently, this does not use context loops, as context loops do not make it
 * easy to retrieve any hierarchial/chain relationships which are necessary for
 * this to be done easily.
 */

static int armature_switch_direction_exec(bContext *C, wmOperator *op) 
{
	Object *ob= CTX_data_edit_object(C);
	bArmature *arm= (bArmature *)ob->data;
	ListBase chains = {NULL, NULL};
	LinkData *chain;
	
	/* get chains of bones (ends on chains) */
	chains_find_tips(arm->edbo, &chains);
	if (chains.first == NULL) return OPERATOR_CANCELLED;
	
	/* loop over chains, only considering selected and visible bones */
	for (chain= chains.first; chain; chain= chain->next) {
		EditBone *ebo, *child=NULL, *parent=NULL;
		
		/* loop over bones in chain */
		for (ebo= chain->data; ebo; ebo= parent) {
			/* parent is this bone's original parent
			 *	- we store this, as the next bone that is checked is this one
			 *	  but the value of ebo->parent may change here...
			 */
			parent= ebo->parent;
			
			/* only if selected and editable */
			if (EBONE_VISIBLE(arm, ebo) && EBONE_EDITABLE(ebo)) {				
				/* swap head and tail coordinates */
				SWAP(float, ebo->head[0], ebo->tail[0]);
				SWAP(float, ebo->head[1], ebo->tail[1]);
				SWAP(float, ebo->head[2], ebo->tail[2]);
				
				/* do parent swapping:
				 *	- use 'child' as new parent
				 *	- connected flag is only set if points are coincidental
				 */
				ebo->parent= child;
				if ((child) && equals_v3v3(ebo->head, child->tail))
					ebo->flag |= BONE_CONNECTED;
				else	
					ebo->flag &= ~BONE_CONNECTED;
				
				/* get next bones 
				 *	- child will become the new parent of next bone
				 */
				child= ebo;
			}
			else {
				/* not swapping this bone, however, if its 'parent' got swapped, unparent us from it 
				 * as it will be facing in opposite direction
				 */
				if ((parent) && (EBONE_VISIBLE(arm, parent) && EBONE_EDITABLE(parent))) {
					ebo->parent= NULL;
					ebo->flag &= ~BONE_CONNECTED;
				}
				
				/* get next bones
				 *	- child will become new parent of next bone (not swapping occurred, 
				 *	  so set to NULL to prevent infinite-loop)
				 */
				child= NULL;
			}
		}
	}
	
	/* free chains */
	BLI_freelistN(&chains);	

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_switch_direction(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Switch Direction";
	ot->idname= "ARMATURE_OT_switch_direction";
	
	/* api callbacks */
	ot->exec = armature_switch_direction_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}
/* ***************** Parenting *********************** */

/* armature parenting options */
#define ARM_PAR_CONNECT 1
#define ARM_PAR_OFFSET	2

/* check for null, before calling! */
static void bone_connect_to_existing_parent(EditBone *bone)
{
	bone->flag |= BONE_CONNECTED;
	VECCOPY(bone->head, bone->parent->tail);
	bone->rad_head = bone->parent->rad_tail;
}

static void bone_connect_to_new_parent(ListBase *edbo, EditBone *selbone, EditBone *actbone, short mode)
{
	EditBone *ebone;
	float offset[3];
	
	if ((selbone->parent) && (selbone->flag & BONE_CONNECTED))
		selbone->parent->flag &= ~(BONE_TIPSEL);
	
	/* make actbone the parent of selbone */
	selbone->parent= actbone;
	
	/* in actbone tree we cannot have a loop */
	for (ebone= actbone->parent; ebone; ebone= ebone->parent) {
		if (ebone->parent==selbone) {
			ebone->parent= NULL;
			ebone->flag &= ~BONE_CONNECTED;
		}
	}
	
	if (mode == ARM_PAR_CONNECT) {	
		/* Connected: Child bones will be moved to the parent tip */
		selbone->flag |= BONE_CONNECTED;
		sub_v3_v3v3(offset, actbone->tail, selbone->head);
		
		VECCOPY(selbone->head, actbone->tail);
		selbone->rad_head= actbone->rad_tail;
		
		add_v3_v3v3(selbone->tail, selbone->tail, offset);
		
		/* offset for all its children */
		for (ebone = edbo->first; ebone; ebone=ebone->next) {
			EditBone *par;
			
			for (par= ebone->parent; par; par= par->parent) {
				if (par==selbone) {
					add_v3_v3v3(ebone->head, ebone->head, offset);
					add_v3_v3v3(ebone->tail, ebone->tail, offset);
					break;
				}
			}
		}
	}
	else {
		/* Offset: Child bones will retain their distance from the parent tip */
		selbone->flag &= ~BONE_CONNECTED;
	}
}

static EnumPropertyItem prop_editarm_make_parent_types[] = {
	{ARM_PAR_CONNECT, "CONNECTED", 0, "Connected", ""},
	{ARM_PAR_OFFSET, "OFFSET", 0, "Keep Offset", ""},
	{0, NULL, 0, NULL, NULL}
};

static int armature_parent_set_exec(bContext *C, wmOperator *op) 
{
	Object *ob= CTX_data_edit_object(C);
	bArmature *arm= (bArmature *)ob->data;
	EditBone *actbone = CTX_data_active_bone(C);
	EditBone *actmirb = NULL;
	short val = RNA_enum_get(op->ptr, "type");
	
	/* there must be an active bone */
	if (actbone == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Operation requires an Active Bone");
		return OPERATOR_CANCELLED;
	}
	else if (arm->flag & ARM_MIRROR_EDIT) {
		/* For X-Axis Mirror Editing option, we may need a mirror copy of actbone
		 * - if there's a mirrored copy of selbone, try to find a mirrored copy of actbone 
		 *	(i.e.  selbone="child.L" and actbone="parent.L", find "child.R" and "parent.R").
		 *	This is useful for arm-chains, for example parenting lower arm to upper arm
		 * - if there's no mirrored copy of actbone (i.e. actbone = "parent.C" or "parent")
		 *	then just use actbone. Useful when doing upper arm to spine.
		 */
		actmirb= ED_armature_bone_get_mirrored(arm->edbo, actbone);
		if (actmirb == NULL) 
			actmirb= actbone;
	}
	
	/* if there is only 1 selected bone, we assume that that is the active bone, 
	 * since a user will need to have clicked on a bone (thus selecting it) to make it active
	 */
	if (CTX_DATA_COUNT(C, selected_editable_bones) <= 1) {
		/* When only the active bone is selected, and it has a parent,
		 * connect it to the parent, as that is the only possible outcome. 
		 */
		if (actbone->parent) {
			bone_connect_to_existing_parent(actbone);
			
			if ((arm->flag & ARM_MIRROR_EDIT) && (actmirb->parent))
				bone_connect_to_existing_parent(actmirb);
		}
	}
	else {
		/* Parent 'selected' bones to the active one
		 * - the context iterator contains both selected bones and their mirrored copies,
		 *   so we assume that unselected bones are mirrored copies of some selected bone
		 * - since the active one (and/or its mirror) will also be selected, we also need 
		 * 	to check that we are not trying to opearate on them, since such an operation 
		 *	would cause errors
		 */
		
		/* parent selected bones to the active one */
		CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) {
			if (ELEM(ebone, actbone, actmirb) == 0) {
				if (ebone->flag & BONE_SELECTED) 
					bone_connect_to_new_parent(arm->edbo, ebone, actbone, val);
				else
					bone_connect_to_new_parent(arm->edbo, ebone, actmirb, val);
			}
		}
		CTX_DATA_END;
	}
	

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

static int armature_parent_set_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	EditBone *actbone = CTX_data_active_bone(C);
	uiPopupMenu *pup= uiPupMenuBegin(C, "Make Parent ", 0);
	uiLayout *layout= uiPupMenuLayout(pup);
	int allchildbones = 0;
	
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) {
		if (ebone != actbone) {
			if (ebone->parent != actbone) allchildbones= 1; 
		}	
	}
	CTX_DATA_END;

	uiItemEnumO(layout, NULL, 0, "ARMATURE_OT_parent_set", "type", ARM_PAR_CONNECT);
	
	/* ob becomes parent, make the associated menus */
	if (allchildbones)
		uiItemEnumO(layout, NULL, 0, "ARMATURE_OT_parent_set", "type", ARM_PAR_OFFSET);	
		
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

void ARMATURE_OT_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Make Parent";
	ot->idname= "ARMATURE_OT_parent_set";
	
	/* api callbacks */
	ot->invoke = armature_parent_set_invoke;
	ot->exec = armature_parent_set_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_editarm_make_parent_types, 0, "ParentType", "Type of parenting");
}

static EnumPropertyItem prop_editarm_clear_parent_types[] = {
	{1, "CLEAR", 0, "Clear Parent", ""},
	{2, "DISCONNECT", 0, "Disconnect Bone", ""},
	{0, NULL, 0, NULL, NULL}
};

static void editbone_clear_parent(EditBone *ebone, int mode)
{
	if (ebone->parent) {
		/* for nice selection */
		ebone->parent->flag &= ~(BONE_TIPSEL);
	}
	
	if (mode==1) ebone->parent= NULL;
	ebone->flag &= ~BONE_CONNECTED;
}

static int armature_parent_clear_exec(bContext *C, wmOperator *op) 
{
	Object *ob= CTX_data_edit_object(C);
	bArmature *arm= (bArmature *)ob->data;
	int val = RNA_enum_get(op->ptr, "type");
		
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) {
		editbone_clear_parent(ebone, val);
	}
	CTX_DATA_END;
	
	ED_armature_sync_selection(arm->edbo);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Parent";
	ot->idname= "ARMATURE_OT_parent_clear";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = armature_parent_clear_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	ot->prop= RNA_def_enum(ot->srna, "type", prop_editarm_clear_parent_types, 0, "ClearType", "What way to clear parenting");
}

/* ****************  Selections  ******************/

static int armature_select_inverse_exec(bContext *C, wmOperator *op)
{
	/*	Set the flags */
	CTX_DATA_BEGIN(C, EditBone *, ebone, visible_bones) {
		/* ignore bone if selection can't change */
		if ((ebone->flag & BONE_UNSELECTABLE) == 0) {
			/* select bone */
			ebone->flag ^= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
		}
	}
	CTX_DATA_END;	

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_inverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Select Inverse";
	ot->idname= "ARMATURE_OT_select_inverse";
	
	/* api callbacks */
	ot->exec= armature_select_inverse_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}
static int armature_de_select_all_exec(bContext *C, wmOperator *op)
{
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		/*	Determine if there are any selected bones
		And therefore whether we are selecting or deselecting */
		if (CTX_DATA_COUNT(C, selected_bones) > 0)
			action = SEL_DESELECT;
	}
	
	/*	Set the flags */
	CTX_DATA_BEGIN(C, EditBone *, ebone, visible_bones) {
		/* ignore bone if selection can't change */
		if ((ebone->flag & BONE_UNSELECTABLE) == 0) {
			switch (action) {
			case SEL_SELECT:
				ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				if(ebone->parent)
					ebone->parent->flag |= (BONE_TIPSEL);
				break;
			case SEL_DESELECT:
				ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				break;
			case SEL_INVERT:
				if (ebone->flag & BONE_SELECTED) {
					ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				} else {
					ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					if(ebone->parent)
						ebone->parent->flag |= (BONE_TIPSEL);
				}
				break;
			}
		}
	}
	CTX_DATA_END;	

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_all(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "deselect all editbone";
	ot->idname= "ARMATURE_OT_select_all";
	
	/* api callbacks */
	ot->exec= armature_de_select_all_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_select_all(ot);
}

/* ********************* select hierarchy operator ************** */

static int armature_select_hierarchy_exec(bContext *C, wmOperator *op)
{
	Object *obedit= CTX_data_edit_object(C);
	Object *ob;
	bArmature *arm;
	EditBone *curbone, *pabone, *chbone;
	int direction = RNA_enum_get(op->ptr, "direction");
	int add_to_sel = RNA_boolean_get(op->ptr, "extend");
	
	ob= obedit;
	arm= (bArmature *)ob->data;
	
	for (curbone= arm->edbo->first; curbone; curbone= curbone->next) {
		/* only work on bone if it is visible and its selection can change */
		if (EBONE_VISIBLE(arm, curbone) && (curbone->flag & BONE_UNSELECTABLE)==0) {
			if (curbone == arm->act_edbone) {
				if (direction == BONE_SELECT_PARENT) {
					if (curbone->parent == NULL) continue;
					else pabone = curbone->parent;
					
					if (EBONE_VISIBLE(arm, pabone)) {
						pabone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
						arm->act_edbone= pabone;
						if (pabone->parent)	pabone->parent->flag |= BONE_TIPSEL;
						
						if (!add_to_sel) curbone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
						break;
					}
					
				} 
				else { // BONE_SELECT_CHILD
					chbone = editbone_get_child(arm, curbone, 1);
					if (chbone == NULL) continue;
					
					if (EBONE_VISIBLE(arm, chbone) && (chbone->flag & BONE_UNSELECTABLE)==0) {
						chbone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
						arm->act_edbone= chbone;
						
						if (!add_to_sel) {
							curbone->flag &= ~(BONE_SELECTED|BONE_ROOTSEL);
							if (curbone->parent) curbone->parent->flag &= ~BONE_TIPSEL;
						}
						break;
					}
				}
			}
		}
	}
	
	ED_armature_sync_selection(arm->edbo);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_hierarchy(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[]= {
	{BONE_SELECT_PARENT, "PARENT", 0, "Select Parent", ""},
	{BONE_SELECT_CHILD, "CHILD", 0, "Select Child", ""},
	{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name= "Select Hierarchy";
	ot->idname= "ARMATURE_OT_select_hierarchy";
	
	/* api callbacks */
	ot->exec= armature_select_hierarchy_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items,
		     BONE_SELECT_PARENT, "Direction", "");
	RNA_def_boolean(ot->srna, "extend", 0, "Add to Selection", "");
}

/* ***************** EditBone Alignment ********************* */

/* helper to fix a ebone position if its parent has moved due to alignment*/
static void fix_connected_bone(EditBone *ebone)
{
	float diff[3];
	
	if (!(ebone->parent) || !(ebone->flag & BONE_CONNECTED) || equals_v3v3(ebone->parent->tail, ebone->head))
		return;
	
	/* if the parent has moved we translate child's head and tail accordingly*/
	sub_v3_v3v3(diff, ebone->parent->tail, ebone->head);
	add_v3_v3v3(ebone->head, ebone->head, diff);
	add_v3_v3v3(ebone->tail, ebone->tail, diff);
	return;
}

/* helper to recursively find chains of connected bones starting at ebone and fix their position */
static void fix_editbone_connected_children(ListBase *edbo, EditBone *ebone)
{
	EditBone *selbone;
	
	for (selbone = edbo->first; selbone; selbone=selbone->next) {
		if ((selbone->parent) && (selbone->parent == ebone) && (selbone->flag & BONE_CONNECTED)) {
			fix_connected_bone(selbone);
			fix_editbone_connected_children(edbo, selbone);
		}
	}
	return;
}			

static void bone_align_to_bone(ListBase *edbo, EditBone *selbone, EditBone *actbone)
{
	float selboneaxis[3], actboneaxis[3], length;

	sub_v3_v3v3(actboneaxis, actbone->tail, actbone->head);
	normalize_v3(actboneaxis);

	sub_v3_v3v3(selboneaxis, selbone->tail, selbone->head);
	length =  len_v3(selboneaxis);

	mul_v3_fl(actboneaxis, length);
	add_v3_v3v3(selbone->tail, selbone->head, actboneaxis);
	selbone->roll = actbone->roll;
	
	/* if the bone being aligned has connected descendants they must be moved
	according to their parent new position, otherwise they would be left
	in an unconsistent state: connected but away from the parent*/
	fix_editbone_connected_children(edbo, selbone);
	return;
}

static int armature_align_bones_exec(bContext *C, wmOperator *op) 
{
	Object *ob= CTX_data_edit_object(C);
	bArmature *arm= (bArmature *)ob->data;
	EditBone *actbone= CTX_data_active_bone(C);
	EditBone *actmirb= NULL;
	
	/* there must be an active bone */
	if (actbone == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Operation requires an Active Bone");
		return OPERATOR_CANCELLED;
	}
	else if (arm->flag & ARM_MIRROR_EDIT) {
		/* For X-Axis Mirror Editing option, we may need a mirror copy of actbone
		 * - if there's a mirrored copy of selbone, try to find a mirrored copy of actbone 
		 *	(i.e.  selbone="child.L" and actbone="parent.L", find "child.R" and "parent.R").
		 *	This is useful for arm-chains, for example parenting lower arm to upper arm
		 * - if there's no mirrored copy of actbone (i.e. actbone = "parent.C" or "parent")
		 *	then just use actbone. Useful when doing upper arm to spine.
		 */
		actmirb= ED_armature_bone_get_mirrored(arm->edbo, actbone);
		if (actmirb == NULL) 
			actmirb= actbone;
	}
	
	/* if there is only 1 selected bone, we assume that that is the active bone, 
	 * since a user will need to have clicked on a bone (thus selecting it) to make it active
	 */
	if (CTX_DATA_COUNT(C, selected_editable_bones) <= 1) {
		/* When only the active bone is selected, and it has a parent,
		 * align it to the parent, as that is the only possible outcome. 
		 */
		if (actbone->parent) {
			bone_align_to_bone(arm->edbo, actbone, actbone->parent);
			
			if ((arm->flag & ARM_MIRROR_EDIT) && (actmirb->parent))
				bone_align_to_bone(arm->edbo, actmirb, actmirb->parent);
		}
	}
	else {
		/* Align 'selected' bones to the active one
		 * - the context iterator contains both selected bones and their mirrored copies,
		 *   so we assume that unselected bones are mirrored copies of some selected bone
		 * - since the active one (and/or its mirror) will also be selected, we also need 
		 * 	to check that we are not trying to opearate on them, since such an operation 
		 *	would cause errors
		 */
		
		/* align selected bones to the active one */
		CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones) {
			if (ELEM(ebone, actbone, actmirb) == 0) {
				if (ebone->flag & BONE_SELECTED)
					bone_align_to_bone(arm->edbo, ebone, actbone);
				else
					bone_align_to_bone(arm->edbo, ebone, actmirb);
			}
		}
		CTX_DATA_END;
	}
	

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_align(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Align Bones";
	ot->idname= "ARMATURE_OT_align";
	ot->description= "Align selected bones to the active bone (or to their parent).";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = armature_align_bones_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ***************** Pose tools ********************* */

// XXX bone_looper is only to be used when we want to access settings (i.e. editability/visibility/selected) that context doesn't offer 
static int bone_looper(Object *ob, Bone *bone, void *data,
				int (*bone_func)(Object *, Bone *, void *)) 
{
    /* We want to apply the function bone_func to every bone 
	* in an armature -- feed bone_looper the first bone and 
	* a pointer to the bone_func and watch it go!. The int count 
	* can be useful for counting bones with a certain property
	* (e.g. skinnable)
	*/
    int count = 0;
	
    if (bone) {
		/* only do bone_func if the bone is non null */
        count += bone_func(ob, bone, data);
		
		/* try to execute bone_func for the first child */
        count += bone_looper(ob, bone->childbase.first, data, bone_func);
		
		/* try to execute bone_func for the next bone at this
			* depth of the recursion.
			*/
        count += bone_looper(ob, bone->next, data, bone_func);
    }
	
    return count;
}

/* called from editview.c, for mode-less pose selection */
/* assumes scene obact and basact is still on old situation */
int ED_do_pose_selectbuffer(Scene *scene, Base *base, unsigned int *buffer, short hits, short extend)
{
	Object *ob= base->object;
	Bone *nearBone;
	
	if (!ob || !ob->pose) return 0;

	nearBone= get_bone_from_selectbuffer(scene, base, buffer, hits, 1);
	
	/* if the bone cannot be affected, don't do anything */
	if ((nearBone) && !(nearBone->flag & BONE_UNSELECTABLE)) {
		bArmature *arm= ob->data;
		
		/* since we do unified select, we don't shift+select a bone if the armature object was not active yet */
		if (!(extend) || (base != scene->basact)) {
			ED_pose_deselectall(ob, 0, 0);
			nearBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
			arm->act_bone= nearBone;
			
				// XXX old cruft! use notifiers instead
			//select_actionchannel_by_name(ob->action, nearBone->name, 1);
		}
		else {
			if (nearBone->flag & BONE_SELECTED) {
				/* if not active, we make it active */
				if(nearBone != arm->act_bone) {
					arm->act_bone= nearBone;
				}
				else {
					nearBone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
					
						// XXX old cruft! use notifiers instead
					//select_actionchannel_by_name(ob->action, nearBone->name, 0);
				}
			}
			else {
				nearBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
				arm->act_bone= nearBone;
				
					// XXX old cruft! use notifiers instead
				//select_actionchannel_by_name(ob->action, nearBone->name, 1);
			}
		}
		
		/* in weightpaint we select the associated vertex group too */
		if (OBACT && OBACT->mode & OB_MODE_WEIGHT_PAINT) {
			if (nearBone == arm->act_bone) {
				ED_vgroup_select_by_name(OBACT, nearBone->name);
				DAG_id_flush_update(&OBACT->id, OB_RECALC_DATA);
			}
		}
		
	}
	
	return nearBone!=NULL;
}

/* test==0: deselect all
   test==1: swap select (apply to all the opposite of current situation) 
   test==2: only clear active tag
   test==3: swap select (no test / inverse selection status of all independently)
*/
void ED_pose_deselectall (Object *ob, int test, int doundo)
{
	bArmature *arm= ob->data;
	bPoseChannel *pchan;
	int	selectmode= 0;
	
	/* we call this from outliner too */
	if (ELEM(NULL, ob, ob->pose)) return;
	
	/*	Determine if we're selecting or deselecting	*/
	if (test==1) {
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ((pchan->bone->layer & arm->layer) && !(pchan->bone->flag & BONE_HIDDEN_P)) {
				if (pchan->bone->flag & BONE_SELECTED)
					break;
			}
		}
		
		if (pchan == NULL)
			selectmode= 1;
	}
	else if (test == 2)
		selectmode= 2;
	
	/*	Set the flags accordingly	*/
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		/* ignore the pchan if it isn't visible or if its selection cannot be changed */
		if ((pchan->bone->layer & arm->layer) && !(pchan->bone->flag & (BONE_HIDDEN_P|BONE_UNSELECTABLE))) {
			if (test==3) {
				pchan->bone->flag ^= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
			}
			else {
				if (selectmode==0) pchan->bone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
				else if (selectmode==1) pchan->bone->flag |= BONE_SELECTED;
			}
		}
	}
	
	if(arm->act_bone && (arm->act_bone->flag & BONE_SELECTED)==0)
		arm->act_bone= NULL;

	//countall(); // XXX need an equivalent to this...
	
	if (doundo) {
		if (selectmode==1) BIF_undo_push("Select All");
		else BIF_undo_push("Deselect All");
	}
}

static int bone_skinnable(Object *ob, Bone *bone, void *datap)
{
    /* Bones that are deforming
     * are regarded to be "skinnable" and are eligible for
     * auto-skinning.
     *
     * This function performs 2 functions:
     *
     *   a) It returns 1 if the bone is skinnable.
     *      If we loop over all bones with this 
     *      function, we can count the number of
     *      skinnable bones.
     *   b) If the pointer data is non null,
     *      it is treated like a handle to a
     *      bone pointer -- the bone pointer
     *      is set to point at this bone, and
     *      the pointer the handle points to
     *      is incremented to point to the
     *      next member of an array of pointers
     *      to bones. This way we can loop using
     *      this function to construct an array of
     *      pointers to bones that point to all
     *      skinnable bones.
     */
	Bone ***hbone;
	int a, segments;
	struct { Object *armob; void *list; int heat; } *data = datap;

	if(!(ob->mode & OB_MODE_WEIGHT_PAINT) || !(bone->flag & BONE_HIDDEN_P)) {
		if (!(bone->flag & BONE_NO_DEFORM)) {
			if (data->heat && data->armob->pose && get_pose_channel(data->armob->pose, bone->name))
				segments = bone->segments;
			else
				segments = 1;
			
			if (data->list != NULL) {
				hbone = (Bone ***) &data->list;
				
				for (a=0; a<segments; a++) {
					**hbone = bone;
					++*hbone;
				}
			}
			return segments;
		}
	}
    return 0;
}

static int ED_vgroup_add_unique_bone(Object *ob, Bone *bone, void *data) 
{
    /* This group creates a vertex group to ob that has the
      * same name as bone (provided the bone is skinnable). 
	 * If such a vertex group aleady exist the routine exits.
      */
	if (!(bone->flag & BONE_NO_DEFORM)) {
		if (!get_named_vertexgroup(ob,bone->name)) {
			ED_vgroup_add_name(ob, bone->name);
			return 1;
		}
    }
    return 0;
}

static int dgroup_skinnable(Object *ob, Bone *bone, void *datap) 
{
    /* Bones that are deforming
     * are regarded to be "skinnable" and are eligible for
     * auto-skinning.
     *
     * This function performs 2 functions:
     *
     *   a) If the bone is skinnable, it creates 
     *      a vertex group for ob that has
     *      the name of the skinnable bone
     *      (if one doesn't exist already).
     *   b) If the pointer data is non null,
     *      it is treated like a handle to a
     *      bDeformGroup pointer -- the 
     *      bDeformGroup pointer is set to point
     *      to the deform group with the bone's
     *      name, and the pointer the handle 
     *      points to is incremented to point to the
     *      next member of an array of pointers
     *      to bDeformGroups. This way we can loop using
     *      this function to construct an array of
     *      pointers to bDeformGroups, all with names
     *      of skinnable bones.
     */
    bDeformGroup ***hgroup, *defgroup;
	int a, segments;
	struct { Object *armob; void *list; int heat; } *data= datap;

	if (!(ob->mode & OB_MODE_WEIGHT_PAINT) || !(bone->flag & BONE_HIDDEN_P)) {
	   if (!(bone->flag & BONE_NO_DEFORM)) {
			if (data->heat && data->armob->pose && get_pose_channel(data->armob->pose, bone->name))
				segments = bone->segments;
			else
				segments = 1;
			
			if (!(defgroup = get_named_vertexgroup(ob, bone->name)))
				defgroup = ED_vgroup_add_name(ob, bone->name);
			
			if (data->list != NULL) {
				hgroup = (bDeformGroup ***) &data->list;
				
				for (a=0; a<segments; a++) {
					**hgroup = defgroup;
					++*hgroup;
				}
			}
			return segments;
		}
	}
    return 0;
}

static void add_vgroups__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	/* DerivedMesh mapFunc for getting final coords in weight paint mode */

	float (*verts)[3] = userData;
	VECCOPY(verts[index], co);
}

static void envelope_bone_weighting(Object *ob, Mesh *mesh, float (*verts)[3], int numbones, Bone **bonelist, bDeformGroup **dgrouplist, bDeformGroup **dgroupflip, float (*root)[3], float (*tip)[3], int *selected, float scale)
{
	/* Create vertex group weights from envelopes */

	Bone *bone;
	bDeformGroup *dgroup;
	float distance;
	int i, iflip, j;

	/* for each vertex in the mesh */
	for (i=0; i < mesh->totvert; i++) {
		iflip = (dgroupflip)? mesh_get_x_mirror_vert(ob, i): 0;
		
		/* for each skinnable bone */
		for (j=0; j < numbones; ++j) {
			if (!selected[j])
				continue;
			
			bone = bonelist[j];
			dgroup = dgrouplist[j];
			
			/* store the distance-factor from the vertex to the bone */
			distance = distfactor_to_bone (verts[i], root[j], tip[j],
				bone->rad_head * scale, bone->rad_tail * scale, bone->dist * scale);
			
			/* add the vert to the deform group if weight!=0.0 */
			if (distance!=0.0)
				ED_vgroup_vert_add (ob, dgroup, i, distance, WEIGHT_REPLACE);
			else
				ED_vgroup_vert_remove (ob, dgroup, i);
			
			/* do same for mirror */
			if (dgroupflip && dgroupflip[j] && iflip >= 0) {
				if (distance!=0.0)
					ED_vgroup_vert_add (ob, dgroupflip[j], iflip, distance,
						WEIGHT_REPLACE);
				else
					ED_vgroup_vert_remove (ob, dgroupflip[j], iflip);
			}
		}
	}
}

void add_verts_to_dgroups(Scene *scene, Object *ob, Object *par, int heat, int mirror)
{
	/* This functions implements the automatic computation of vertex group
	 * weights, either through envelopes or using a heat equilibrium.
	 *
	 * This function can be called both when parenting a mesh to an armature,
	 * or in weightpaint + posemode. In the latter case selection is taken
	 * into account and vertex weights can be mirrored.
	 *
	 * The mesh vertex positions used are either the final deformed coords
	 * from the derivedmesh in weightpaint mode, the final subsurf coords
	 * when parenting, or simply the original mesh coords.
	 */

	bArmature *arm= par->data;
	Bone **bonelist, *bone;
	bDeformGroup **dgrouplist, **dgroupflip;
	bDeformGroup *dgroup, *curdg;
	bPoseChannel *pchan;
	Mesh *mesh;
	Mat4 *bbone = NULL;
	float (*root)[3], (*tip)[3], (*verts)[3];
	int *selected;
	int numbones, vertsfilled = 0, i, j, segments = 0;
	int wpmode = (ob->mode & OB_MODE_WEIGHT_PAINT);
	struct { Object *armob; void *list; int heat; } looper_data;

	looper_data.armob = par;
	looper_data.heat= heat;
	looper_data.list= NULL;

	/* count the number of skinnable bones */
	numbones = bone_looper(ob, arm->bonebase.first, &looper_data, bone_skinnable);
	
	if (numbones == 0)
		return;
	
	/* create an array of pointer to bones that are skinnable
	 * and fill it with all of the skinnable bones */
	bonelist = MEM_callocN(numbones*sizeof(Bone *), "bonelist");
	looper_data.list= bonelist;
	bone_looper(ob, arm->bonebase.first, &looper_data, bone_skinnable);

	/* create an array of pointers to the deform groups that
	 * coorespond to the skinnable bones (creating them
	 * as necessary. */
	dgrouplist = MEM_callocN(numbones*sizeof(bDeformGroup *), "dgrouplist");
	dgroupflip = MEM_callocN(numbones*sizeof(bDeformGroup *), "dgroupflip");

	looper_data.list= dgrouplist;
	bone_looper(ob, arm->bonebase.first, &looper_data, dgroup_skinnable);

	/* create an array of root and tip positions transformed into
	 * global coords */
	root = MEM_callocN(numbones*sizeof(float)*3, "root");
	tip = MEM_callocN(numbones*sizeof(float)*3, "tip");
	selected = MEM_callocN(numbones*sizeof(int), "selected");

	for (j=0; j < numbones; ++j) {
   		bone = bonelist[j];
		dgroup = dgrouplist[j];
		
		/* handle bbone */
		if (heat) {
			if (segments == 0) {
				segments = 1;
				bbone = NULL;
				
				if ((par->pose) && (pchan=get_pose_channel(par->pose, bone->name))) {
					if (bone->segments > 1) {
						segments = bone->segments;
						bbone = b_bone_spline_setup(pchan, 1);
					}
				}
			}
			
			segments--;
		}
		
		/* compute root and tip */
		if (bbone) {
			VECCOPY(root[j], bbone[segments].mat[3]);
			mul_m4_v3(bone->arm_mat, root[j]);
			if ((segments+1) < bone->segments) {
				VECCOPY(tip[j], bbone[segments+1].mat[3])
				mul_m4_v3(bone->arm_mat, tip[j]);
			}
			else
				VECCOPY(tip[j], bone->arm_tail)
		}
		else {
			VECCOPY(root[j], bone->arm_head);
			VECCOPY(tip[j], bone->arm_tail);
		}
		
		mul_m4_v3(par->obmat, root[j]);
		mul_m4_v3(par->obmat, tip[j]);
		
		/* set selected */
		if (wpmode) {
			if ((arm->layer & bone->layer) && (bone->flag & BONE_SELECTED))
				selected[j] = 1;
		}
		else
			selected[j] = 1;
		
		/* find flipped group */
		if (mirror) {
			char name[32];
			
			BLI_strncpy(name, dgroup->name, 32);
			// 0 = don't strip off number extensions
			bone_flip_name(name, 0);
			
			for (curdg = ob->defbase.first; curdg; curdg=curdg->next) {
				if (!strcmp(curdg->name, name))
					break;
			}
			
			dgroupflip[j] = curdg;
		}
	}

	/* create verts */
    mesh = (Mesh*)ob->data;
	verts = MEM_callocN(mesh->totvert*sizeof(*verts), "closestboneverts");

	if (wpmode) {
		/* if in weight paint mode, use final verts from derivedmesh */
		DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
		
		if (dm->foreachMappedVert) {
			dm->foreachMappedVert(dm, add_vgroups__mapFunc, (void*)verts);
			vertsfilled = 1;
		}
		
		dm->release(dm);
	}
	else if (modifiers_findByType(ob, eModifierType_Subsurf)) {
		/* is subsurf on? Lets use the verts on the limit surface then.
	 	 * = same amount of vertices as mesh, but vertices  moved to the
		 * subsurfed position, like for 'optimal'. */
		subsurf_calculate_limit_positions(mesh, verts);
		vertsfilled = 1;
	}

	/* transform verts to global space */
	for (i=0; i < mesh->totvert; i++) {
		if (!vertsfilled)
			VECCOPY(verts[i], mesh->mvert[i].co)
		mul_m4_v3(ob->obmat, verts[i]);
	}

	/* compute the weights based on gathered vertices and bones */
	if (heat) {
		heat_bone_weighting(ob, mesh, verts, numbones, dgrouplist, dgroupflip,
			root, tip, selected);
	}
	else {
		envelope_bone_weighting(ob, mesh, verts, numbones, bonelist, dgrouplist,
			dgroupflip, root, tip, selected, mat4_to_scale(par->obmat));
	}
	
    /* free the memory allocated */
    MEM_freeN(bonelist);
    MEM_freeN(dgrouplist);
	MEM_freeN(dgroupflip);
	MEM_freeN(root);
	MEM_freeN(tip);
	MEM_freeN(selected);
	MEM_freeN(verts);
}

void create_vgroups_from_armature(Scene *scene, Object *ob, Object *par, int mode)
{
	/* Lets try to create some vertex groups 
	 * based on the bones of the parent armature.
	 */
	bArmature *arm= par->data;

	if(mode == ARM_GROUPS_NAME) {
		/* Traverse the bone list, trying to create empty vertex 
		 * groups cooresponding to the bone.
		 */
		bone_looper(ob, arm->bonebase.first, NULL, ED_vgroup_add_unique_bone);

		if (ob->type == OB_MESH)
			ED_vgroup_data_create(ob->data);
	}
	else if(mode == ARM_GROUPS_ENVELOPE || mode == ARM_GROUPS_AUTO) {
		/* Traverse the bone list, trying to create vertex groups 
		 * that are populated with the vertices for which the
		 * bone is closest.
		 */
		add_verts_to_dgroups(scene, ob, par, (mode == ARM_GROUPS_AUTO), 0);
	}
} 
/* ************* Clear Pose *****************************/

static int pose_clear_scale_exec(bContext *C, wmOperator *op) 
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	
	KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Scaling");
	bCommonKeySrc cks;
	ListBase dsources = {&cks, &cks};
	
	/* init common-key-source for use by KeyingSets */
	memset(&cks, 0, sizeof(bCommonKeySrc));
	cks.id= &ob->id;
	
	/* only clear those channels that are not locked */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones) {
		if ((pchan->protectflag & OB_LOCK_SCALEX)==0)
			pchan->size[0]= 1.0f;
		if ((pchan->protectflag & OB_LOCK_SCALEY)==0)
			pchan->size[1]= 1.0f;
		if ((pchan->protectflag & OB_LOCK_SCALEZ)==0)
			pchan->size[2]= 1.0f;
			
		/* do auto-keyframing as appropriate */
		if (autokeyframe_cfra_can_key(scene, &ob->id)) {
			/* init cks for this PoseChannel, then use the relative KeyingSets to keyframe it */
			cks.pchan= pchan;
			modify_keyframes(scene, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
			
			/* clear any unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag &= ~BONE_UNKEYED;
		}
		else {
			/* add unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag |= BONE_UNKEYED;
		}
	}
	CTX_DATA_END;
	
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_scale_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Pose Scale";
	ot->idname= "POSE_OT_scale_clear";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = pose_clear_scale_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int pose_clear_loc_exec(bContext *C, wmOperator *op) 
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	
	KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Location");
	bCommonKeySrc cks;
	ListBase dsources = {&cks, &cks};
	
	/* init common-key-source for use by KeyingSets */
	memset(&cks, 0, sizeof(bCommonKeySrc));
	cks.id= &ob->id;
	
	/* only clear those channels that are not locked */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones) {
		/* clear location */
		if ((pchan->protectflag & OB_LOCK_LOCX)==0)
			pchan->loc[0]= 0.0f;
		if ((pchan->protectflag & OB_LOCK_LOCY)==0)
			pchan->loc[1]= 0.0f;
		if ((pchan->protectflag & OB_LOCK_LOCZ)==0)
			pchan->loc[2]= 0.0f;
			
		/* do auto-keyframing as appropriate */
		if (autokeyframe_cfra_can_key(scene, &ob->id)) {
			/* init cks for this PoseChannel, then use the relative KeyingSets to keyframe it */
			cks.pchan= pchan;
			modify_keyframes(scene, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
			
			/* clear any unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag &= ~BONE_UNKEYED;
		}
		else {
			/* add unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag |= BONE_UNKEYED;
		}
	}
	CTX_DATA_END;
	
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_loc_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Pose Location";
	ot->idname= "POSE_OT_loc_clear";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = pose_clear_loc_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int pose_clear_rot_exec(bContext *C, wmOperator *op) 
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	
	KeyingSet *ks= ANIM_builtin_keyingset_get_named(NULL, "Rotation");
	bCommonKeySrc cks;
	ListBase dsources = {&cks, &cks};
	
	/* init common-key-source for use by KeyingSets */
	memset(&cks, 0, sizeof(bCommonKeySrc));
	cks.id= &ob->id;
	
	/* only clear those channels that are not locked */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones) {
		if (pchan->protectflag & (OB_LOCK_ROTX|OB_LOCK_ROTY|OB_LOCK_ROTZ|OB_LOCK_ROTW)) {
			/* check if convert to eulers for locking... */
			if (pchan->protectflag & OB_LOCK_ROT4D) {
				/* perform clamping on a component by component basis */
				if (pchan->rotmode == ROT_MODE_AXISANGLE) {
					if ((pchan->protectflag & OB_LOCK_ROTW) == 0)
						pchan->rotAngle= 0.0f;
					if ((pchan->protectflag & OB_LOCK_ROTX) == 0)
						pchan->rotAxis[0]= 0.0f;
					if ((pchan->protectflag & OB_LOCK_ROTY) == 0)
						pchan->rotAxis[1]= 0.0f;
					if ((pchan->protectflag & OB_LOCK_ROTZ) == 0)
						pchan->rotAxis[2]= 0.0f;
						
					/* check validity of axis - axis should never be 0,0,0 (if so, then we make it rotate about y) */
					if (IS_EQ(pchan->rotAxis[0], pchan->rotAxis[1]) && IS_EQ(pchan->rotAxis[1], pchan->rotAxis[2]))
						pchan->rotAxis[1] = 1.0f;
				}
				else if (pchan->rotmode == ROT_MODE_QUAT) {
					if ((pchan->protectflag & OB_LOCK_ROTW) == 0)
						pchan->quat[0]= 1.0f;
					if ((pchan->protectflag & OB_LOCK_ROTX) == 0)
						pchan->quat[1]= 0.0f;
					if ((pchan->protectflag & OB_LOCK_ROTY) == 0)
						pchan->quat[2]= 0.0f;
					if ((pchan->protectflag & OB_LOCK_ROTZ) == 0)
						pchan->quat[3]= 0.0f;
				}
				else {
					/* the flag may have been set for the other modes, so just ignore the extra flag... */
					if ((pchan->protectflag & OB_LOCK_ROTX) == 0)
						pchan->eul[0]= 0.0f;
					if ((pchan->protectflag & OB_LOCK_ROTY) == 0)
						pchan->eul[1]= 0.0f;
					if ((pchan->protectflag & OB_LOCK_ROTZ) == 0)
						pchan->eul[2]= 0.0f;
				}
			}
			else {
				/* perform clamping using euler form (3-components) */
				float eul[3], oldeul[3], quat1[4];
				
				if (pchan->rotmode == ROT_MODE_QUAT) {
					QUATCOPY(quat1, pchan->quat);
					quat_to_eul( oldeul,pchan->quat);
				}
				else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
					axis_angle_to_eulO( oldeul, EULER_ORDER_DEFAULT,pchan->rotAxis, pchan->rotAngle);
				}
				else {
					VECCOPY(oldeul, pchan->eul);
				}
				
				eul[0]= eul[1]= eul[2]= 0.0f;
				
				if (pchan->protectflag & OB_LOCK_ROTX)
					eul[0]= oldeul[0];
				if (pchan->protectflag & OB_LOCK_ROTY)
					eul[1]= oldeul[1];
				if (pchan->protectflag & OB_LOCK_ROTZ)
					eul[2]= oldeul[2];
				
				if (pchan->rotmode == ROT_MODE_QUAT) {
					eul_to_quat( pchan->quat,eul);
					/* quaternions flip w sign to accumulate rotations correctly */
					if ((quat1[0]<0.0f && pchan->quat[0]>0.0f) || (quat1[0]>0.0f && pchan->quat[0]<0.0f)) {
						mul_qt_fl(pchan->quat, -1.0f);
					}
				}
				else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
					eulO_to_axis_angle( pchan->rotAxis, &pchan->rotAngle,eul, EULER_ORDER_DEFAULT);
				}
				else {
					VECCOPY(pchan->eul, eul);
				}
			}
		}						
		else { 
			if (pchan->rotmode == ROT_MODE_QUAT) {
				pchan->quat[1]=pchan->quat[2]=pchan->quat[3]= 0.0f; 
				pchan->quat[0]= 1.0f;
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				/* by default, make rotation of 0 radians around y-axis (roll) */
				pchan->rotAxis[0]=pchan->rotAxis[2]=pchan->rotAngle= 0.0f;
				pchan->rotAxis[1]= 1.0f;
			}
			else {
				pchan->eul[0]= pchan->eul[1]= pchan->eul[2]= 0.0f;
			}
		}
		
		/* do auto-keyframing as appropriate */
		if (autokeyframe_cfra_can_key(scene, &ob->id)) {
			/* init cks for this PoseChannel, then use the relative KeyingSets to keyframe it */
			cks.pchan= pchan;
			modify_keyframes(scene, &dsources, NULL, ks, MODIFYKEY_MODE_INSERT, (float)CFRA);
			
			/* clear any unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag &= ~BONE_UNKEYED;
		}
		else {
			/* add unkeyed tags */
			if (pchan->bone)
				pchan->bone->flag |= BONE_UNKEYED;
		}
	}
	CTX_DATA_END;
	
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_rot_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Pose Rotation";
	ot->idname= "POSE_OT_rot_clear";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = pose_clear_rot_exec;
	ot->poll = ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

}

/* ***************** selections ********************** */

static int pose_select_inverse_exec(bContext *C, wmOperator *op)
{
	
	/*	Set the flags */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) {
		if ((pchan->bone->flag & BONE_UNSELECTABLE) == 0) {
			pchan->bone->flag ^= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
		}
	}	
	CTX_DATA_END;
	
	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_select_inverse(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Select Inverse";
	ot->idname= "POSE_OT_select_inverse";
	
	/* api callbacks */
	ot->exec= pose_select_inverse_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}
static int pose_de_select_all_exec(bContext *C, wmOperator *op)
{
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		/* Determine if there are any selected bones and therefore whether we are selecting or deselecting */
		// NOTE: we have to check for > 1 not > 0, since there is almost always an active bone that can't be cleared...
		if (CTX_DATA_COUNT(C, selected_pose_bones) > 1)
			action = SEL_DESELECT;
	}
	
	/*	Set the flags */
	CTX_DATA_BEGIN(C, bPoseChannel *, pchan, visible_pose_bones) {
		/* select pchan only if selectable, but deselect works always */
		switch (action) {
		case SEL_SELECT:
			if ((pchan->bone->flag & BONE_UNSELECTABLE)==0)
				pchan->bone->flag |= BONE_SELECTED;
			break;
		case SEL_DESELECT:
			pchan->bone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
			break;
		case SEL_INVERT:
			if (pchan->bone->flag & BONE_SELECTED) {
				pchan->bone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
			} else if ((pchan->bone->flag & BONE_UNSELECTABLE)==0) {
					pchan->bone->flag |= BONE_SELECTED;
			}
			break;
		}
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_select_all(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "deselect all bones";
	ot->idname= "POSE_OT_select_all";
	
	/* api callbacks */
	ot->exec= pose_de_select_all_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	WM_operator_properties_select_all(ot);
}

static int pose_select_parent_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	bPoseChannel *pchan,*parent;

	/*	Determine if there is an active bone */
	pchan=CTX_data_active_pose_bone(C);
	if (pchan) {
		bArmature *arm= ob->data;
		parent=pchan->parent;
		if ((parent) && !(parent->bone->flag & (BONE_HIDDEN_P|BONE_UNSELECTABLE))) {
			parent->bone->flag |= BONE_SELECTED;
			arm->act_bone= parent->bone;
		}
		else {
			return OPERATOR_CANCELLED;
		}
	}
	else {
		return OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_select_parent(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "select parent bone";
	ot->idname= "POSE_OT_select_parent";

	/* api callbacks */
	ot->exec= pose_select_parent_exec;
	ot->poll= ED_operator_posemode;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}

/* ************* hide/unhide pose bones ******************* */

static int hide_selected_pose_bone(Object *ob, Bone *bone, void *ptr) 
{
	bArmature *arm= ob->data;
	
	if (arm->layer & bone->layer) {
		if (bone->flag & BONE_SELECTED) {
			bone->flag |= BONE_HIDDEN_P;
			if(arm->act_bone==bone)
				arm->act_bone= NULL;
		}
	}
	return 0;
}

static int hide_unselected_pose_bone(Object *ob, Bone *bone, void *ptr) 
{
	bArmature *arm= ob->data;
	
	if (arm->layer & bone->layer) {
		// hrm... typo here?
		if ((bone->flag & BONE_SELECTED)==0) {
			bone->flag |= BONE_HIDDEN_P;
			if(arm->act_bone==bone)
				arm->act_bone= NULL;
		}
	}
	return 0;
}

/* active object is armature in posemode, poll checked */
static int pose_hide_exec(bContext *C, wmOperator *op) 
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm= ob->data;

	if(RNA_boolean_get(op->ptr, "unselected"))
	   bone_looper(ob, arm->bonebase.first, NULL, 
				hide_unselected_pose_bone);
	else
	   bone_looper(ob, arm->bonebase.first, NULL, 
				   hide_selected_pose_bone);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide Selected";
	ot->idname= "POSE_OT_hide";
	
	/* api callbacks */
	ot->exec= pose_hide_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "");
}

static int show_pose_bone(Object *ob, Bone *bone, void *ptr) 
{
	bArmature *arm= ob->data;
	
	if (arm->layer & bone->layer) {
		if (bone->flag & BONE_HIDDEN_P) {
			bone->flag &= ~BONE_HIDDEN_P;
			bone->flag |= BONE_SELECTED;
		}
	}
	
	return 0;
}

/* active object is armature in posemode, poll checked */
static int pose_reveal_exec(bContext *C, wmOperator *op) 
{
	Object *ob= CTX_data_active_object(C);
	bArmature *arm= ob->data;
	
	bone_looper(ob, arm->bonebase.first, NULL, show_pose_bone);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void POSE_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reveal Selected";
	ot->idname= "POSE_OT_reveal";
	
	/* api callbacks */
	ot->exec= pose_reveal_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************* RENAMING DISASTERS ************ */

/* note: there's a unique_editbone_name() too! */
void unique_bone_name (bArmature *arm, char *name)
{
	char		tempname[64];
	int			number;
	char		*dot;
	
	if (get_named_bone(arm, name)) {
		
		/*	Strip off the suffix, if it's a number */
		number= strlen(name);
		if(number && isdigit(name[number-1])) {
			dot= strrchr(name, '.');	// last occurrence
			if (dot)
				*dot=0;
		}
		
		for (number = 1; number <=999; number++) {
			sprintf (tempname, "%s.%03d", name, number);
			if (!get_named_bone(arm, tempname)) {
				BLI_strncpy (name, tempname, 32);
				return;
			}
		}
	}
}

#define MAXBONENAME 32
/* helper call for armature_bone_rename */
static void constraint_bone_name_fix(Object *ob, ListBase *conlist, char *oldname, char *newname)
{
	bConstraint *curcon;
	bConstraintTarget *ct;
	
	for (curcon = conlist->first; curcon; curcon=curcon->next) {
		bConstraintTypeInfo *cti= constraint_get_typeinfo(curcon);
		ListBase targets = {NULL, NULL};
		
		if (cti && cti->get_constraint_targets) {
			cti->get_constraint_targets(curcon, &targets);
			
			for (ct= targets.first; ct; ct= ct->next) {
				if (ct->tar == ob) {
					if (!strcmp(ct->subtarget, oldname) )
						BLI_strncpy(ct->subtarget, newname, MAXBONENAME);
				}
			}
			
			if (cti->flush_constraint_targets)
				cti->flush_constraint_targets(curcon, &targets, 0);
		}	
	}
}

/* called by UI for renaming a bone */
/* warning: make sure the original bone was not renamed yet! */
/* seems messy, but thats what you get with not using pointers but channel names :) */
void ED_armature_bone_rename(bArmature *arm, char *oldnamep, char *newnamep)
{
	Object *ob;
	char newname[MAXBONENAME];
	char oldname[MAXBONENAME];
	
	/* names better differ! */
	if(strncmp(oldnamep, newnamep, MAXBONENAME)) {
		
		/* we alter newname string... so make copy */
		BLI_strncpy(newname, newnamep, MAXBONENAME);
		/* we use oldname for search... so make copy */
		BLI_strncpy(oldname, oldnamep, MAXBONENAME);
		
		/* now check if we're in editmode, we need to find the unique name */
		if (arm->edbo) {
			EditBone *eBone= editbone_name_exists(arm->edbo, oldname);
			
			if (eBone) {
				unique_editbone_name(arm->edbo, newname, NULL);
				BLI_strncpy(eBone->name, newname, MAXBONENAME);
			}
			else return;
		}
		else {
			Bone *bone= get_named_bone(arm, oldname);
			
			if (bone) {
				unique_bone_name(arm, newname);
				BLI_strncpy(bone->name, newname, MAXBONENAME);
			}
			else return;
		}
		
		/* do entire dbase - objects */
		for (ob= G.main->object.first; ob; ob= ob->id.next) {
			/* we have the object using the armature */
			if (arm==ob->data) {
				Object *cob;
				
				/* Rename the pose channel, if it exists */
				if (ob->pose) {
					bPoseChannel *pchan = get_pose_channel(ob->pose, oldname);
					if (pchan)
						BLI_strncpy(pchan->name, newname, MAXBONENAME);
				}
				
				/* Update any object constraints to use the new bone name */
				for (cob= G.main->object.first; cob; cob= cob->id.next) {
					if (cob->constraints.first)
						constraint_bone_name_fix(ob, &cob->constraints, oldname, newname);
					if (cob->pose) {
						bPoseChannel *pchan;
						for (pchan = cob->pose->chanbase.first; pchan; pchan=pchan->next) {
							constraint_bone_name_fix(ob, &pchan->constraints, oldname, newname);
						}
					}
				}
			}
					
			/* See if an object is parented to this armature */
			if (ob->parent && (ob->parent->data == arm)) {
				if (ob->partype==PARBONE) {
					/* bone name in object */
					if (!strcmp(ob->parsubstr, oldname))
						BLI_strncpy(ob->parsubstr, newname, MAXBONENAME);
				}
			}
			
			if (modifiers_usesArmature(ob, arm)) { 
				bDeformGroup *dg;
				/* bone name in defgroup */
				for (dg=ob->defbase.first; dg; dg=dg->next) {
					if (!strcmp(dg->name, oldname))
					   BLI_strncpy(dg->name, newname, MAXBONENAME);
				}
			}
			
			/* Fix animation data attached to this object */
			// TODO: should we be using the database wide version instead (since drivers may break)
			if (ob->adt) {
				/* posechannels only... */
				BKE_animdata_fix_paths_rename(&ob->id, ob->adt, "pose.bones", oldname, newname);
			}
		}
	}
}


static int armature_flip_names_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_edit_object(C);
	bArmature *arm;
	char newname[32];
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm= ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
	{
		BLI_strncpy(newname, ebone->name, sizeof(newname));
		bone_flip_name(newname, 1);		// 1 = do strip off number extensions
		ED_armature_bone_rename(arm, ebone->name, newname);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_flip_names (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Flip Names";
	ot->idname= "ARMATURE_OT_flip_names";
	ot->description= "Flips (and corrects) the names of selected bones.";
	
	/* api callbacks */
	ot->exec= armature_flip_names_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int armature_autoside_names_exec (bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_edit_object(C);
	bArmature *arm;
	char newname[32];
	short axis= RNA_enum_get(op->ptr, "type");
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm= ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
	{
		BLI_strncpy(newname, ebone->name, sizeof(newname));
		bone_autoside_name(newname, 1, axis, ebone->head[axis], ebone->tail[axis]);
		ED_armature_bone_rename(arm, ebone->name, newname);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_autoside_names (wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[]= {
 		{0, "XAXIS", 0, "X-Axis", "Left/Right"},
		{1, "YAXIS", 0, "Y-Axis", "Front/Back"},
		{2, "ZAXIS", 0, "Z-Axis", "Top/Bottom"},
		{0, NULL, 0, NULL, NULL}};
	
	/* identifiers */
	ot->name= "AutoName by Axis";
	ot->idname= "ARMATURE_OT_autoside_names";
	ot->description= "Automatically renames the selected bones according to which side of the target axis they fall on.";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= armature_autoside_names_exec;
	ot->poll= ED_operator_editarmature;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* settings */
	ot->prop= RNA_def_enum(ot->srna, "type", axis_items, 0, "Axis", "Axis tag names with.");
}



/* if editbone (partial) selected, copy data */
/* context; editmode armature, with mirror editing enabled */
void transform_armature_mirror_update(Object *obedit)
{
	bArmature *arm= obedit->data;
	EditBone *ebo, *eboflip;
	
	for (ebo= arm->edbo->first; ebo; ebo=ebo->next) {
		/* no layer check, correct mirror is more important */
		if (ebo->flag & (BONE_TIPSEL|BONE_ROOTSEL)) {
			eboflip= ED_armature_bone_get_mirrored(arm->edbo, ebo);
			
			if (eboflip) {
				/* we assume X-axis flipping for now */
				if (ebo->flag & BONE_TIPSEL) {
					EditBone *children;
					
					eboflip->tail[0]= -ebo->tail[0];
					eboflip->tail[1]= ebo->tail[1];
					eboflip->tail[2]= ebo->tail[2];
					eboflip->rad_tail= ebo->rad_tail;

					/* Also move connected children, in case children's name aren't mirrored properly */
					for (children=arm->edbo->first; children; children=children->next) {
						if (children->parent == eboflip && children->flag & BONE_CONNECTED) {
							VECCOPY(children->head, eboflip->tail);
							children->rad_head = ebo->rad_tail;
						}
					}
				}
				if (ebo->flag & BONE_ROOTSEL) {
					eboflip->head[0]= -ebo->head[0];
					eboflip->head[1]= ebo->head[1];
					eboflip->head[2]= ebo->head[2];
					eboflip->rad_head= ebo->rad_head;
					
					/* Also move connected parent, in case parent's name isn't mirrored properly */
					if (eboflip->parent && eboflip->flag & BONE_CONNECTED)
					{
						EditBone *parent = eboflip->parent;
						VECCOPY(parent->tail, eboflip->head);
						parent->rad_tail = ebo->rad_head;
					}
				}
				if (ebo->flag & BONE_SELECTED) {
					eboflip->dist= ebo->dist;
					eboflip->roll= -ebo->roll;
					eboflip->xwidth= ebo->xwidth;
					eboflip->zwidth= ebo->zwidth;
				}
			}
		}
	}
}


/*****************************************************************************************************/
/*************************************** SKELETON GENERATOR ******************************************/
/*****************************************************************************************************/

#if 0

/**************************************** SUBDIVISION ALGOS ******************************************/

EditBone * subdivideByAngle(Scene *scene, Object *obedit, ReebArc *arc, ReebNode *head, ReebNode *tail)
{
	bArmature *arm= obedit->data;
	EditBone *lastBone = NULL;
	
	if (scene->toolsettings->skgen_options & SKGEN_CUT_ANGLE)
	{
		ReebArcIterator arc_iter;
		BArcIterator *iter = (BArcIterator*)&arc_iter;
		float *previous = NULL, *current = NULL;
		EditBone *child = NULL;
		EditBone *parent = NULL;
		EditBone *root = NULL;
		float angleLimit = (float)cos(scene->toolsettings->skgen_angle_limit * M_PI / 180.0f);
		
		parent = ED_armature_edit_bone_add(arm, "Bone");
		parent->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
		VECCOPY(parent->head, head->p);
		
		root = parent;
		
		initArcIterator(iter, arc, head);
		IT_next(iter);
		previous = iter->p;
		
		for (IT_next(iter);
			IT_stopped(iter) == 0;
			previous = iter->p, IT_next(iter))
		{
			float vec1[3], vec2[3];
			float len1, len2;
			
			current = iter->p;

			sub_v3_v3v3(vec1, previous, parent->head);
			sub_v3_v3v3(vec2, current, previous);

			len1 = normalize_v3(vec1);
			len2 = normalize_v3(vec2);

			if (len1 > 0.0f && len2 > 0.0f && dot_v3v3(vec1, vec2) < angleLimit)
			{
				VECCOPY(parent->tail, previous);

				child = ED_armature_edit_bone_add(arm, "Bone");
				VECCOPY(child->head, parent->tail);
				child->parent = parent;
				child->flag |= BONE_CONNECTED|BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				
				parent = child; /* new child is next parent */
			}
		}
		VECCOPY(parent->tail, tail->p);
		
		/* If the bone wasn't subdivided, delete it and return NULL
		 * to let subsequent subdivision methods do their thing. 
		 * */
		if (parent == root)
		{
			if(parent==arm->act_edbone) arm->act_edbone= NULL;
			ED_armature_edit_bone_remove(arm, parent);
			parent = NULL;
		}
		
		lastBone = parent; /* set last bone in the chain */
	}
	
	return lastBone;
}

EditBone * test_subdivideByCorrelation(Scene *scene, Object *obedit, ReebArc *arc, ReebNode *head, ReebNode *tail)
{
	EditBone *lastBone = NULL;

	if (scene->toolsettings->skgen_options & SKGEN_CUT_CORRELATION)
	{
		float invmat[4][4] = {	{1, 0, 0, 0},
								{0, 1, 0, 0},
								{0, 0, 1, 0},
								{0, 0, 0, 1}};
		float tmat[3][3] = {	{1, 0, 0},
								{0, 1, 0},
								{0, 0, 1}};
		ReebArcIterator arc_iter;
		BArcIterator *iter = (BArcIterator*)&arc_iter;
		bArmature *arm= obedit->data;
		
		initArcIterator(iter, arc, head);
		
		lastBone = subdivideArcBy(arm, arm->edbo, iter, invmat, tmat, nextAdaptativeSubdivision);
	}
	
  	return lastBone;
}

float arcLengthRatio(ReebArc *arc)
{
	float arcLength = 0.0f;
	float embedLength = 0.0f;
	int i;
	
	arcLength = len_v3v3(arc->head->p, arc->tail->p);
	
	if (arc->bcount > 0)
	{
		/* Add the embedding */
		for ( i = 1; i < arc->bcount; i++)
		{
			embedLength += len_v3v3(arc->buckets[i - 1].p, arc->buckets[i].p);
		}
		/* Add head and tail -> embedding vectors */
		embedLength += len_v3v3(arc->head->p, arc->buckets[0].p);
		embedLength += len_v3v3(arc->tail->p, arc->buckets[arc->bcount - 1].p);
	}
	else
	{
		embedLength = arcLength;
	}
	
	return embedLength / arcLength;	
}

EditBone * test_subdivideByLength(Scene *scene, Object *obedit, ReebArc *arc, ReebNode *head, ReebNode *tail)
{
	EditBone *lastBone = NULL;
	if ((scene->toolsettings->skgen_options & SKGEN_CUT_LENGTH) &&
		arcLengthRatio(arc) >= G.scene->toolsettings->skgen_length_ratio)
	{
		float invmat[4][4] = {	{1, 0, 0, 0},
								{0, 1, 0, 0},
								{0, 0, 1, 0},
								{0, 0, 0, 1}};
		float tmat[3][3] = {	{1, 0, 0},
								{0, 1, 0},
								{0, 0, 1}};
		ReebArcIterator arc_iter;
		BArcIterator *iter = (BArcIterator*)&arc_iter;
		bArmature *arm= obedit->data;
		
		initArcIterator(iter, arc, head);
		
		lastBone = subdivideArcBy(arm, arm->edbo, iter, invmat, tmat, nextLengthSubdivision);
	}
	
	return lastBone;
}

/***************************************** MAIN ALGORITHM ********************************************/

void generateSkeletonFromReebGraph(Scene *scene, ReebGraph *rg)
{
	Object *obedit= scene->obedit; // XXX get from context
	GHash *arcBoneMap = NULL;
	ReebArc *arc = NULL;
	ReebNode *node = NULL;
	Object *src = NULL;
	Object *dst = NULL;
	
	src = scene->basact->object;
	
	if (obedit != NULL)
	{
		ED_armature_from_edit(obedit);
		ED_armature_edit_free(obedit);
	}
	
	dst = add_object(scene, OB_ARMATURE);
	ED_object_base_init_transform(NULL, scene->basact, NULL, NULL); 	// XXX NULL is C, loc, rot
	obedit= scene->basact->object;
	
	/* Copy orientation from source */
	VECCOPY(dst->loc, src->obmat[3]);
	mat4_to_eul( dst->rot,src->obmat);
	mat4_to_size( dst->size,src->obmat);
	
	where_is_object(scene, obedit);
	
	ED_armature_to_edit(obedit);

	arcBoneMap = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	BLI_markdownSymmetry((BGraph*)rg, rg->nodes.first, scene->toolsettings->skgen_symmetry_limit);
	
	for (arc = rg->arcs.first; arc; arc = arc->next) 
	{
		EditBone *lastBone = NULL;
		ReebNode *head, *tail;
		int i;

		/* Find out the direction of the arc through simple heuristics (in order of priority) :
		 * 
		 * 1- Arcs on primary symmetry axis (symmetry == 1) point up (head: high weight -> tail: low weight)
		 * 2- Arcs starting on a primary axis point away from it (head: node on primary axis)
		 * 3- Arcs point down (head: low weight -> tail: high weight)
		 *
		 * Finally, the arc direction is stored in its flag: 1 (low -> high), -1 (high -> low)
		 */

		/* if arc is a symmetry axis, internal bones go up the tree */		
		if (arc->symmetry_level == 1 && arc->tail->degree != 1)
		{
			head = arc->tail;
			tail = arc->head;
			
			arc->flag = -1; /* mark arc direction */
		}
		/* Bones point AWAY from the symmetry axis */
		else if (arc->head->symmetry_level == 1)
		{
			head = arc->head;
			tail = arc->tail;
			
			arc->flag = 1; /* mark arc direction */
		}
		else if (arc->tail->symmetry_level == 1)
		{
			head = arc->tail;
			tail = arc->head;
			
			arc->flag = -1; /* mark arc direction */
		}
		/* otherwise, always go from low weight to high weight */
		else
		{
			head = arc->head;
			tail = arc->tail;
			
			arc->flag = 1; /* mark arc direction */
		}
		
		/* Loop over subdivision methods */	
		for (i = 0; lastBone == NULL && i < SKGEN_SUB_TOTAL; i++)
		{
			switch(scene->toolsettings->skgen_subdivisions[i])
			{
				case SKGEN_SUB_LENGTH:
					lastBone = test_subdivideByLength(scene, obedit, arc, head, tail);
					break;
				case SKGEN_SUB_ANGLE:
					lastBone = subdivideByAngle(scene, obedit, arc, head, tail);
					break;
				case SKGEN_SUB_CORRELATION:
					lastBone = test_subdivideByCorrelation(scene, obedit, arc, head, tail);
					break;
			}
		}
	
		if (lastBone == NULL)
		{
			EditBone	*bone;
			bone = ED_armature_edit_bone_add(obedit->data, "Bone");
			bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
			
			VECCOPY(bone->head, head->p);
			VECCOPY(bone->tail, tail->p);
			
			/* set first and last bone, since there's only one */
			lastBone = bone;
		}
		
		BLI_ghash_insert(arcBoneMap, arc, lastBone);
	}

	/* Second pass, setup parent relationship between arcs */
	for (node = rg->nodes.first; node; node = node->next)
	{
		ReebArc *incomingArc = NULL;
		int i;

		for (i = 0; i < node->degree; i++)
		{
			arc = (ReebArc*)node->arcs[i];

			/* if arc is incoming into the node */
			if ((arc->head == node && arc->flag == -1) || (arc->tail == node && arc->flag == 1))
			{
				if (incomingArc == NULL)
				{
					incomingArc = arc;
					/* loop further to make sure there's only one incoming arc */
				}
				else
				{
					/* skip this node if more than one incomingArc */
					incomingArc = NULL;
					break; /* No need to look further, we are skipping already */
				}
			}
		}

		if (incomingArc != NULL)
		{
			EditBone *parentBone = BLI_ghash_lookup(arcBoneMap, incomingArc);

			/* Look for outgoing arcs and parent their bones */
			for (i = 0; i < node->degree; i++)
			{
				arc = node->arcs[i];

				/* if arc is outgoing from the node */
				if ((arc->head == node && arc->flag == 1) || (arc->tail == node && arc->flag == -1))
				{
					EditBone *childBone = BLI_ghash_lookup(arcBoneMap, arc);

					/* find the root bone */
					while(childBone->parent != NULL)
					{
						childBone = childBone->parent;
					}

					childBone->parent = parentBone;
					childBone->flag |= BONE_CONNECTED;
				}
			}
		}
	}
	
	BLI_ghash_free(arcBoneMap, NULL, NULL);
	
	BIF_undo_push("Generate Skeleton");
}

void generateSkeleton(Scene *scene)
{
	ReebGraph *reebg;
	
//	setcursor_space(SPACE_VIEW3D, CURSOR_WAIT);
	
	reebg = BIF_ReebGraphFromEditMesh();

	generateSkeletonFromReebGraph(scene, reebg);

	REEB_freeGraph(reebg);

	//setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
}

#endif
