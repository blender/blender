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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 * editarmature.c: Interface for creating and posing armature objects
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h> 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

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
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_ghash.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"
#include "BKE_utildefines.h"
#include "BKE_modifier.h"

#include "BIF_editaction.h"
#include "BIF_editmode_undo.h"
#include "BIF_editdeform.h"
#include "BIF_editarmature.h"
#include "BIF_editconstraint.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_meshlaplacian.h"
#include "BIF_meshtools.h"
#include "BIF_poseobject.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_transform.h"

#include "BDR_editobject.h"
#include "BDR_drawobject.h"

#include "BSE_edit.h"
#include "BSE_view.h"
#include "BSE_trans_types.h"

#include "PIL_time.h"

#include "reeb.h" // FIX ME

#include "mydevice.h"
#include "blendef.h"
#include "nla.h"

extern	float center[3], centroid[3];	/* Originally defined in editobject.c */

/*	Macros	*/
#define TEST_EDITARMATURE {if(G.obedit==0) return; if( (G.vd->lay & G.obedit->lay)==0 ) return;}

/* prototypes for later */
static EditBone *editbone_name_exists (ListBase *ebones, char *name);	// proto for below

/* **************** tools on Editmode Armature **************** */

/* converts Bones to EditBone list, used for tools as well */
void make_boneList(ListBase *list, ListBase *bones, EditBone *parent)
{
	EditBone	*eBone;
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
		
		eBone->roll= 0.0;
		
		/* roll fixing */
		VecSubf(delta, eBone->tail, eBone->head);
		vec_roll_to_mat3(delta, 0.0, postmat);
		
		Mat3CpyMat4(premat, curBone->arm_mat);
		
		Mat3Inv(imat, postmat);
		Mat3MulMat3(difmat, imat, premat);
		
		eBone->roll = atan2(difmat[2][0], difmat[2][2]);
		
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
		
		BLI_addtail(list, eBone);
		
		/*	Add children if necessary */
		if (curBone->childbase.first) 
			make_boneList(list, &curBone->childbase, eBone);
	}
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
			VecSubf(delta, ebone->tail, ebone->head);
			vec_roll_to_mat3(delta, ebone->roll, premat);
			
			/* Get the bone postmat */
			Mat3CpyMat4(postmat, curBone->arm_mat);
			
			Mat3Inv(imat, premat);
			Mat3MulMat3(difmat, imat, postmat);
#if 0
			printf ("Bone %s\n", curBone->name);
			printmatrix4("premat", premat);
			printmatrix4("postmat", postmat);
			printmatrix4("difmat", difmat);
			printf ("Roll = %f\n",  (-atan2(difmat[2][0], difmat[2][2]) * (180.0/M_PI)));
#endif
			curBone->roll = -atan2(difmat[2][0], difmat[2][2]);
			
			/* and set restposition again */
			where_is_armature_bone(curBone, curBone->parent);
		}
		fix_bonelist_roll(&curBone->childbase, editbonelist);
	}
}

/* converts the editbones back to the armature */
void editbones_to_armature (ListBase *list, Object *ob)
{
	bArmature *arm;
	EditBone *eBone, *neBone;
	Bone	*newBone;
	Object *obt;
	
	arm = get_armature(ob);
	if (!list) return;
	if (!arm) return;
	
	/* armature bones */
	free_bones(arm);
	
	/* remove zero sized bones, this gives instable restposes */
	for (eBone=list->first; eBone; eBone= neBone) {
		float len= VecLenf(eBone->head, eBone->tail);
		neBone= eBone->next;
		if (len <= FLT_EPSILON) {
			EditBone *fBone;
			
			/*	Find any bones that refer to this bone	*/
			for (fBone=list->first; fBone; fBone= fBone->next) {
				if (fBone->parent==eBone)
					fBone->parent= eBone->parent;
			}
			printf("Warning: removed zero sized bone: %s\n", eBone->name);
			BLI_freelinkN(list, eBone);
		}
	}
	
	/*	Copy the bones from the editData into the armature */
	for (eBone=list->first; eBone; eBone=eBone->next) {
		newBone= MEM_callocN(sizeof(Bone), "bone");
		eBone->temp= newBone;	/* Associate the real Bones with the EditBones */
		
		BLI_strncpy(newBone->name, eBone->name, 32);
		memcpy(newBone->head, eBone->head, sizeof(float)*3);
		memcpy(newBone->tail, eBone->tail, sizeof(float)*3);
		newBone->flag= eBone->flag;
		if (eBone->flag & BONE_ACTIVE) 
			newBone->flag |= BONE_SELECTED;	/* important, editbones can be active with only 1 point selected */
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
	}
	
	/*	Fix parenting in a separate pass to ensure ebone->bone connections
		are valid at this point */
	for (eBone=list->first;eBone;eBone=eBone->next) {
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
				VecSubf(delta, eBone->parent->tail, eBone->parent->head);
				vec_roll_to_mat3(delta, eBone->parent->roll, M_parentRest);
				
				/* Get this bone's  matrix (rotation only) */
				VecSubf(delta, eBone->tail, eBone->head);
				vec_roll_to_mat3(delta, eBone->roll, M_boneRest);
				
				/* Invert the parent matrix */
				Mat3Inv(iM_parentRest, M_parentRest);
				
				/* Get the new head and tail */
				VecSubf(newBone->head, eBone->head, eBone->parent->tail);
				VecSubf(newBone->tail, eBone->tail, eBone->parent->tail);
				
				Mat3MulVecfl(iM_parentRest, newBone->head);
				Mat3MulVecfl(iM_parentRest, newBone->tail);
			}
		}
		/*	...otherwise add this bone to the armature's bonebase */
		else
			BLI_addtail(&arm->bonebase, newBone);
	}
	
	/* Make a pass through the new armature to fix rolling */
	/* also builds restposition again (like where_is_armature) */
	fix_bonelist_roll(&arm->bonebase, list);
	
	/* so all users of this armature should get rebuilt */
	for (obt= G.main->object.first; obt; obt= obt->id.next) {
		if (obt->data==arm)
			armature_rebuild_pose(obt, arm);
	}
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
}



void apply_rot_armature (Object *ob, float mat[3][3])
{
	ListBase	list;
	EditBone *ebone;
	bArmature *arm;
	float scale = Mat3ToScalef(mat);	/* store the scale of the matrix here to use on envelopes */
	arm = get_armature(ob);

	if (!arm)
		return;	
	
	/* Put the armature into editmode */
	list.first= list.last = NULL;
	make_boneList(&list, &arm->bonebase, NULL);

	/* Do the rotations */
	for (ebone = list.first; ebone; ebone=ebone->next){
		Mat3MulVecfl(mat, ebone->head);
		Mat3MulVecfl(mat, ebone->tail);
		
		ebone->rad_head	*= scale;
		ebone->rad_tail	*= scale;
		ebone->dist		*= scale;
	}
	
	/* Turn the list into an armature */
	editbones_to_armature(&list, ob);
	
	/* Free the editbones */
	if (list.first){
		BLI_freelistN (&list);
	}
}

/* 0 == do center, 1 == center new, 2 == center cursor */
void docenter_armature (Object *ob, int centermode)
{
	ListBase	list;
	EditBone *ebone;
	bArmature *arm;
	float cent[3] = {0.0f, 0.0f, 0.0f};
	float min[3], max[3];
	float omat[3][3];
		
	arm = get_armature(ob);
	if (!arm) return;

	/* Put the armature into editmode */
	list.first= list.last = NULL;
	make_boneList(&list, &arm->bonebase, NULL);

	/* Find the centerpoint */
	if (centermode == 2) {
		VECCOPY(cent, give_cursor());
		Mat4Invert(ob->imat, ob->obmat);
		Mat4MulVecfl(ob->imat, cent);
	}
	else {
		INIT_MINMAX(min, max);
		
		for (ebone= list.first; ebone; ebone=ebone->next) {
			DO_MINMAX(ebone->head, min, max);
			DO_MINMAX(ebone->tail, min, max);
		}
		
		cent[0]= (min[0]+max[0])/2.0f;
		cent[1]= (min[1]+max[1])/2.0f;
		cent[2]= (min[2]+max[2])/2.0f;
	}
	
	/* Do the adjustments */
	for (ebone= list.first; ebone; ebone=ebone->next){
		VecSubf(ebone->head, ebone->head, cent);
		VecSubf(ebone->tail, ebone->tail, cent);
	}
	
	/* Turn the list into an armature */
	editbones_to_armature(&list, ob);
	
	/* Free the editbones */
	if (list.first){
		BLI_freelistN(&list);
	}
	
	/* Adjust object location for new centerpoint */
	if(centermode && G.obedit==0) {
		Mat3CpyMat4(omat, ob->obmat);
		
		Mat3MulVecfl(omat, cent);
		ob->loc[0]+= cent[0];
		ob->loc[1]+= cent[1];
		ob->loc[2]+= cent[2];
	}
}

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
						bActionConstraint *data= con->data;
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

int join_armature(void)
{
	Object	*ob;
	bArmature *arm;
	Base	*base, *nextbase;
	bPose *pose, *opose;
	bPoseChannel *pchan, *pchann;
	ListBase ebbase, eblist;
	EditBone *curbone;
	float	mat[4][4], oimat[4][4];
	
	/*	Ensure we're not in editmode and that the active object is an armature*/
	/* if(G.obedit) return; */ /* Alredy checked in join_menu() */
	
	ob= OBACT;
	if (ob->type!=OB_ARMATURE) return 0;
	if (object_data_is_libdata(ob)) {
		error_libdata();
		return 0;
	}
	arm= get_armature(ob); 
	
	/* Get editbones of active armature to add editbones to */
	ebbase.first=ebbase.last= NULL;
	make_boneList(&ebbase, &arm->bonebase, NULL);
	
	/* get pose of active object and move it out of posemode */
	pose= ob->pose;
	ob->flag &= ~OB_POSEMODE;
	BASACT->flag &= ~OB_POSEMODE;
	
	for (base=FIRSTBASE; base; base=nextbase) {
		nextbase = base->next;
		if (TESTBASE(base)){
			if ((base->object->type==OB_ARMATURE) && (base->object!=ob)) {
				/* Make a list of editbones in current armature */
				eblist.first=eblist.last= NULL;
				make_boneList(&eblist, &((bArmature *)base->object->data)->bonebase, NULL);
				
				/* Get Pose of current armature */
				opose= base->object->pose;
				base->object->flag &= ~OB_POSEMODE;
				BASACT->flag &= ~OB_POSEMODE;
				
				/* Find the difference matrix */
				Mat4Invert(oimat, ob->obmat);
				Mat4MulMat4(mat, base->object->obmat, oimat);
				
				/* Copy bones and posechannels from the object to the edit armature */
				for (pchan=opose->chanbase.first; pchan; pchan=pchann) {
					pchann= pchan->next;
					curbone= editbone_name_exists(&eblist, pchan->name);
					
					/* Get new name */
					unique_editbone_name(&ebbase, curbone->name);
					
					/* Transform the bone */
					{
						float premat[4][4];
						float postmat[4][4];
						float difmat[4][4];
						float imat[4][4];
						float temp[3][3];
						float delta[3];
						
						/* Get the premat */
						VecSubf(delta, curbone->tail, curbone->head);
						vec_roll_to_mat3(delta, curbone->roll, temp);
						
						Mat4MulMat34(premat, temp, mat);
						
						Mat4MulVecfl(mat, curbone->head);
						Mat4MulVecfl(mat, curbone->tail);
						
						/* Get the postmat */
						VecSubf(delta, curbone->tail, curbone->head);
						vec_roll_to_mat3(delta, curbone->roll, temp);
						Mat4CpyMat3(postmat, temp);
						
						/* Find the roll */
						Mat4Invert(imat, premat);
						Mat4MulMat4(difmat, postmat, imat);
						
						curbone->roll -= atan2(difmat[2][0], difmat[2][2]);
					}
					
					/* Fix Constraints and Other Links to this Bone and Armature */
					joined_armature_fix_links(ob, base->object, pchan, curbone);
					
					/* Rename pchan */
					sprintf(pchan->name, curbone->name);
					
					/* Jump Ship! */
					BLI_remlink(&eblist, curbone);
					BLI_addtail(&ebbase, curbone);
					
					BLI_remlink(&opose->chanbase, pchan);
					BLI_addtail(&pose->chanbase, pchan);
				}
				
				free_and_unlink_base(base);
			}
		}
	}
	
	DAG_scene_sort(G.scene);	// because we removed object(s)
	
	editbones_to_armature(&ebbase, ob);
	if (ebbase.first) BLI_freelistN(&ebbase);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	return 1;
}

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
 */
static void separate_armature_bones (Object *ob, short sel) 
{
	ListBase edbo = {NULL, NULL};
	bArmature *arm= (bArmature *)ob->data;
	bPoseChannel *pchan, *pchann;
	EditBone *curbone;
	
	/* make local set of editbones to manipulate here */
	make_boneList(&edbo, &arm->bonebase, NULL);
	
	/* go through pose-channels, checking if a bone should be removed */
	for (pchan=ob->pose->chanbase.first; pchan; pchan=pchann) {
		pchann= pchan->next;
		curbone= editbone_name_exists(&edbo, pchan->name);
		
		/* check if bone needs to be removed */
		if ( (sel && (curbone->flag & BONE_SELECTED)) ||
			 (!sel && !(curbone->flag & BONE_SELECTED)) )
		{
			EditBone *ebo;
			bPoseChannel *pchn;
			
			/* clear the bone->parent var of any bone that had this as its parent  */
			for (ebo= edbo.first; ebo; ebo= ebo->next) {
				if (ebo->parent == curbone) {
					ebo->parent= NULL;
					ebo->temp= NULL; /* this is needed to prevent random crashes with in editbones_to_armature */
					ebo->flag &= ~BONE_CONNECTED;
				}
			}
			
			/* clear the pchan->parent var of any pchan that had this as its parent */
			for (pchn= ob->pose->chanbase.first; pchn; pchn=pchn->next) {
				if (pchn->parent == pchan)
					pchn->parent= NULL;
			}
			
			/* free any of the extra-data this pchan might have */
			if (pchan->path) MEM_freeN(pchan->path);
			free_constraints(&pchan->constraints);
			
			/* get rid of unneeded bone */
			BLI_freelinkN(&edbo, curbone);
			BLI_freelinkN(&ob->pose->chanbase, pchan);
		}
	}
	
	/* exit editmode (recalculates pchans too) */
	editbones_to_armature(&edbo, ob);
	BLI_freelistN(&edbo);
}

void separate_armature (void)
{
	Object *oldob, *newob;
	Base *base, *oldbase, *newbase;
	bArmature *arm;
	
	if ( G.vd==0 || (G.vd->lay & G.obedit->lay)==0 ) return;
	if ( okee("Separate")==0 ) return;

	waitcursor(1);
	
	arm= G.obedit->data;
	
	/* we are going to do this as follows (unlike every other instance of separate):
	 *	1. exit editmode +posemode for active armature/base. Take note of what this is.
	 *	2. duplicate base - BASACT is the new one now
	 *	3. for each of the two armatures, enter editmode -> remove appropriate bones -> exit editmode + recalc
	 *	4. fix constraint links
	 *	5. make original armature active and enter editmode
	 */
	
	/* 1) only edit-base selected */
	base= FIRSTBASE;
	for (base= FIRSTBASE; base; base= base->next) {
		if (base->lay & G.vd->lay) {
			if (base->object==G.obedit) base->flag |= 1;
			else base->flag &= ~1;
		}
	}
	
	/* 1) store starting settings and exit editmode */
	oldob= G.obedit;
	oldbase= BASACT;
	oldob->flag &= ~OB_POSEMODE;
	oldbase->flag &= ~OB_POSEMODE;
	
	load_editArmature();
	free_editArmature();
	
	/* 2) duplicate base */
	adduplicate(1, USER_DUP_ARM); /* no transform and zero so do get a linked dupli */
	
	newbase= BASACT; /* basact is set in adduplicate() */
	newob= newbase->object;		
	newbase->flag &= ~SELECT;
	
	
	/* 3) remove bones that shouldn't still be around on both armatures */
	separate_armature_bones(oldob, 1);
	separate_armature_bones(newob, 0);
	
	
	/* 4) fix links before depsgraph flushes */ // err... or after?
	separated_armature_fix_links(oldob, newob);
	
	DAG_object_flush_update(G.scene, oldob, OB_RECALC_DATA);	/* this is the original one */
	DAG_object_flush_update(G.scene, newob, OB_RECALC_DATA);	/* this is the separated one */
	
	
	/* 5) restore original conditions */
	G.obedit= oldob;
	BASACT= oldbase;
	BASACT->flag |= SELECT;
	
	make_editArmature();
	
	/* recalc/redraw + cleanup */
	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Separate Armature");
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
static void *get_bone_from_selectbuffer(Base *base, unsigned int *buffer, short hits, short findunsel)
{
	Object *ob= base->object;
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
				if (G.obedit==NULL || base->object!=G.obedit) {
					/* no singular posemode, so check for correct object */
					if(base->selcol == (hitresult & 0xFFFF)) {
						bone = get_indexed_bone(ob, hitresult);

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
					ebone = BLI_findlink(&G.edbo, hitresult);
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
static void *get_nearest_bone (short findunsel)
{
	unsigned int buffer[MAXPICKBUF];
	short hits;
	
	persp(PERSP_VIEW);
	
	glInitNames();
	hits= view3d_opengl_select(buffer, MAXPICKBUF, 0, 0, 0, 0);

	if (hits>0)
		return get_bone_from_selectbuffer(BASACT, buffer, hits, findunsel);
	
	return NULL;
}

/* used by posemode and editmode */
void select_bone_parent (void)
{
	Object *ob;
	bArmature *arm;	
	
	/* get data */
	if (G.obedit)
		ob= G.obedit;
	else if (OBACT)
		ob= OBACT;
	else
		return;
	arm= (bArmature *)ob->data;
	
	/* determine which mode armature is in */
	if ((!G.obedit) && (ob->flag & OB_POSEMODE)) {
		/* deal with pose channels */
		/* channels are sorted on dependency, so the loop below won't result in a flood-select */
		bPoseChannel *pchan=NULL;
		
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			/* check if bone in original selection */
			if (pchan->bone->flag & BONE_SELECTED) {
				bPoseChannel *chanpar= pchan->parent;
				
				/* check if any parent */
				if ((chanpar) && ((chanpar->bone->flag & BONE_SELECTED)==0)) {
					chanpar->bone->flag |= BONE_SELECTED;
					select_actionchannel_by_name (ob->action, pchan->name, 1);
				}
			}
		}
	}
	else if (G.obedit) {
		/* deal with editbones */
		EditBone *curbone, *parbone, *parpar;
		
		/* prevent floods */
		for (curbone= G.edbo.first; curbone; curbone= curbone->next)
			curbone->temp= NULL;
		
		for (curbone= G.edbo.first; curbone; curbone= curbone->next) {
			/* check if bone selected */
			if ((curbone->flag & BONE_SELECTED) && curbone->temp==NULL) {
				parbone= curbone->parent;
				
				/* check if any parent */
				if ((parbone) && ((parbone->flag & BONE_SELECTED)==0)) {
					/* select the parent bone */
					parbone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
					
					/* check if parent has parent */
					parpar= parbone->parent;
					
					if ((parpar) && (parbone->flag & BONE_CONNECTED)) {
						parpar->flag |= BONE_TIPSEL;
					}
					/* tag this bone to not flood selection */
					parbone->temp= parbone;
				}
			}
		}
		
		/* to be sure... */
		for (curbone= G.edbo.first; curbone; curbone= curbone->next)
			curbone->temp= NULL;
		
	}
	
	/* undo + redraw pushes */
	countall(); // flushes selection!

	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Select Parent");
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

/* used by posemode and editmode */
void setflag_armature (short mode)
{
	Object *ob;
	bArmature *arm;	
	int flag;
	
	/* get data */
	if (G.obedit)
		ob= G.obedit;
	else if (OBACT)
		ob= OBACT;
	else
		return;
	arm= (bArmature *)ob->data;
	
	/* get flag to set (sync these with the ones used in eBone_Flag */
	if (mode == 2)
		flag= pupmenu("Disable Setting%t|Draw Wire%x1|Deform%x2|Mult VG%x3|Hinge%x4|No Scale%x5");
	else if (mode == 1)
		flag= pupmenu("Enable Setting%t|Draw Wire%x1|Deform%x2|Mult VG%x3|Hinge%x4|No Scale%x5");
	else
		flag= pupmenu("Toggle Setting%t|Draw Wire%x1|Deform%x2|Mult VG%x3|Hinge%x4|No Scale%x5");
	switch (flag) {
		case 1: 	flag = BONE_DRAWWIRE; 	break;
		case 2:		flag = BONE_NO_DEFORM; break;
		case 3: 	flag = BONE_MULT_VG_ENV; break;
		case 4:		flag = BONE_HINGE; break;
		case 5:		flag = BONE_NO_SCALE; break;
		default:	return;
	}
	
	/* determine which mode armature is in */
	if ((!G.obedit) && (ob->flag & OB_POSEMODE)) {
		/* deal with pose channels */
		bPoseChannel *pchan;
		
		/* set setting */
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ((pchan->bone) && (arm->layer & pchan->bone->layer)) {
				if (pchan->bone->flag & BONE_SELECTED) {
					bone_setflag(&pchan->bone->flag, flag, mode);
				}
			}
		}
	}
	else if (G.obedit) {
		/* deal with editbones */
		EditBone *curbone;
		
		/* set setting */
		for (curbone= G.edbo.first; curbone; curbone= curbone->next) {
			if (arm->layer & curbone->layer) {
				if (curbone->flag & BONE_SELECTED) {
					bone_setflag(&curbone->flag, flag, mode);
				}
			}
		}
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Change Bone Setting");
}

/* **************** END PoseMode & EditMode *************************** */
/* **************** Posemode stuff ********************** */


static void selectconnected_posebonechildren (Object *ob, Bone *bone)
{
	Bone *curBone;
	
	if (!(bone->flag & BONE_CONNECTED))
		return;
	
	select_actionchannel_by_name (ob->action, bone->name, !(G.qual & LR_SHIFTKEY));
	
	if (G.qual & LR_SHIFTKEY)
		bone->flag &= ~BONE_SELECTED;
	else
		bone->flag |= BONE_SELECTED;
	
	for (curBone=bone->childbase.first; curBone; curBone=curBone->next){
		selectconnected_posebonechildren (ob, curBone);
	}
}

/* within active object context */
void selectconnected_posearmature(void)
{
	Bone *bone, *curBone, *next;
	Object *ob= OBACT;
	
	if(!ob || !ob->pose) return;
	
	if (G.qual & LR_SHIFTKEY)
		bone= get_nearest_bone(0);
	else
		bone = get_nearest_bone(1);
	
	if (!bone)
		return;
	
	/* Select parents */
	for (curBone=bone; curBone; curBone=next){
		select_actionchannel_by_name (ob->action, curBone->name, !(G.qual & LR_SHIFTKEY));
		if (G.qual & LR_SHIFTKEY)
			curBone->flag &= ~BONE_SELECTED;
		else
			curBone->flag |= BONE_SELECTED;
		
		if (curBone->flag & BONE_CONNECTED)
			next=curBone->parent;
		else
			next=NULL;
	}
	
	/* Select children */
	for (curBone=bone->childbase.first; curBone; curBone=next){
		selectconnected_posebonechildren (ob, curBone);
	}
	
	countall(); // flushes selection!

	allqueue (REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWOOPS, 0);
	BIF_undo_push("Select connected");

}

/* **************** END Posemode stuff ********************** */
/* **************** EditMode stuff ********************** */

/* called in space.c */
void selectconnected_armature(void)
{
	EditBone *bone, *curBone, *next;

	if (G.qual & LR_SHIFTKEY)
		bone= get_nearest_bone(0);
	else
		bone= get_nearest_bone(1);

	if (!bone)
		return;

	/* Select parents */
	for (curBone=bone; curBone; curBone=next){
		if (G.qual & LR_SHIFTKEY){
			curBone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
		}
		else{
			curBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
		}

		if (curBone->flag & BONE_CONNECTED)
			next=curBone->parent;
		else
			next=NULL;
	}

	/* Select children */
	while (bone){
		for (curBone=G.edbo.first; curBone; curBone=next){
			next = curBone->next;
			if (curBone->parent == bone){
				if (curBone->flag & BONE_CONNECTED){
					if (G.qual & LR_SHIFTKEY)
						curBone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
					else
						curBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL);
					bone=curBone;
					break;
				}
				else{ 
					bone=NULL;
					break;
				}
			}
		}
		if (!curBone)
			bone=NULL;

	}

	countall(); // flushes selection!

	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Select connected");

}

/* does bones and points */
/* note that BONE ROOT only gets drawn for root bones (or without IK) */
static EditBone * get_nearest_editbonepoint (int findunsel, int *selmask)
{
	EditBone *ebone;
	unsigned int buffer[MAXPICKBUF];
	unsigned int hitresult, besthitresult=BONESEL_NOSEL;
	int i, mindep= 4;
	short hits, mval[2];

	persp(PERSP_VIEW);

	glInitNames();
	
	getmouseco_areawin(mval);
	hits= view3d_opengl_select(buffer, MAXPICKBUF, mval[0]-5, mval[1]-5, mval[0]+5, mval[1]+5);
	if(hits==0)
		hits= view3d_opengl_select(buffer, MAXPICKBUF, mval[0]-12, mval[1]-12, mval[0]+12, mval[1]+12);
		
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
					
					ebone = BLI_findlink(&G.edbo, hitresult & ~BONESEL_ANY);
					
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
			
			ebone= BLI_findlink(&G.edbo, besthitresult & ~BONESEL_ANY);
			
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

static void delete_bone(EditBone* exBone)
{
	EditBone *curBone;
	
	/* Find any bones that refer to this bone */
	for (curBone=G.edbo.first;curBone;curBone=curBone->next) {
		if (curBone->parent==exBone) {
			curBone->parent=exBone->parent;
			curBone->flag &= ~BONE_CONNECTED;
		}
	}
	
	BLI_freelinkN(&G.edbo,exBone);
}

/* only editmode! */
void delete_armature(void)
{
	bArmature *arm= G.obedit->data;
	EditBone	*curBone, *next;
	bConstraint *con;
	
	TEST_EDITARMATURE;
	if (okee("Erase selected bone(s)")==0) return;

	/* Select mirrored bones */
	if (arm->flag & ARM_MIRROR_EDIT) {
		for (curBone=G.edbo.first; curBone; curBone=curBone->next) {
			if (arm->layer & curBone->layer) {
				if (curBone->flag & BONE_SELECTED) {
					next = armature_bone_get_mirrored(curBone);
					if (next)
						next->flag |= BONE_SELECTED;
				}
			}
		}
	}
	
	/*  First erase any associated pose channel */
	if (G.obedit->pose) {
		bPoseChannel *chan, *next;
		for (chan=G.obedit->pose->chanbase.first; chan; chan=next) {
			next= chan->next;
			curBone = editbone_name_exists(&G.edbo, chan->name);
			
			if (curBone && (curBone->flag & BONE_SELECTED) && (arm->layer & curBone->layer)) {
				free_constraints(&chan->constraints);
				BLI_freelinkN (&G.obedit->pose->chanbase, chan);
			}
			else {
				for (con= chan->constraints.first; con; con= con->next) {
					bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct= targets.first; ct; ct= ct->next) {
							if (ct->tar == G.obedit) {
								if (ct->subtarget[0]) {
									curBone = editbone_name_exists(&G.edbo, ct->subtarget);
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
	
	
	for (curBone=G.edbo.first;curBone;curBone=next) {
		next=curBone->next;
		if (arm->layer & curBone->layer) {
			if (curBone->flag & BONE_SELECTED)
				delete_bone(curBone);
		}
	}
	
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
	countall(); // flushes selection!
	
	BIF_undo_push("Delete bone(s)");
}

/* context: editmode armature */
void mouse_armature(void)
{
	EditBone *nearBone = NULL, *ebone;
	int	selmask;

	nearBone= get_nearest_editbonepoint(1, &selmask);
	if (nearBone) {
		
		if (!(G.qual & LR_SHIFTKEY)) {
			deselectall_armature(0, 0);
		}
		
		/* by definition the non-root connected bones have no root point drawn,
	       so a root selection needs to be delivered to the parent tip,
	       countall() (bad location) flushes these flags */
		
		if(selmask & BONE_SELECTED) {
			if(nearBone->parent && (nearBone->flag & BONE_CONNECTED)) {
				/* click in a chain */
				if(G.qual & LR_SHIFTKEY) {
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
				if(G.qual & LR_SHIFTKEY) {
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
			if ((G.qual & LR_SHIFTKEY) && (nearBone->flag & selmask))
				nearBone->flag &= ~selmask;
			else
				nearBone->flag |= selmask;
		}

		countall(); // flushes selection!
		
		if(nearBone) {
			/* then now check for active status */
			for (ebone=G.edbo.first;ebone;ebone=ebone->next) ebone->flag &= ~BONE_ACTIVE;
			if(nearBone->flag & BONE_SELECTED) nearBone->flag |= BONE_ACTIVE;
		}
		
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWOOPS, 0);
	}

	rightmouse_transform();
}

void free_editArmature(void)
{
	/*	Clear the editbones list */
	if (G.edbo.first)
		BLI_freelistN(&G.edbo);
}

void remake_editArmature(void)
{
	if(okee("Reload original data")==0) return;
	
	make_editArmature();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWBUTSHEAD, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	
//	BIF_undo_push("Delete bone");
}

/* Put object in EditMode */
void make_editArmature(void)
{
	bArmature *arm;
	
	if (G.obedit==0) return;
	
	free_editArmature();
	
	arm= get_armature(G.obedit);
	if (!arm) return;
	
	make_boneList(&G.edbo, &arm->bonebase,NULL);
}

/* put EditMode back in Object */
void load_editArmature(void)
{
	bArmature *arm;

	arm= get_armature(G.obedit);
	if (!arm) return;
	
	editbones_to_armature(&G.edbo, G.obedit);
}

/* toggle==0: deselect
   toggle==1: swap 
   toggle==2: only active tag
*/
void deselectall_armature(int toggle, int doundo)
{
	bArmature *arm= G.obedit->data;
	EditBone	*eBone;
	int			sel=1;

	if(toggle==1) {
		/*	Determine if there are any selected bones
			And therefore whether we are selecting or deselecting */
		for (eBone=G.edbo.first;eBone;eBone=eBone->next){
//			if(arm->layer & eBone->layer) {
				if (eBone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)){
					sel=0;
					break;
				}
//			}
		}
	}
	else sel= toggle;
	
	/*	Set the flags */
	for (eBone=G.edbo.first;eBone;eBone=eBone->next){
		if (sel==1) {
			if(arm->layer & eBone->layer && (eBone->flag & BONE_HIDDEN_A)==0) {
				eBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				if(eBone->parent)
					eBone->parent->flag |= (BONE_TIPSEL);
			}
		}
		else if (sel==2)
			eBone->flag &= ~(BONE_ACTIVE);
		else
			eBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL | BONE_ACTIVE);
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
	
	countall(); // flushes selection!
	if (doundo) {
		if (sel==1) BIF_undo_push("Select All");
		else BIF_undo_push("Deselect All");
	}
}

/* Sets the roll value of selected bones, depending on the mode
 * 	mode == 0: their z-axes point upwards 
 * 	mode == 1: their z-axes point towards 3d-cursor
 */
void auto_align_armature(short mode)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebone;
	EditBone *flipbone = NULL;
	float	delta[3];
	float	curmat[3][3];
	float  	*cursor= give_cursor();
		
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
			if (arm->flag & ARM_MIRROR_EDIT)
				flipbone = armature_bone_get_mirrored(ebone);
			
			if ((ebone->flag & BONE_SELECTED) || 
				(flipbone && flipbone->flag & BONE_SELECTED)) 
			{
				/* specific method used to calculate roll depends on mode */
				if (mode == 1) {
					/* Z-Axis point towards cursor */
					float	mat[4][4], tmat[4][4], imat[4][4];
					float 	rmat[4][4], rot[3];
					float	vec[3];
					
					/* find the current bone matrix as a 4x4 matrix (in Armature Space) */
					VecSubf(delta, ebone->tail, ebone->head);
					vec_roll_to_mat3(delta, ebone->roll, curmat);
					Mat4CpyMat3(mat, curmat);
					VECCOPY(mat[3], ebone->head);
					
					/* multiply bone-matrix by object matrix (so that bone-matrix is in WorldSpace) */
					Mat4MulMat4(tmat, mat, G.obedit->obmat);
					Mat4Invert(imat, tmat);
					
					/* find position of cursor relative to bone */
					VecMat4MulVecfl(vec, imat, cursor);
					
					/* check that cursor is in usable position */
					if ((IS_EQ(vec[0], 0)==0) && (IS_EQ(vec[2], 0)==0)) {
						/* Compute a rotation matrix around y */
						rot[1] = atan2(vec[0], vec[2]);
						rot[0] = rot[2] = 0.0f;
						EulToMat4(rot, rmat);
						
						/* Multiply the bone matrix by rotation matrix. This should be new bone-matrix */
						Mat4MulMat4(tmat, rmat, mat);
						Mat3CpyMat4(curmat, tmat);
						
						/* Now convert from new bone-matrix, back to a roll value (in radians) */
						mat3_to_vec_roll(curmat, delta, &ebone->roll);
					}
				}
				else { 
					/* Z-Axis Point Up */
					float	xaxis[3]={1.0, 0.0, 0.0}, yaxis[3], zaxis[3]={0.0, 0.0, 1.0};
					float	targetmat[3][3], imat[3][3], diffmat[3][3];
					
					/* Find the current bone matrix */
					VecSubf(delta, ebone->tail, ebone->head);
					vec_roll_to_mat3(delta, 0.0, curmat);
					
					/* Make new matrix based on y axis & z-up */
					VECCOPY (yaxis, curmat[1]);
					
					Mat3One(targetmat);
					VECCOPY (targetmat[0], xaxis);
					VECCOPY (targetmat[1], yaxis);
					VECCOPY (targetmat[2], zaxis);
					Mat3Ortho(targetmat);
					
					/* Find the difference between the two matrices */
					Mat3Inv(imat, targetmat);
					Mat3MulMat3(diffmat, imat, curmat);
					
					ebone->roll = atan2(diffmat[2][0], diffmat[2][2]);
				}				
			}
		}
	}
}

/* **************** undo for armatures ************** */

static void undoBones_to_editBones(void *lbv)
{
	ListBase *lb= lbv;
	EditBone *ebo, *newebo;
	
	BLI_freelistN(&G.edbo);
	
	/* copy  */
	for(ebo= lb->first; ebo; ebo= ebo->next) {
		newebo= MEM_dupallocN(ebo);
		ebo->temp= newebo;
		BLI_addtail(&G.edbo, newebo);
	}
	
	/* set pointers */
	for(newebo= G.edbo.first; newebo; newebo= newebo->next) {
		if(newebo->parent) newebo->parent= newebo->parent->temp;
	}
	/* be sure they dont hang ever */
	for(newebo= G.edbo.first; newebo; newebo= newebo->next) {
		newebo->temp= NULL;
	}
}

static void *editBones_to_undoBones(void)
{
	ListBase *lb;
	EditBone *ebo, *newebo;
	
	lb= MEM_callocN(sizeof(ListBase), "listbase undo");
	
	/* copy */
	for(ebo= G.edbo.first; ebo; ebo= ebo->next) {
		newebo= MEM_dupallocN(ebo);
		ebo->temp= newebo;
		BLI_addtail(lb, newebo);
	}
	
	/* set pointers */
	for(newebo= lb->first; newebo; newebo= newebo->next) {
		if(newebo->parent) newebo->parent= newebo->parent->temp;
	}
	
	return lb;
}

static void free_undoBones(void *lbv)
{
	ListBase *lb= lbv;
	
	BLI_freelistN(lb);
	MEM_freeN(lb);
}

/* and this is all the undo system needs to know */
void undo_push_armature(char *name)
{
	undo_editmode_push(name, free_undoBones, undoBones_to_editBones, editBones_to_undoBones, NULL);
}



/* **************** END EditMode stuff ********************** */
/* *************** Adding stuff in editmode *************** */

/* default bone add, returns it selected, but without tail set */
static EditBone *add_editbone(char *name)
{
	bArmature *arm= G.obedit->data;
	
	EditBone *bone= MEM_callocN(sizeof(EditBone), "eBone");
	
	BLI_strncpy(bone->name, name, 32);
	unique_editbone_name(&G.edbo, bone->name);
	
	BLI_addtail(&G.edbo, bone);
	
	bone->flag |= BONE_TIPSEL;
	bone->weight= 1.0F;
	bone->dist= 0.25F;
	bone->xwidth= 0.1;
	bone->zwidth= 0.1;
	bone->ease1= 1.0;
	bone->ease2= 1.0;
	bone->rad_head= 0.10;
	bone->rad_tail= 0.05;
	bone->segments= 1;
	bone->layer= arm->layer;
	
	return bone;
}

static void add_primitive_bone(Object *ob, short newob)
{
	float		obmat[3][3], curs[3], viewmat[3][3], totmat[3][3], imat[3][3];
	EditBone	*bone;
	
	VECCOPY(curs, give_cursor());	

	/* Get inverse point for head and orientation for tail */
	Mat4Invert(G.obedit->imat, G.obedit->obmat);
	Mat4MulVecfl(G.obedit->imat, curs);

	if ( !(newob) || (U.flag & USER_ADD_VIEWALIGNED) ) Mat3CpyMat4(obmat, G.vd->viewmat);
	else Mat3One(obmat);
	
	Mat3CpyMat4(viewmat, G.obedit->obmat);
	Mat3MulMat3(totmat, obmat, viewmat);
	Mat3Inv(imat, totmat);
	
	deselectall_armature(0, 0);
	
	/*	Create a bone	*/
	bone= add_editbone("Bone");

	VECCOPY(bone->head, curs);
	
	if ( !(newob) || (U.flag & USER_ADD_VIEWALIGNED) )
		VecAddf(bone->tail, bone->head, imat[1]);	// bone with unit length 1
	else
		VecAddf(bone->tail, bone->head, imat[2]);	// bone with unit length 1, pointing up Z
	
}

void add_primitiveArmature(int type)
{
	short newob=0;
	
	if(G.scene->id.lib) return;
	
	/* this function also comes from an info window */
	if ELEM(curarea->spacetype, SPACE_VIEW3D, SPACE_INFO); else return;
	if (G.vd==NULL) return;
	
	G.f &= ~(G_VERTEXPAINT+G_TEXTUREPAINT+G_WEIGHTPAINT+G_SCULPTMODE);
	setcursor_space(SPACE_VIEW3D, CURSOR_STD);

	check_editmode(OB_ARMATURE);
	
	/* If we're not the "obedit", make a new object and enter editmode */
	if (G.obedit==NULL) {
		add_object(OB_ARMATURE);
		base_init_from_view3d(BASACT, G.vd);
		G.obedit= BASACT->object;
		
		where_is_object(G.obedit);
		
		make_editArmature();
		setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
		newob=1;
	}
	
	/* no primitive support yet */
	add_primitive_bone(G.obedit, newob);
	
	countall(); // flushes selection!

	if ((newob) && !(U.flag & USER_ADD_EDITMODE)) {
		exit_editmode(2);
	}
	
	allqueue(REDRAWALL, 0);
	BIF_undo_push("Add primitive");
}

/* the ctrl-click method */
void addvert_armature(void)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebone, *newbone, *flipbone;
	float *curs, mat[3][3],imat[3][3];
	int a, to_root= 0;
	
	TEST_EDITARMATURE;
	
	/* find the active or selected bone */
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
			if (ebone->flag & (BONE_ACTIVE|BONE_TIPSEL)) 
				break;
		}
	}
	
	if (ebone==NULL) {
		for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
			if (arm->layer & ebone->layer) {
				if (ebone->flag & (BONE_ACTIVE|BONE_ROOTSEL)) 
					break;
			}
		}
		if (ebone == NULL) 
			return;
		
		to_root= 1;
	}
	
	deselectall_armature(0, 0);
	
	/* we re-use code for mirror editing... */
	flipbone= NULL;
	if (arm->flag & ARM_MIRROR_EDIT)
		flipbone= armature_bone_get_mirrored(ebone);

	for (a=0; a<2; a++) {
		if (a==1) {
			if (flipbone==NULL)
				break;
			else {
				SWAP(EditBone *, flipbone, ebone);
			}
		}
		
		newbone= add_editbone(ebone->name);
		newbone->flag |= BONE_ACTIVE;
		
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
		
		curs= give_cursor();
		VECCOPY(newbone->tail, curs);
		VecSubf(newbone->tail, newbone->tail, G.obedit->obmat[3]);
		
		if (a==1) 
			newbone->tail[0]= -newbone->tail[0];
		
		Mat3CpyMat4(mat, G.obedit->obmat);
		Mat3Inv(imat, mat);
		Mat3MulVecfl(imat, newbone->tail);
		
		newbone->length= VecLenf(newbone->head, newbone->tail);
		newbone->rad_tail= newbone->length*0.05f;
		newbone->dist= newbone->length*0.25f;
		
	}
	
	countall();
	
	BIF_undo_push("Add Bone");
	allqueue(REDRAWVIEW3D, 0);
}

/* adds an EditBone between the nominated locations (should be in the right space) */
static EditBone *add_points_bone (float head[], float tail[]) 
{
	EditBone *ebo;
	
	ebo= add_editbone("Bone");
	
	VECCOPY(ebo->head, head);
	VECCOPY(ebo->tail, tail);
	
	return ebo;
}


static EditBone *get_named_editbone(char *name)
{
	EditBone  *eBone;

	if (name)
		for (eBone=G.edbo.first; eBone; eBone=eBone->next) {
			if (!strcmp(name, eBone->name))
				return eBone;
		}

	return NULL;
}

static void update_dup_subtarget(EditBone *dupBone)
{
	/* If an edit bone has been duplicated, lets
	 * update it's constraints if the subtarget
	 * they point to has also been duplicated
	 */
	EditBone     *oldtarget, *newtarget;
	bPoseChannel *chan;
	bConstraint  *curcon;
	ListBase     *conlist;
	
	if ( (chan = verify_pose_channel(OBACT->pose, dupBone->name)) ) {
		if ( (conlist = &chan->constraints) ) {
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
						if ((ct->tar == G.obedit) && (ct->subtarget[0])) {
							oldtarget = get_named_editbone(ct->subtarget);
							if (oldtarget) {
								/* was the subtarget bone duplicated too? If
								 * so, update the constraint to point at the 
								 * duplicate of the old subtarget.
								 */
								if (oldtarget->flag & BONE_SELECTED){
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


void adduplicate_armature(void)
{
	bArmature *arm= G.obedit->data;
	EditBone	*eBone = NULL;
	EditBone	*curBone;
	EditBone	*firstDup=NULL;	/*	The beginning of the duplicated bones in the edbo list */
	
	countall(); // flushes selection!

	/* Select mirrored bones */
	if (arm->flag & ARM_MIRROR_EDIT) {
		for (curBone=G.edbo.first; curBone; curBone=curBone->next) {
			if (arm->layer & curBone->layer) {
				if (curBone->flag & BONE_SELECTED) {
					eBone = armature_bone_get_mirrored(curBone);
					if (eBone)
						eBone->flag |= BONE_SELECTED;
				}
			}
		}
	}
	
	/*	Find the selected bones and duplicate them as needed */
	for (curBone=G.edbo.first; curBone && curBone!=firstDup; curBone=curBone->next) {
		if (arm->layer & curBone->layer) {
			if (curBone->flag & BONE_SELECTED) {
				eBone=MEM_callocN(sizeof(EditBone), "addup_editbone");
				eBone->flag |= BONE_SELECTED;
				
				/*	Copy data from old bone to new bone */
				memcpy (eBone, curBone, sizeof(EditBone));
				
				curBone->temp = eBone;
				eBone->temp = curBone;
				
				unique_editbone_name(&G.edbo, eBone->name);
				BLI_addtail(&G.edbo, eBone);
				if (!firstDup)
					firstDup=eBone;
				
				/* Lets duplicate the list of constraints that the
				 * current bone has.
				 */
				if (OBACT->pose) {
					bPoseChannel *chanold, *channew;
					ListBase     *listold, *listnew;
					
					chanold = verify_pose_channel (OBACT->pose, curBone->name);
					if (chanold) {
						listold = &chanold->constraints;
						if (listold) {
							/* WARNING: this creates a new posechannel, but there will not be an attached bone 
							 *		yet as the new bones created here are still 'EditBones' not 'Bones'. 
							 */
							channew = 
								verify_pose_channel(OBACT->pose, eBone->name);
							if (channew) {
								/* copy transform locks */
								channew->protectflag = chanold->protectflag;
								
								/* ik (dof) settings */
								channew->ikflag = chanold->ikflag;
								VECCOPY(channew->limitmin, chanold->limitmin);
								VECCOPY(channew->limitmax, chanold->limitmax);
								VECCOPY(channew->stiffness, chanold->stiffness);
								channew->ikstretch= chanold->ikstretch;
								
								/* constraints */
								listnew = &channew->constraints;
								copy_constraints (listnew, listold);
							}
						}
					}
				}
			}
		}
	}

	/*	Run though the list and fix the pointers */
	for (curBone=G.edbo.first; curBone && curBone!=firstDup; curBone=curBone->next) {
		if (arm->layer & curBone->layer) {
			if (curBone->flag & BONE_SELECTED) {
				eBone=(EditBone*) curBone->temp;
				
				/*	If this bone has no parent,
				Set the duplicate->parent to NULL
				*/
				if (!curBone->parent)
					eBone->parent = NULL;
				/*	If this bone has a parent that IS selected,
					Set the duplicate->parent to the curBone->parent->duplicate
					*/
				else if (curBone->parent->flag & BONE_SELECTED)
					eBone->parent= (EditBone *)curBone->parent->temp;
				/*	If this bone has a parent that IS not selected,
					Set the duplicate->parent to the curBone->parent
					*/
				else {
					eBone->parent=(EditBone*) curBone->parent; 
					eBone->flag &= ~BONE_CONNECTED;
				}
				
				/* Lets try to fix any constraint subtargets that might
					have been duplicated */
				update_dup_subtarget(eBone);
			}
		}
	} 
	
	/*	Deselect the old bones and select the new ones */
	
	for (curBone=G.edbo.first; curBone && curBone!=firstDup; curBone=curBone->next) {
		if (arm->layer & curBone->layer)
			curBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL | BONE_ACTIVE);
	}
	
	BIF_TransformSetUndo("Add Duplicate");
	initTransform(TFM_TRANSLATION, CTX_NO_PET);
	Transform();
	
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
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
static void chains_find_tips (ListBase *list)
{
	EditBone *curBone, *ebo;
	LinkData *ld;
	
	/* note: this is potentially very slow ... there's got to be a better way */
	for (curBone= G.edbo.first; curBone; curBone= curBone->next) {
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
		if (VecEqual(ebp->vec, vec)) {			
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
void fill_bones_armature(void)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebo, *newbone=NULL;
	ListBase points = {NULL, NULL};
	int count;
	
	/* loop over all bones, and only consider if visible */
	for (ebo= G.edbo.first; ebo; ebo= ebo->next) {
		if ((arm->layer & ebo->layer) && !(ebo->flag & BONE_HIDDEN_A)) {
			if (!(ebo->flag & BONE_CONNECTED) && (ebo->flag & BONE_ROOTSEL))
				fill_add_joint(ebo, 0, &points);
			if (ebo->flag & BONE_TIPSEL) 
				fill_add_joint(ebo, 1, &points);
		}
	}
	
	/* the number of joints determines how we fill:
	 *	1) between joint and cursor (joint=head, cursor=tail)
	 *	2) between the two joints (order is dependent on active-bone/hierachy)
	 * 	3+) error (a smarter method involving finding chains needs to be worked out
	 */
	count= BLI_countlist(&points);
	
	if (count == 0) {
		error("No joints selected");
		return;
	}
	else if (count == 1) {
		EditBonePoint *ebp;
		float curs[3];
		
		/* Get Points - selected joint */
		ebp= (EditBonePoint *)points.first;
		
		/* Get points - cursor (tail) */
		VECCOPY (curs, give_cursor());	
		
		Mat4Invert(G.obedit->imat, G.obedit->obmat);
		Mat4MulVecfl(G.obedit->imat, curs);
		
		/* Create a bone */
		newbone= add_points_bone(ebp->vec, curs);
	}
	else if (count == 2) {
		EditBonePoint *ebp, *ebp2;
		float head[3], tail[3];
		
		/* check that the points don't belong to the same bone */
		ebp= (EditBonePoint *)points.first;
		ebp2= ebp->next;
		
		if ((ebp->head_owner==ebp2->tail_owner) && (ebp->head_owner!=NULL)) {
			error("Same bone selected...");
			BLI_freelistN(&points);
			return;
		}
		if ((ebp->tail_owner==ebp2->head_owner) && (ebp->tail_owner!=NULL)) {
			error("Same bone selected...");
			BLI_freelistN(&points);
			return;
		}
		
		/* find which one should be the 'head' */
		if ((ebp->head_owner && ebp2->head_owner) || (ebp->tail_owner && ebp2->tail_owner)) {
			/* rule: whichever one is closer to 3d-cursor */
			float curs[3];
			float vecA[3], vecB[3];
			float distA, distB;
			
			/* get cursor location */
			VECCOPY (curs, give_cursor());	
			
			Mat4Invert(G.obedit->imat, G.obedit->obmat);
			Mat4MulVecfl(G.obedit->imat, curs);
			
			/* get distances */
			VecSubf(vecA, ebp->vec, curs);
			VecSubf(vecB, ebp2->vec, curs);
			distA= VecLength(vecA);
			distB= VecLength(vecB);
			
			/* compare distances - closer one therefore acts as direction for bone to go */
			if (distA < distB) {
				VECCOPY(head, ebp2->vec);
				VECCOPY(tail, ebp->vec);
			}
			else {
				VECCOPY(head, ebp->vec);
				VECCOPY(tail, ebp2->vec);
			}
		}
		else if (ebp->head_owner) {
			VECCOPY(head, ebp->vec);
			VECCOPY(tail, ebp2->vec);
		}
		else if (ebp2->head_owner) {
			VECCOPY(head, ebp2->vec);
			VECCOPY(tail, ebp->vec);
		}
		
		/* add new bone */
		newbone= add_points_bone(head, tail);
	}
	else {
		// FIXME.. figure out a method for multiple bones
		error("Too many points selected"); 
		printf("Points selected: %d \n", count);
		BLI_freelistN(&points);
		return;
	}
	
	/* free points */
	BLI_freelistN(&points);
	
	/* undo + updates */
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Fill Bones");
}

/* this function merges between two bones, removes them and those in-between, 
 * and adjusts the parent relationships for those in-between
 */
static void bones_merge(EditBone *start, EditBone *end, EditBone *endchild, ListBase *chains)
{
	EditBone *ebo, *ebone, *newbone;
	LinkData *chain;
	float head[3], tail[3];
	
	/* check if same bone */
	if (start == end) {
		printf("Error: same bone! \n");
		printf("\tstart = %s, end = %s \n", start->name, end->name);
	}
	
	/* step 1: add a new bone
	 *	- head = head/tail of start (default head)
	 *	- tail = head/tail of end (default tail)
	 *	- parent = parent of start
	 */
	if ((start->flag & BONE_TIPSEL) && !(start->flag & (BONE_SELECTED|BONE_ACTIVE))) {
		VECCOPY(head, start->tail);
	}
	else {
		VECCOPY(head, start->head);
	}
	if ((end->flag & BONE_ROOTSEL) && !(end->flag & (BONE_SELECTED|BONE_ACTIVE))) {
		VECCOPY(tail, end->head);
	}
	else {
		VECCOPY(tail, end->tail);
	}
	newbone= add_points_bone(head, tail);
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
		BLI_freelinkN(&G.edbo, ebo);
	}
}

/* bone merging - has a menu! */
void merge_armature(void)
{
	bArmature *arm= G.obedit->data;
	short val= 0;
	
	/* process a menu to determine how to merge */
	// TODO: there's room for more modes of merging stuff...
	val= pupmenu("Merge Selected Bones%t|Within Chains%x1");
	if (val <= 0) return;
	
	if (val == 1) {
		/* go down chains, merging bones */
		ListBase chains = {NULL, NULL};
		LinkData *chain, *nchain;
		EditBone *ebo;
		
		/* get chains (ends on chains) */
		chains_find_tips(&chains);
		if (chains.first == NULL) return;
		
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
				if ( (arm->layer & ebo->layer) && !(ebo->flag & BONE_HIDDEN_A) &&
					 ((ebo->flag & BONE_CONNECTED) || (ebo->parent==NULL)) &&
					 (ebo->flag & (BONE_SELECTED|BONE_ACTIVE)) )
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
						bones_merge(bstart, bend, bchild, &chains);
					
					bstart = NULL;
					bend = NULL;
					bchild = NULL;
				}
			}
			
			/* merge from bstart to bend if something not merged */
			if (bstart && bend)
				bones_merge(bstart, bend, bchild, &chains);
			
			/* put back link */
			BLI_insertlinkbefore(&chains, nchain, chain);
		}		
		
		BLI_freelistN(&chains);
	}
	
	/* undo + updates */
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Merge Bones");
}

/* ************** END Add/Remove stuff in editmode ************ */
/* *************** Tools in editmode *********** */


void hide_selected_armature_bones(void)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebone;
	
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
			if (ebone->flag & (BONE_SELECTED)) {
				ebone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL|BONE_ACTIVE);
				ebone->flag |= BONE_HIDDEN_A;
			}
		}
	}
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Hide Bones");
}

void hide_unselected_armature_bones(void)
{
	EditBone *ebone;
	
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		bArmature *arm= G.obedit->data;
		if (arm->layer & ebone->layer) {
			if (ebone->flag & (BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL));
			else {
				ebone->flag &= ~BONE_ACTIVE;
				ebone->flag |= BONE_HIDDEN_A;
			}
		}
	}
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Hide Unselected Bones");
}

void show_all_armature_bones(void)
{
	EditBone *ebone;
	
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		bArmature *arm= G.obedit->data;
		if(arm->layer & ebone->layer) {
			if (ebone->flag & BONE_HIDDEN_A) {
				ebone->flag |= (BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL);
				ebone->flag &= ~BONE_HIDDEN_A;
			}
		}
	}
	countall();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	BIF_undo_push("Reveal Bones");
}

/* check for null, before calling! */
static void bone_connect_to_existing_parent(EditBone *bone)
{
	bone->flag |= BONE_CONNECTED;
	VECCOPY(bone->head, bone->parent->tail);
	bone->rad_head = bone->parent->rad_tail;
}

static void bone_connect_to_new_parent(EditBone *selbone, EditBone *actbone, short mode)
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
	
	if (mode == 1) {	
		/* Connected: Child bones will be moved to the parent tip */
		selbone->flag |= BONE_CONNECTED;
		VecSubf(offset, actbone->tail, selbone->head);
		
		VECCOPY(selbone->head, actbone->tail);
		selbone->rad_head= actbone->rad_tail;
		
		VecAddf(selbone->tail, selbone->tail, offset);
		
		/* offset for all its children */
		for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
			EditBone *par;
			
			for (par= ebone->parent; par; par= par->parent) {
				if (par==selbone) {
					VecAddf(ebone->head, ebone->head, offset);
					VecAddf(ebone->tail, ebone->tail, offset);
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

void make_bone_parent(void)
{
	bArmature *arm= G.obedit->data;
	EditBone *actbone, *ebone, *selbone;
	EditBone *flipbone, *flippar;
	short allchildbones= 0, foundselbone= 0;
	short val;
	
	/* find active bone to parent to */
	for (actbone = G.edbo.first; actbone; actbone=actbone->next) {
		if (arm->layer & actbone->layer) {
			if (actbone->flag & BONE_ACTIVE)
				break;
		}
	}
	if (actbone == NULL) {
		error("Needs an active bone");
		return; 
	}

	/* find selected bones */
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
			if ((ebone->flag & BONE_SELECTED) && (ebone != actbone)) {
				foundselbone++;
				if (ebone->parent != actbone) allchildbones= 1; 
			}	
		}
	}
	/* abort if no selected bones, and active bone doesn't have a parent to work with instead */
	if (foundselbone==0 && actbone->parent==NULL) {
		error("Need selected bone(s)");
		return;
	}
	
	/* 'Keep Offset' option is only displayed if it's likely to be useful */
	if (allchildbones)
		val= pupmenu("Make Parent%t|Connected%x1|Keep Offset%x2");
	else
		val= pupmenu("Make Parent%t|Connected%x1");
	
	if (val < 1) return;

	if (foundselbone==0 && actbone->parent) {
		/* When only the active bone is selected, and it has a parent,
		 * connect it to the parent, as that is the only possible outcome. 
		 */
		bone_connect_to_existing_parent(actbone);
		
		if (arm->flag & ARM_MIRROR_EDIT) {
			flipbone = armature_bone_get_mirrored(actbone);
			if (flipbone)
				bone_connect_to_existing_parent(flipbone);
		}
	}
	else {
		/* loop through all editbones, parenting all selected bones to the active bone */
		for (selbone = G.edbo.first; selbone; selbone=selbone->next) {
			if (arm->layer & selbone->layer) {
				if ((selbone->flag & BONE_SELECTED) && (selbone!=actbone)) {
					/* parent selbone to actbone */
					bone_connect_to_new_parent(selbone, actbone, val);
					
					if (arm->flag & ARM_MIRROR_EDIT) {
						/* - if there's a mirrored copy of selbone, try to find a mirrored copy of actbone 
						 *	(i.e.  selbone="child.L" and actbone="parent.L", find "child.R" and "parent.R").
						 *	This is useful for arm-chains, for example parenting lower arm to upper arm
						 * - if there's no mirrored copy of actbone (i.e. actbone = "parent.C" or "parent")
						 *	then just use actbone. Useful when doing upper arm to spine.
						 */
						flipbone = armature_bone_get_mirrored(selbone);
						flippar = armature_bone_get_mirrored(actbone);
						
						if (flipbone) {
							if (flippar)
								bone_connect_to_new_parent(flipbone, flippar, val);
							else
								bone_connect_to_new_parent(flipbone, actbone, val);
						}
					}
				}
			}
		}
	}

	countall(); /* checks selection */
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWOOPS, 0);
	BIF_undo_push("Make Parent");

	return;
}

static void editbone_clear_parent(EditBone *ebone, int mode)
{
	if (ebone->parent) {
		/* for nice selection */
		ebone->parent->flag &= ~(BONE_TIPSEL);
	}
	
	if (mode==1) ebone->parent= NULL;
	ebone->flag &= ~BONE_CONNECTED;
}

void clear_bone_parent(void)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebone;
	EditBone *flipbone = NULL;
	short val;
	
	val= pupmenu("Clear Parent%t|Clear Parent%x1|Disconnect Bone%x2");
	if (val<1) return;
	
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
			if (ebone->flag & BONE_SELECTED) {
				if (arm->flag & ARM_MIRROR_EDIT)
					flipbone = armature_bone_get_mirrored(ebone);
					
				if (flipbone)
					editbone_clear_parent(flipbone, val);
				editbone_clear_parent(ebone, val);
			}
		}
	}
	
	countall(); // checks selection
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWOOPS, 0);
	BIF_undo_push("Clear Parent");
}
	

static EditBone *editbone_name_exists (ListBase *ebones, char *name)
{
	EditBone	*eBone;
	
	if (ebones == NULL) ebones = &G.edbo;
	
	for (eBone=ebones->first; eBone; eBone=eBone->next) {
		if (!strcmp(name, eBone->name))
			return eBone;
	}
	return NULL;
}

/* note: there's a unique_bone_name() too! */
void unique_editbone_name (ListBase *ebones, char *name)
{
	char		tempname[64];
	int			number;
	char		*dot;
	
	if (editbone_name_exists(ebones, name)) {
		/*	Strip off the suffix, if it's a number */
		number= strlen(name);
		if (number && isdigit(name[number-1])) {
			dot= strrchr(name, '.');	// last occurrance
			if (dot)
				*dot=0;
		}
		
		for (number = 1; number <=999; number++) {
			sprintf (tempname, "%s.%03d", name, number);
			if (!editbone_name_exists(ebones, tempname)) {
				BLI_strncpy(name, tempname, 32);
				return;
			}
		}
	}
}

/* context; editmode armature */
/* if forked && mirror-edit: makes two bones with flipped names */
void extrude_armature(int forked)
{
	bArmature *arm= G.obedit->data;
	EditBone *newbone, *ebone, *flipbone, *first=NULL;
	int a, totbone= 0, do_extrude;
	
	TEST_EDITARMATURE;
	
	/* since we allow root extrude too, we have to make sure selection is OK */
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
			if (ebone->flag & BONE_ROOTSEL) {
				if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
					if (ebone->parent->flag & BONE_TIPSEL)
						ebone->flag &= ~BONE_ROOTSEL;
				}
			}
		}
	}
	
	/* Duplicate the necessary bones */
	for (ebone = G.edbo.first; ((ebone) && (ebone!=first)); ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
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
				if(arm->flag & ARM_MIRROR_EDIT) {
					flipbone= armature_bone_get_mirrored(ebone);
					if (flipbone) {
						forked= 0;	// we extrude 2 different bones
						if (flipbone->flag & (BONE_TIPSEL|BONE_ROOTSEL|BONE_SELECTED))
							/* don't want this bone to be selected... */
							flipbone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL|BONE_ACTIVE);
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
						
						if (newbone->parent && ebone->flag & BONE_CONNECTED) {
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
						if(strlen(newbone->name)<30) {
							if(a==0) strcat(newbone->name, "_L");
							else strcat(newbone->name, "_R");
						}
					}
					unique_editbone_name(&G.edbo, newbone->name);
					
					/* Add the new bone to the list */
					BLI_addtail(&G.edbo, newbone);
					if (!first)
						first = newbone;
					
					/* restore ebone if we were flipping */
					if (a==1 && flipbone) 
						SWAP(EditBone *, flipbone, ebone);
				}
			}
			
			/* Deselect the old bone */
			ebone->flag &= ~(BONE_TIPSEL|BONE_SELECTED|BONE_ROOTSEL|BONE_ACTIVE);
		}		
	}
	/* if only one bone, make this one active */
	if (totbone==1 && first) first->flag |= BONE_ACTIVE;
	
	/* Transform the endpoints */
	countall(); // flushes selection!
	BIF_TransformSetUndo("Extrude");
	initTransform(TFM_TRANSLATION, CTX_NO_PET);
	Transform();
	
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
}

/* context; editmode armature */
void subdivide_armature(int numcuts)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebone, *newbone, *tbone, *mbone;
	int a, i;
	
	if (numcuts < 1) return;

	for (mbone = G.edbo.last; mbone; mbone= mbone->prev) {
		if (arm->layer & mbone->layer) {
			if (mbone->flag & BONE_SELECTED) {
				for (i=numcuts+1; i>1; i--) {
					/* compute cut ratio first */
					float cutratio= 1/(float)i;
					float cutratioI= 1-cutratio;
					
					/* take care of mirrored stuff */
					for (a=0; a<2; a++) {
						float val1[3];
						float val2[3];
						float val3[3];
						
						/* try to find mirrored bone on a != 0 */
						if (a) {
							if (arm->flag & ARM_MIRROR_EDIT)
								ebone= armature_bone_get_mirrored(mbone);
							else 
								ebone= NULL;
						}
						else
							ebone= mbone;
							
						if (ebone) {
							newbone= MEM_mallocN(sizeof(EditBone), "ebone subdiv");
							*newbone = *ebone;
							BLI_addtail(&G.edbo, newbone);
							
							/* calculate location of newbone->head */
							VECCOPY(val1, ebone->head);
							VECCOPY(val2, ebone->tail);
							VECCOPY(val3, newbone->head);
							
							val3[0]= val1[0]*cutratio+val2[0]*cutratioI;
							val3[1]= val1[1]*cutratio+val2[1]*cutratioI;
							val3[2]= val1[2]*cutratio+val2[2]*cutratioI;
							
							VECCOPY(newbone->head, val3);
							VECCOPY(newbone->tail, ebone->tail);
							VECCOPY(ebone->tail, newbone->head);
							
							newbone->rad_head= 0.5*(ebone->rad_head+ebone->rad_tail);
							ebone->rad_tail= newbone->rad_head;
							
							newbone->flag |= BONE_CONNECTED;
							
							unique_editbone_name (&G.edbo, newbone->name);
							
							/* correct parent bones */
							for (tbone = G.edbo.first; tbone; tbone=tbone->next) {
								if (tbone->parent==ebone)
									tbone->parent= newbone;
							}
							newbone->parent= ebone;
						}
					}
				}
			}
		}
	}
	
	if (numcuts==1) BIF_undo_push("Subdivide");
	else BIF_undo_push("Subdivide multi");
}

/* ***************** Pose tools ********************* */

void clear_armature(Object *ob, char mode)
{
	bPoseChannel *pchan;
	bArmature	*arm;

	arm= get_armature(ob);
	if (arm == NULL)
		return;
	
	/* only clear those channels that are not locked */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if (pchan->bone && (pchan->bone->flag & BONE_SELECTED)) {
			if (arm->layer & pchan->bone->layer) {
				switch (mode) {
					case 'r':
						if (pchan->protectflag & (OB_LOCK_ROTX|OB_LOCK_ROTY|OB_LOCK_ROTZ)) {
							float eul[3], oldeul[3], quat1[4];
							
							QUATCOPY(quat1, pchan->quat);
							QuatToEul(pchan->quat, oldeul);
							eul[0]= eul[1]= eul[2]= 0.0f;
							
							if (pchan->protectflag & OB_LOCK_ROTX)
								eul[0]= oldeul[0];
							if (pchan->protectflag & OB_LOCK_ROTY)
								eul[1]= oldeul[1];
							if (pchan->protectflag & OB_LOCK_ROTZ)
								eul[2]= oldeul[2];
							
							EulToQuat(eul, pchan->quat);
							/* quaternions flip w sign to accumulate rotations correctly */
							if ((quat1[0]<0.0f && pchan->quat[0]>0.0f) || (quat1[0]>0.0f && pchan->quat[0]<0.0f)) {
								QuatMulf(pchan->quat, -1.0f);
							}
						}						
						else { 
							pchan->quat[1]=pchan->quat[2]=pchan->quat[3]=0.0F; 
							pchan->quat[0]=1.0F;
						}
						break;
					case 'g':
						if ((pchan->protectflag & OB_LOCK_LOCX)==0)
							pchan->loc[0]= 0.0f;
						if ((pchan->protectflag & OB_LOCK_LOCY)==0)
							pchan->loc[1]= 0.0f;
						if ((pchan->protectflag & OB_LOCK_LOCZ)==0)
							pchan->loc[2]= 0.0f;
						break;
					case 's':
						if ((pchan->protectflag & OB_LOCK_SCALEX)==0)
							pchan->size[0]= 1.0f;
						if ((pchan->protectflag & OB_LOCK_SCALEY)==0)
							pchan->size[1]= 1.0f;
						if ((pchan->protectflag & OB_LOCK_SCALEZ)==0)
							pchan->size[2]= 1.0f;
						break;
						
				}
				
				/* the current values from IPO's may not be zero, so tag as unkeyed */
				pchan->bone->flag |= BONE_UNKEYED;
			}
		}
	}
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	/* no update for this object, this will execute the action again */
	/* is weak... like for ipo editing which uses ctime now... */
	where_is_pose (ob);
	ob->recalc= 0;
}

/* helper for function below */
static int clear_active_flag(Object *ob, Bone *bone, void *data) 
{
	bone->flag &= ~BONE_ACTIVE;
	return 0;
}


/* called from editview.c, for mode-less pose selection */
int do_pose_selectbuffer(Base *base, unsigned int *buffer, short hits)
{
	Object *ob= base->object;
	Bone *nearBone;
	
	if (!ob || !ob->pose) return 0;

	nearBone= get_bone_from_selectbuffer(base, buffer, hits, 1);

	if (nearBone) {
		bArmature *arm= ob->data;
		
		/* since we do unified select, we don't shift+select a bone if the armature object was not active yet */
		if (!(G.qual & LR_SHIFTKEY) || (base != BASACT)) {
			deselectall_posearmature(ob, 0, 0);
			nearBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL|BONE_ACTIVE);
			select_actionchannel_by_name(ob->action, nearBone->name, 1);
		}
		else {
			if (nearBone->flag & BONE_SELECTED) {
				/* if not active, we make it active */
				if((nearBone->flag & BONE_ACTIVE)==0) {
					bone_looper(ob, arm->bonebase.first, NULL, clear_active_flag);
					nearBone->flag |= BONE_ACTIVE;
				}
				else {
					nearBone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL|BONE_ACTIVE);
					select_actionchannel_by_name(ob->action, nearBone->name, 0);
				}
			}
			else {
				bone_looper(ob, arm->bonebase.first, NULL, clear_active_flag);
				
				nearBone->flag |= (BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL|BONE_ACTIVE);
				select_actionchannel_by_name(ob->action, nearBone->name, 1);
			}
		}
		
		/* in weightpaint we select the associated vertex group too */
		if (G.f & G_WEIGHTPAINT) {
			if (nearBone->flag & BONE_ACTIVE) {
				vertexgroup_select_by_name(OBACT, nearBone->name);
				DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
			}
		}
		
		allqueue(REDRAWVIEW3D, 0);
		allqueue(REDRAWACTION, 0);
		allqueue(REDRAWIPO, 0);		/* To force action/constraint ipo update */
		allqueue(REDRAWBUTSEDIT, 0);
		allqueue(REDRAWBUTSOBJECT, 0);
		allqueue(REDRAWOOPS, 0);
	}
	
	return nearBone!=NULL;
}

/* test==0: deselect all
   test==1: swap select
   test==2: only clear active tag 
*/
void deselectall_posearmature (Object *ob, int test, int doundo)
{
	bArmature *arm;
	bPoseChannel *pchan;
	int	selectmode= 0;
	
	/* we call this from outliner too, but with OBACT set OK */
	if (ELEM(NULL, ob, ob->pose)) return;
	arm= get_armature(ob);
	
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
		if ((pchan->bone->layer & arm->layer) && !(pchan->bone->flag & BONE_HIDDEN_P)) {
			if (selectmode==0) pchan->bone->flag &= ~(BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL|BONE_ACTIVE);
			else if (selectmode==1) pchan->bone->flag |= BONE_SELECTED;
			else pchan->bone->flag &= ~BONE_ACTIVE;
		}
	}
	
	/* action editor */
	deselect_actionchannels(ob->action, 0);	/* deselects for sure */
	if (selectmode == 1)
		deselect_actionchannels(ob->action, 1);	/* swaps */
	
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWOOPS, 0);
	allqueue(REDRAWACTION, 0);
	
	countall();
	
	if (doundo) {
		if (selectmode==1) BIF_undo_push("Select All");
		else BIF_undo_push("Deselect All");
	}
}


int bone_looper(Object *ob, Bone *bone, void *data,
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

	if(!(G.f & G_WEIGHTPAINT) || !(bone->flag & BONE_HIDDEN_P)) {
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

static int add_defgroup_unique_bone(Object *ob, Bone *bone, void *data) 
{
    /* This group creates a vertex group to ob that has the
      * same name as bone (provided the bone is skinnable). 
	 * If such a vertex group aleady exist the routine exits.
      */
	if (!(bone->flag & BONE_NO_DEFORM)) {
		if (!get_named_vertexgroup(ob,bone->name)) {
			add_defgroup_name(ob, bone->name);
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

	if (!(G.f & G_WEIGHTPAINT) || !(bone->flag & BONE_HIDDEN_P)) {
	   if (!(bone->flag & BONE_NO_DEFORM)) {
			if (data->heat && data->armob->pose && get_pose_channel(data->armob->pose, bone->name))
				segments = bone->segments;
			else
				segments = 1;
			
			if (!(defgroup = get_named_vertexgroup(ob, bone->name)))
				defgroup = add_defgroup_name(ob, bone->name);
			
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
				add_vert_to_defgroup (ob, dgroup, i, distance, WEIGHT_REPLACE);
			else
				remove_vert_defgroup (ob, dgroup, i);
			
			/* do same for mirror */
			if (dgroupflip && dgroupflip[j] && iflip >= 0) {
				if (distance!=0.0)
					add_vert_to_defgroup (ob, dgroupflip[j], iflip, distance,
						WEIGHT_REPLACE);
				else
					remove_vert_defgroup (ob, dgroupflip[j], iflip);
			}
		}
	}
}

void add_verts_to_dgroups(Object *ob, Object *par, int heat, int mirror)
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

	bArmature *arm;
	Bone **bonelist, *bone;
	bDeformGroup **dgrouplist, **dgroupflip;
	bDeformGroup *dgroup, *curdg;
	bPoseChannel *pchan;
	Mesh *mesh;
	Mat4 *bbone = NULL;
	float (*root)[3], (*tip)[3], (*verts)[3];
	int *selected;
	int numbones, vertsfilled = 0, i, j, segments = 0;
	int wpmode = (G.f & G_WEIGHTPAINT);
	struct { Object *armob; void *list; int heat; } looper_data;

	/* If the parent object is not an armature exit */
	arm = get_armature(par);
	if (!arm)
		return;
	
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
			Mat4MulVecfl(bone->arm_mat, root[j]);
			if ((segments+1) < bone->segments) {
				VECCOPY(tip[j], bbone[segments+1].mat[3])
				Mat4MulVecfl(bone->arm_mat, tip[j]);
			}
			else
				VECCOPY(tip[j], bone->arm_tail)
		}
		else {
			VECCOPY(root[j], bone->arm_head);
			VECCOPY(tip[j], bone->arm_tail);
		}
		
		Mat4MulVecfl(par->obmat, root[j]);
		Mat4MulVecfl(par->obmat, tip[j]);
		
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
		DerivedMesh *dm = mesh_get_derived_final(ob, CD_MASK_BAREMESH);
		
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
		Mat4MulVecfl(ob->obmat, verts[i]);
	}

	/* compute the weights based on gathered vertices and bones */
	if (heat) {
		heat_bone_weighting(ob, mesh, verts, numbones, dgrouplist, dgroupflip,
			root, tip, selected);
	}
	else {
		envelope_bone_weighting(ob, mesh, verts, numbones, bonelist, dgrouplist,
			dgroupflip, root, tip, selected, Mat4ToScalef(par->obmat));
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

void create_vgroups_from_armature(Object *ob, Object *par)
{
	/* Lets try to create some vertex groups 
	 * based on the bones of the parent armature.
	 */

	bArmature *arm;
	short mode;

	/* If the parent object is not an armature exit */
	arm = get_armature(par);
	if (!arm)
		return;

	/* Prompt the user on whether/how they want the vertex groups
	 * added to the child mesh */
    mode= pupmenu("Create Vertex Groups? %t|"
				  "Don't Create Groups %x1|"
				  "Name Groups %x2|"
                  "Create From Envelopes %x3|"
				  "Create From Bone Heat %x4|");
	switch (mode) {
	case 2:
		/* Traverse the bone list, trying to create empty vertex 
		 * groups cooresponding to the bone.
		 */
		bone_looper(ob, arm->bonebase.first, NULL,
					add_defgroup_unique_bone);
		if (ob->type == OB_MESH)
			create_dverts(ob->data);
		
		break;
	
	case 3:
	case 4:
		/* Traverse the bone list, trying to create vertex groups 
		 * that are populated with the vertices for which the
		 * bone is closest.
		 */
		add_verts_to_dgroups(ob, par, (mode == 4), 0);
		break;
	}
} 

static int hide_selected_pose_bone(Object *ob, Bone *bone, void *ptr) 
{
	bArmature *arm= ob->data;
	
	if (arm->layer & bone->layer) {
		if (bone->flag & BONE_SELECTED) {
			bone->flag |= BONE_HIDDEN_P;
			bone->flag &= ~(BONE_SELECTED|BONE_ACTIVE);
		}
	}
	return 0;
}

/* active object is armature */
void hide_selected_pose_bones(void) 
{
	bArmature *arm= OBACT->data;

	if (!arm)
		return;

	bone_looper(OBACT, arm->bonebase.first, NULL, 
				hide_selected_pose_bone);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
	BIF_undo_push("Hide Bones");
}

static int hide_unselected_pose_bone(Object *ob, Bone *bone, void *ptr) 
{
	bArmature *arm= ob->data;
	
	if (arm->layer & bone->layer) {
		// hrm... typo here?
		if (~bone->flag & BONE_SELECTED) {
			bone->flag |= BONE_HIDDEN_P;
			bone->flag &= ~BONE_ACTIVE;
		}
	}
	return 0;
}

/* active object is armature */
void hide_unselected_pose_bones(void) 
{
	bArmature		*arm;

	arm=get_armature(OBACT);

	if (!arm)
		return;

	bone_looper(OBACT, arm->bonebase.first, NULL, 
				hide_unselected_pose_bone);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
	BIF_undo_push("Hide Unselected Bone");
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

/* active object is armature in posemode */
void show_all_pose_bones(void) 
{
	bArmature		*arm;

	arm=get_armature(OBACT);

	if (!arm)
		return;

	bone_looper(OBACT, arm->bonebase.first, NULL, 
				show_pose_bone);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWACTION, 0);
	BIF_undo_push("Reveal Bones");
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
			dot= strrchr(name, '.');	// last occurrance
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
void armature_bone_rename(bArmature *arm, char *oldnamep, char *newnamep)
{
	Object *ob;
	Ipo *ipo;
	char newname[MAXBONENAME];
	char oldname[MAXBONENAME];
	
	/* names better differ! */
	if(strncmp(oldnamep, newnamep, MAXBONENAME)) {
		
		/* we alter newname string... so make copy */
		BLI_strncpy(newname, newnamep, MAXBONENAME);
		/* we use oldname for search... so make copy */
		BLI_strncpy(oldname, oldnamep, MAXBONENAME);
		
		/* now check if we're in editmode, we need to find the unique name */
		if ((G.obedit) && (G.obedit->data==arm)) {
			EditBone	*eBone;
			
			eBone= editbone_name_exists(&G.edbo, oldname);
			if (eBone) {
				unique_editbone_name(&G.edbo, newname);
				BLI_strncpy(eBone->name, newname, MAXBONENAME);
			}
			else return;
		}
		else {
			Bone *bone= get_named_bone(arm, oldname);
			
			if (bone) {
				unique_bone_name (arm, newname);
				BLI_strncpy(bone->name, newname, MAXBONENAME);
			}
			else return;
		}
		
		/* do entire dbase - objects */
		for (ob= G.main->object.first; ob; ob= ob->id.next) {
			/* we have the object using the armature */
			if (arm==ob->data) {
				Object *cob;
				bAction  *act;
				bActionChannel *achan;
				bActionStrip *strip;
				
				/* Rename action channel if necessary */
				act = ob->action;
				if (act && !act->id.lib) {
					/*	Find the appropriate channel */
					achan= get_action_channel(act, oldname);
					if (achan) 
						BLI_strncpy(achan->name, newname, MAXBONENAME);
				}
		
				/* Rename the pose channel, if it exists */
				if (ob->pose) {
					bPoseChannel *pchan = get_pose_channel(ob->pose, oldname);
					if (pchan)
						BLI_strncpy(pchan->name, newname, MAXBONENAME);
				}
				
				/* check all nla-strips too */
				for (strip= ob->nlastrips.first; strip; strip= strip->next) {
					/* Rename action channel if necessary */
					act = strip->act;
					if (act && !act->id.lib) {
						/*	Find the appropriate channel */
						achan= get_action_channel(act, oldname);
						if (achan) 
							BLI_strncpy(achan->name, newname, MAXBONENAME);
					}
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
		}
		
		/* do entire db - ipo's for the drivers */
		for (ipo= G.main->ipo.first; ipo; ipo= ipo->id.next) {
			IpoCurve *icu;
			
			/* check each curve's driver */
			for (icu= ipo->curve.first; icu; icu= icu->next) {
				IpoDriver *icd= icu->driver;
				
				if ((icd) && (icd->ob)) {
					ob= icd->ob;
					
					if (icu->driver->type == IPO_DRIVER_TYPE_NORMAL) {
						if (!strcmp(oldname, icd->name))
							BLI_strncpy(icd->name, newname, MAXBONENAME);
					}
					else {
						/* TODO: pydrivers need to be treated differently */
					}
				}
			}			
		}
	}
}

/* context editmode object */
void armature_flip_names(void)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebone;
	char newname[32];
	
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
			if (ebone->flag & BONE_SELECTED) {
				BLI_strncpy(newname, ebone->name, sizeof(newname));
				bone_flip_name(newname, 1);		// 1 = do strip off number extensions
				armature_bone_rename(G.obedit->data, ebone->name, newname);
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

/* context: edtimode armature */
void armature_autoside_names(short axis)
{
	bArmature *arm= G.obedit->data;
	EditBone *ebone;
	char newname[32];
	
	for (ebone = G.edbo.first; ebone; ebone=ebone->next) {
		if (arm->layer & ebone->layer) {
			if (ebone->flag & BONE_SELECTED) {
				BLI_strncpy(newname, ebone->name, sizeof(newname));
				bone_autoside_name(newname, 1, axis, ebone->head[axis], ebone->tail[axis]);
				armature_bone_rename(G.obedit->data, ebone->name, newname);
			}
		}
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWOOPS, 0);
	BIF_undo_push("Auto-side name");
}

/* context: editmode armature */
EditBone *armature_bone_get_mirrored(EditBone *ebo)
{
	EditBone *eboflip= NULL;
	char name[32];
	
	BLI_strncpy(name, ebo->name, sizeof(name));
	bone_flip_name(name, 0);		// 0 = don't strip off number extensions

	for (eboflip=G.edbo.first; eboflip; eboflip=eboflip->next) {
		if (ebo != eboflip) {
			if (!strcmp (name, eboflip->name)) 
				break;
		}
	}
	
	return eboflip;
}

/* if editbone (partial) selected, copy data */
/* context; editmode armature, with mirror editing enabled */
void transform_armature_mirror_update(void)
{
	EditBone *ebo, *eboflip;
	
	for (ebo=G.edbo.first; ebo; ebo=ebo->next) {
		/* no layer check, correct mirror is more important */
		if (ebo->flag & (BONE_TIPSEL|BONE_ROOTSEL)) {
			eboflip= armature_bone_get_mirrored(ebo);
			
			if (eboflip) {
				/* we assume X-axis flipping for now */
				if (ebo->flag & BONE_TIPSEL) {
					eboflip->tail[0]= -ebo->tail[0];
					eboflip->tail[1]= ebo->tail[1];
					eboflip->tail[2]= ebo->tail[2];
					eboflip->rad_tail= ebo->rad_tail;
				}
				if (ebo->flag & BONE_ROOTSEL) {
					eboflip->head[0]= -ebo->head[0];
					eboflip->head[1]= ebo->head[1];
					eboflip->head[2]= ebo->head[2];
					eboflip->rad_head= ebo->rad_head;
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

/**************************************** SYMMETRY HANDLING ******************************************/

void markdownSymmetryArc(ReebArc *arc, ReebNode *node, int level);

void mirrorAlongAxis(float v[3], float center[3], float axis[3])
{
	float dv[3], pv[3];
	
	VecSubf(dv, v, center);
	Projf(pv, dv, axis);
	VecMulf(pv, -2);
	VecAddf(v, v, pv);
}

/* Helper structure for radial symmetry */
typedef struct RadialArc
{
	ReebArc *arc; 
	float n[3]; /* normalized vector joining the nodes of the arc */
} RadialArc;

void reestablishRadialSymmetry(ReebNode *node, int depth, float axis[3])
{
	RadialArc *ring = NULL;
	RadialArc *unit;
	float limit = G.scene->toolsettings->skgen_symmetry_limit;
	int symmetric = 1;
	int count = 0;
	int i;

	/* count the number of arcs in the symmetry ring */
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		ReebArc *connectedArc = node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->flags == -depth)
		{
			count++;
		}
	}

	ring = MEM_callocN(sizeof(RadialArc) * count, "radial symmetry ring");
	unit = ring;

	/* fill in the ring */
	for (unit = ring, i = 0; node->arcs[i] != NULL; i++)
	{
		ReebArc *connectedArc = node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->flags == -depth)
		{
			ReebNode *otherNode = OTHER_NODE(connectedArc, node);
			float vec[3];

			unit->arc = connectedArc;

			/* project the node to node vector on the symmetry plane */
			VecSubf(unit->n, otherNode->p, node->p);
			Projf(vec, unit->n, axis);
			VecSubf(unit->n, unit->n, vec);

			Normalize(unit->n);

			unit++;
		}
	}

	/* sort ring */
	for (i = 0; i < count - 1; i++)
	{
		float minAngle = 3; /* arbitrary high value, higher than 2, at least */
		int minIndex = -1;
		int j;

		for (j = i + 1; j < count; j++)
		{
			float angle = Inpf(ring[i].n, ring[j].n);

			/* map negative values to 1..2 */
			if (angle < 0)
			{
				angle = 1 - angle;
			}

			if (angle < minAngle)
			{
				minIndex = j;
				minAngle = angle;
			}
		}

		/* swap if needed */
		if (minIndex != i + 1)
		{
			RadialArc tmp;
			tmp = ring[i + 1];
			ring[i + 1] = ring[minIndex];
			ring[minIndex] = tmp;
		}
	}

	for (i = 0; i < count && symmetric; i++)
	{
		ReebNode *node1, *node2;
		float tangent[3];
		float normal[3];
		float p[3];
		int j = (i + 1) % count; /* next arc in the circular list */

		VecAddf(tangent, ring[i].n, ring[j].n);
		Crossf(normal, tangent, axis);
		
		node1 = OTHER_NODE(ring[i].arc, node);
		node2 = OTHER_NODE(ring[j].arc, node);

		VECCOPY(p, node2->p);
		mirrorAlongAxis(p, node->p, normal);
		
		/* check if it's within limit before continuing */
		if (VecLenf(node1->p, p) > limit)
		{
			symmetric = 0;
		}

	}

	if (symmetric)
	{
		/* first pass, merge incrementally */
		for (i = 0; i < count - 1; i++)
		{
			ReebNode *node1, *node2;
			float tangent[3];
			float normal[3];
			int j = i + 1;
	
			VecAddf(tangent, ring[i].n, ring[j].n);
			Crossf(normal, tangent, axis);
			
			node1 = OTHER_NODE(ring[i].arc, node);
			node2 = OTHER_NODE(ring[j].arc, node);
	
			/* mirror first node and mix with the second */
			mirrorAlongAxis(node1->p, node->p, normal);
			VecLerpf(node2->p, node2->p, node1->p, 1.0f / (j + 1));
			
			/* Merge buckets
			 * there shouldn't be any null arcs here, but just to be safe 
			 * */
			if (ring[i].arc->bcount > 0 && ring[j].arc->bcount > 0)
			{
				ReebArcIterator iter1, iter2;
				EmbedBucket *bucket1 = NULL, *bucket2 = NULL;
				
				initArcIterator(&iter1, ring[i].arc, node);
				initArcIterator(&iter2, ring[j].arc, node);
				
				bucket1 = nextBucket(&iter1);
				bucket2 = nextBucket(&iter2);
			
				/* Make sure they both start at the same value */	
				while(bucket1 && bucket1->val < bucket2->val)
				{
					bucket1 = nextBucket(&iter1);
				}
				
				while(bucket2 && bucket2->val < bucket1->val)
				{
					bucket2 = nextBucket(&iter2);
				}
		
		
				for ( ;bucket1 && bucket2; bucket1 = nextBucket(&iter1), bucket2 = nextBucket(&iter2))
				{
					bucket2->nv += bucket1->nv; /* add counts */
					
					/* mirror on axis */
					mirrorAlongAxis(bucket1->p, node->p, normal);
					/* add bucket2 in bucket1 */
					VecLerpf(bucket2->p, bucket2->p, bucket1->p, (float)bucket1->nv / (float)(bucket2->nv));
				}
			}
		}
		
		/* second pass, mirror back on previous arcs */
		for (i = count - 1; i > 0; i--)
		{
			ReebNode *node1, *node2;
			float tangent[3];
			float normal[3];
			int j = i - 1;
	
			VecAddf(tangent, ring[i].n, ring[j].n);
			Crossf(normal, tangent, axis);
			
			node1 = OTHER_NODE(ring[i].arc, node);
			node2 = OTHER_NODE(ring[j].arc, node);
	
			/* copy first node than mirror */
			VECCOPY(node2->p, node1->p);
			mirrorAlongAxis(node2->p, node->p, normal);
			
			/* Copy buckets
			 * there shouldn't be any null arcs here, but just to be safe 
			 * */
			if (ring[i].arc->bcount > 0 && ring[j].arc->bcount > 0)
			{
				ReebArcIterator iter1, iter2;
				EmbedBucket *bucket1 = NULL, *bucket2 = NULL;
				
				initArcIterator(&iter1, ring[i].arc, node);
				initArcIterator(&iter2, ring[j].arc, node);
				
				bucket1 = nextBucket(&iter1);
				bucket2 = nextBucket(&iter2);
			
				/* Make sure they both start at the same value */	
				while(bucket1 && bucket1->val < bucket2->val)
				{
					bucket1 = nextBucket(&iter1);
				}
				
				while(bucket2 && bucket2->val < bucket1->val)
				{
					bucket2 = nextBucket(&iter2);
				}
		
		
				for ( ;bucket1 && bucket2; bucket1 = nextBucket(&iter1), bucket2 = nextBucket(&iter2))
				{
					/* copy and mirror back to bucket2 */			
					bucket2->nv = bucket1->nv;
					VECCOPY(bucket2->p, bucket1->p);
					mirrorAlongAxis(bucket2->p, node->p, normal);
				}
			}
		}
	}

	MEM_freeN(ring);
}

void reestablishAxialSymmetry(ReebNode *node, int depth, float axis[3])
{
	ReebArc *arc1 = NULL;
	ReebArc *arc2 = NULL;
	ReebNode *node1 = NULL, *node2 = NULL;
	float limit = G.scene->toolsettings->skgen_symmetry_limit;
	float nor[3], vec[3], p[3];
	int i;
	
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		ReebArc *connectedArc = node->arcs[i];
		
		/* depth is store as a negative in flag. symmetry level is positive */
		if (connectedArc->flags == -depth)
		{
			if (arc1 == NULL)
			{
				arc1 = connectedArc;
				node1 = OTHER_NODE(arc1, node);
			}
			else
			{
				arc2 = connectedArc;
				node2 = OTHER_NODE(arc2, node);
				break; /* Can stop now, the two arcs have been found */
			}
		}
	}
	
	/* shouldn't happen, but just to be sure */
	if (node1 == NULL || node2 == NULL)
	{
		return;
	}
	
	VecSubf(p, node1->p, node->p);
	Crossf(vec, p, axis);
	Crossf(nor, vec, axis);
	
	/* mirror node2 along axis */
	VECCOPY(p, node2->p);
	mirrorAlongAxis(p, node->p, nor);
	
	/* check if it's within limit before continuing */
	if (VecLenf(node1->p, p) <= limit)
	{
	
		/* average with node1 */
		VecAddf(node1->p, node1->p, p);
		VecMulf(node1->p, 0.5f);
		
		/* mirror back on node2 */
		VECCOPY(node2->p, node1->p);
		mirrorAlongAxis(node2->p, node->p, nor);
		
		/* Merge buckets
		 * there shouldn't be any null arcs here, but just to be safe 
		 * */
		if (arc1->bcount > 0 && arc2->bcount > 0)
		{
			ReebArcIterator iter1, iter2;
			EmbedBucket *bucket1 = NULL, *bucket2 = NULL;
			
			initArcIterator(&iter1, arc1, node);
			initArcIterator(&iter2, arc2, node);
			
			bucket1 = nextBucket(&iter1);
			bucket2 = nextBucket(&iter2);
		
			/* Make sure they both start at the same value */	
			while(bucket1 && bucket1->val < bucket2->val)
			{
				bucket1 = nextBucket(&iter1);
			}
			
			while(bucket2 && bucket2->val < bucket1->val)
			{
				bucket2 = nextBucket(&iter2);
			}
	
	
			for ( ;bucket1 && bucket2; bucket1 = nextBucket(&iter1), bucket2 = nextBucket(&iter2))
			{
				bucket1->nv += bucket2->nv; /* add counts */
				
				/* mirror on axis */
				mirrorAlongAxis(bucket2->p, node->p, nor);
				/* add bucket2 in bucket1 */
				VecLerpf(bucket1->p, bucket1->p, bucket2->p, (float)bucket2->nv / (float)(bucket1->nv));
	
				/* copy and mirror back to bucket2 */			
				bucket2->nv = bucket1->nv;
				VECCOPY(bucket2->p, bucket1->p);
				mirrorAlongAxis(bucket2->p, node->p, nor);
			}
		}
	}
}

void markdownSecondarySymmetry(ReebNode *node, int depth, int level)
{
	float axis[3] = {0, 0, 0};
	int count = 0;
	int i;

	/* Only reestablish spatial symmetry if needed */
	if (G.scene->toolsettings->skgen_options & SKGEN_SYMMETRY)
	{
		/* count the number of branches in this symmetry group
		 * and determinte the axis of symmetry
		 *  */	
		for (i = 0; node->arcs[i] != NULL; i++)
		{
			ReebArc *connectedArc = node->arcs[i];
			
			/* depth is store as a negative in flag. symmetry level is positive */
			if (connectedArc->flags == -depth)
			{
				count++;
			}
			/* If arc is on the axis */
			else if (connectedArc->flags == level)
			{
				VecAddf(axis, axis, connectedArc->v1->p);
				VecSubf(axis, axis, connectedArc->v2->p);
			}
		}
	
		Normalize(axis);
	
		/* Split between axial and radial symmetry */
		if (count == 2)
		{
			reestablishAxialSymmetry(node, depth, axis);
		}
		else
		{
			reestablishRadialSymmetry(node, depth, axis);
		}
	}

	/* markdown secondary symetries */	
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		ReebArc *connectedArc = node->arcs[i];
		
		if (connectedArc->flags == -depth)
		{
			/* markdown symmetry for branches corresponding to the depth */
			markdownSymmetryArc(connectedArc, node, level + 1);
		}
	}
}

void markdownSymmetryArc(ReebArc *arc, ReebNode *node, int level)
{
	int i;
	arc->flags = level;
	
	node = OTHER_NODE(arc, node);
	
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		ReebArc *connectedArc = node->arcs[i];
		
		if (connectedArc != arc)
		{
			ReebNode *connectedNode = OTHER_NODE(connectedArc, node);
			
			/* symmetry level is positive value, negative values is subtree depth */
			connectedArc->flags = -subtreeDepth(connectedNode, connectedArc);
		}
	}

	arc = NULL;

	for (i = 0; node->arcs[i] != NULL; i++)
	{
		int issymmetryAxis = 0;
		ReebArc *connectedArc = node->arcs[i];
		
		/* only arcs not already marked as symetric */
		if (connectedArc->flags < 0)
		{
			int j;
			
			/* true by default */
			issymmetryAxis = 1;
			
			for (j = 0; node->arcs[j] != NULL && issymmetryAxis == 1; j++)
			{
				ReebArc *otherArc = node->arcs[j];
				
				/* different arc, same depth */
				if (otherArc != connectedArc && otherArc->flags == connectedArc->flags)
				{
					/* not on the symmetry axis */
					issymmetryAxis = 0;
				} 
			}
		}
		
		/* arc could be on the symmetry axis */
		if (issymmetryAxis == 1)
		{
			/* no arc as been marked previously, keep this one */
			if (arc == NULL)
			{
				arc = connectedArc;
			}
			else
			{
				/* there can't be more than one symmetry arc */
				arc = NULL;
				break;
			}
		}
	}
	
	/* go down the arc continuing the symmetry axis */
	if (arc)
	{
		markdownSymmetryArc(arc, node, level);
	}

	
	/* secondary symmetry */
	for (i = 0; node->arcs[i] != NULL; i++)
	{
		ReebArc *connectedArc = node->arcs[i];
		
		/* only arcs not already marked as symetric and is not the next arc on the symmetry axis */
		if (connectedArc->flags < 0)
		{
			/* subtree depth is store as a negative value in the flag */
			markdownSecondarySymmetry(node, -connectedArc->flags, level);
		}
	}
}

void markdownSymmetry(ReebGraph *rg)
{
	ReebNode *node;
	ReebArc *arc;
	/* only for Acyclic graphs */
	int cyclic = isGraphCyclic(rg);
	
	/* mark down all arcs as non-symetric */
	for (arc = rg->arcs.first; arc; arc = arc->next)
	{
		arc->flags = 0;
	}
	
	/* mark down all nodes as not on the symmetry axis */
	for (node = rg->nodes.first; node; node = node->next)
	{
		node->flags = 0;
	}

	/* node list is sorted, so lowest node is always the head (by design) */
	node = rg->nodes.first;
	
	/* only work on acyclic graphs and if only one arc is incident on the first node */
	if (cyclic == 0 && countConnectedArcs(rg, node) == 1)
	{
		arc = node->arcs[0];
		
		markdownSymmetryArc(arc, node, 1);

		/* mark down non-symetric arcs */
		for (arc = rg->arcs.first; arc; arc = arc->next)
		{
			if (arc->flags < 0)
			{
				arc->flags = 0;
			}
			else
			{
				/* mark down nodes with the lowest level symmetry axis */
				if (arc->v1->flags == 0 || arc->v1->flags > arc->flags)
				{
					arc->v1->flags = arc->flags;
				}
				if (arc->v2->flags == 0 || arc->v2->flags > arc->flags)
				{
					arc->v2->flags = arc->flags;
				}
			}
		}
	}
}

/**************************************** SUBDIVISION ALGOS ******************************************/

EditBone * subdivideByAngle(ReebArc *arc, ReebNode *head, ReebNode *tail)
{
	EditBone *lastBone = NULL;
	if (G.scene->toolsettings->skgen_options & SKGEN_CUT_ANGLE)
	{
		ReebArcIterator iter;
		EmbedBucket *current = NULL;
		EmbedBucket *previous = NULL;
		EditBone *child = NULL;
		EditBone *parent = NULL;
		EditBone *root = NULL;
		float angleLimit = (float)cos(G.scene->toolsettings->skgen_angle_limit * M_PI / 180.0f);
		
		parent = add_editbone("Bone");
		parent->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
		VECCOPY(parent->head, head->p);
		
		root = parent;
		
		for (initArcIterator(&iter, arc, head), previous = nextBucket(&iter), current = nextBucket(&iter);
			current;
			previous = current, current = nextBucket(&iter))
		{
			float vec1[3], vec2[3];
			float len1, len2;

			VecSubf(vec1, previous->p, parent->head);
			VecSubf(vec2, current->p, previous->p);

			len1 = Normalize(vec1);
			len2 = Normalize(vec2);

			if (len1 > 0.0f && len2 > 0.0f && Inpf(vec1, vec2) < angleLimit)
			{
				VECCOPY(parent->tail, previous->p);

				child = add_editbone("Bone");
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
			delete_bone(parent);
			parent = NULL;
		}
		
		lastBone = parent; /* set last bone in the chain */
	}
	
	return lastBone;
}

float calcCorrelation(ReebArc *arc, int start, int end, float v0[3], float n[3])
{
	int len = 2 + abs(end - start);
	
	if (len > 2)
	{
		ReebArcIterator iter;
		EmbedBucket *bucket = NULL;
		float avg_t = 0.0f;
		float s_t = 0.0f;
		float s_xyz = 0.0f;
		
		/* First pass, calculate average */
		for (initArcIterator2(&iter, arc, start, end), bucket = nextBucket(&iter);
			bucket;
			bucket = nextBucket(&iter))
		{
			float v[3];
			
			VecSubf(v, bucket->p, v0);
			avg_t += Inpf(v, n);
		}
		
		avg_t /= Inpf(n, n);
		avg_t += 1.0f; /* adding start (0) and end (1) values */
		avg_t /= len;
		
		/* Second pass, calculate s_xyz and s_t */
		for (initArcIterator2(&iter, arc, start, end), bucket = nextBucket(&iter);
			bucket;
			bucket = nextBucket(&iter))
		{
			float v[3], d[3];
			float dt;
			
			VecSubf(v, bucket->p, v0);
			Projf(d, v, n);
			VecSubf(v, v, d);
			
			dt = VecLength(d) - avg_t;
			
			s_t += dt * dt;
			s_xyz += Inpf(v, v);
		}
		
		/* adding start(0) and end(1) values to s_t */
		s_t += (avg_t * avg_t) + (1 - avg_t) * (1 - avg_t);
		
		return 1.0f - s_xyz / s_t; 
	}
	else
	{
		return 1.0f;
	}
}

EditBone * subdivideByCorrelation(ReebArc *arc, ReebNode *head, ReebNode *tail)
{
	ReebArcIterator iter;
	float n[3];
	float CORRELATION_THRESHOLD = G.scene->toolsettings->skgen_correlation_limit;
	EditBone *lastBone = NULL;
	
	/* init iterator to get start and end from head */
	initArcIterator(&iter, arc, head);
	
	/* Calculate overall */
	VecSubf(n, arc->buckets[iter.end].p, head->p);
	
	if (G.scene->toolsettings->skgen_options & SKGEN_CUT_CORRELATION && 
		calcCorrelation(arc, iter.start, iter.end, head->p, n) < CORRELATION_THRESHOLD)
	{
		EmbedBucket *bucket = NULL;
		EmbedBucket *previous = NULL;
		EditBone *child = NULL;
		EditBone *parent = NULL;
		int boneStart = iter.start;

		parent = add_editbone("Bone");
		parent->flag = BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
		VECCOPY(parent->head, head->p);
		
		for (previous = nextBucket(&iter), bucket = nextBucket(&iter);
			bucket;
			previous = bucket, bucket = nextBucket(&iter))
		{
			/* Calculate normal */
			VecSubf(n, bucket->p, parent->head);

			if (calcCorrelation(arc, boneStart, iter.index, parent->head, n) < CORRELATION_THRESHOLD)
			{
				VECCOPY(parent->tail, previous->p);

				child = add_editbone("Bone");
				VECCOPY(child->head, parent->tail);
				child->parent = parent;
				child->flag |= BONE_CONNECTED|BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				
				parent = child; // new child is next parent
				boneStart = iter.index; // start from end
			}
		}

		VECCOPY(parent->tail, tail->p);
		
		lastBone = parent; /* set last bone in the chain */
	}
	
	return lastBone;
}

float arcLengthRatio(ReebArc *arc)
{
	float arcLength = 0.0f;
	float embedLength = 0.0f;
	int i;
	
	arcLength = VecLenf(arc->v1->p, arc->v2->p);
	
	if (arc->bcount > 0)
	{
		/* Add the embedding */
		for ( i = 1; i < arc->bcount; i++)
		{
			embedLength += VecLenf(arc->buckets[i - 1].p, arc->buckets[i].p);
		}
		/* Add head and tail -> embedding vectors */
		embedLength += VecLenf(arc->v1->p, arc->buckets[0].p);
		embedLength += VecLenf(arc->v2->p, arc->buckets[arc->bcount - 1].p);
	}
	else
	{
		embedLength = arcLength;
	}
	
	return embedLength / arcLength;	
}

EditBone * subdivideByLength(ReebArc *arc, ReebNode *head, ReebNode *tail)
{
	EditBone *lastBone = NULL;
	if ((G.scene->toolsettings->skgen_options & SKGEN_CUT_LENGTH) &&
		arcLengthRatio(arc) >= G.scene->toolsettings->skgen_length_ratio)
	{
		ReebArcIterator iter;
		EmbedBucket *bucket = NULL;
		EmbedBucket *previous = NULL;
		EditBone *child = NULL;
		EditBone *parent = NULL;
		float lengthLimit = G.scene->toolsettings->skgen_length_limit;
		int same = 0;
		
		parent = add_editbone("Bone");
		parent->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
		VECCOPY(parent->head, head->p);

		initArcIterator(&iter, arc, head);

		bucket = nextBucket(&iter);
		
		while (bucket != NULL)
		{
			float *vec0 = NULL;
			float *vec1 = bucket->p;

			/* first bucket. Previous is head */
			if (previous == NULL)
			{
				vec0 = head->p;
			}
			/* Previous is a valid bucket */
			else
			{
				vec0 = previous->p;
			}
			
			/* If lengthLimit hits the current segment */
			if (VecLenf(vec1, parent->head) > lengthLimit)
			{
				if (same == 0)
				{
					float dv[3], off[3];
					float a, b, c, f;
					
					/* Solve quadratic distance equation */
					VecSubf(dv, vec1, vec0);
					a = Inpf(dv, dv);
					
					VecSubf(off, vec0, parent->head);
					b = 2 * Inpf(dv, off);
					
					c = Inpf(off, off) - (lengthLimit * lengthLimit);
					
					f = (-b + (float)sqrt(b * b - 4 * a * c)) / (2 * a);
					
					//printf("a %f, b %f, c %f, f %f\n", a, b, c, f);
					
					if (isnan(f) == 0 && f < 1.0f)
					{
						VECCOPY(parent->tail, dv);
						VecMulf(parent->tail, f);
						VecAddf(parent->tail, parent->tail, vec0);
					}
					else
					{
						VECCOPY(parent->tail, vec1);
					}
				}
				else
				{
					float dv[3];
					
					VecSubf(dv, vec1, vec0);
					Normalize(dv);
					 
					VECCOPY(parent->tail, dv);
					VecMulf(parent->tail, lengthLimit);
					VecAddf(parent->tail, parent->tail, parent->head);
				}
				
				child = add_editbone("Bone");
				VECCOPY(child->head, parent->tail);
				child->parent = parent;
				child->flag |= BONE_CONNECTED|BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
				
				parent = child; // new child is next parent
				
				same = 1; // mark as same
			}
			else
			{
				previous = bucket;
				bucket = nextBucket(&iter);
				same = 0; // Reset same
			}
		}
		VECCOPY(parent->tail, tail->p);
		
		lastBone = parent; /* set last bone in the chain */
	}
	
	return lastBone;
}

/***************************************** MAIN ALGORITHM ********************************************/

void generateSkeletonFromReebGraph(ReebGraph *rg)
{
	GHash *arcBoneMap = NULL;
	ReebArc *arc = NULL;
	ReebNode *node = NULL;
	Object *src = NULL;
	Object *dst = NULL;
	
	src = BASACT->object;
	
	if (G.obedit != NULL)
	{
		exit_editmode(EM_FREEDATA|EM_FREEUNDO|EM_WAITCURSOR); // freedata, and undo
	}

	setcursor_space(SPACE_VIEW3D, CURSOR_WAIT);
	
	dst = add_object(OB_ARMATURE);
	base_init_from_view3d(BASACT, G.vd);
	G.obedit= BASACT->object;
	
	/* Copy orientation from source */
	VECCOPY(dst->loc, src->obmat[3]);
	Mat4ToEul(src->obmat, dst->rot);
	Mat4ToSize(src->obmat, dst->size);
	
	where_is_object(G.obedit);
	
	make_editArmature();

	arcBoneMap = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	markdownSymmetry(rg);
	
	for (arc = rg->arcs.first; arc; arc = arc->next) 
	{
		EditBone *lastBone = NULL;
		ReebNode *head, *tail;
		int i;

		/* Find out the direction of the arc through simple heuristics (in order of priority) :
		 * 
		 * 1- Arcs on primary symmetry axis (flags == 1) point up (head: high weight -> tail: low weight)
		 * 2- Arcs starting on a primary axis point away from it (head: node on primary axis)
		 * 3- Arcs point down (head: low weight -> tail: high weight)
		 *
		 * Finally, the arc direction is stored in its flags: 1 (low -> high), -1 (high -> low)
		 */

		/* if arc is a symmetry axis, internal bones go up the tree */		
		if (arc->flags == 1 && arc->v2->degree != 1)
		{
			head = arc->v2;
			tail = arc->v1;
			
			arc->flags = -1; /* mark arc direction */
		}
		/* Bones point AWAY from the symmetry axis */
		else if (arc->v1->flags == 1)
		{
			head = arc->v1;
			tail = arc->v2;
			
			arc->flags = 1; /* mark arc direction */
		}
		else if (arc->v2->flags == 1)
		{
			head = arc->v2;
			tail = arc->v1;
			
			arc->flags = -1; /* mark arc direction */
		}
		/* otherwise, always go from low weight to high weight */
		else
		{
			head = arc->v1;
			tail = arc->v2;
			
			arc->flags = 1; /* mark arc direction */
		}
		
		/* Loop over subdivision methods */	
		for (i = 0; lastBone == NULL && i < SKGEN_SUB_TOTAL; i++)
		{
			switch(G.scene->toolsettings->skgen_subdivisions[i])
			{
				case SKGEN_SUB_LENGTH:
					lastBone = subdivideByLength(arc, head, tail);
					break;
				case SKGEN_SUB_ANGLE:
					lastBone = subdivideByAngle(arc, head, tail);
					break;
				case SKGEN_SUB_CORRELATION:
					lastBone = subdivideByCorrelation(arc, head, tail);
					break;
			}
		}
	
		if (lastBone == NULL)
		{
			EditBone	*bone;
			bone = add_editbone("Bone");
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

		for (i = 0; node->arcs[i] != NULL; i++)
		{
			arc = node->arcs[i];

			/* if arc is incoming into the node */
			if ((arc->v1 == node && arc->flags == -1) || (arc->v2 == node && arc->flags == 1))
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
			for (i = 0; node->arcs[i] != NULL; i++)
			{
				arc = node->arcs[i];

				/* if arc is outgoing from the node */
				if ((arc->v1 == node && arc->flags == 1) || (arc->v2 == node && arc->flags == -1))
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

	setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
	
	BIF_undo_push("Generate Skeleton");
}

void generateSkeleton(void)
{
	EditMesh *em = G.editMesh;
	ReebGraph *rg = NULL;
	int i;
	
	if (em == NULL)
		return;

	setcursor_space(SPACE_VIEW3D, CURSOR_WAIT);

	if (weightFromDistance(em) == 0)
	{
		error("No selected vertex\n");
		return;
	}
		
	renormalizeWeight(em, 1.0f);
	
	weightToHarmonic(em);
	
#ifdef DEBUG_REEB
	weightToVCol(em);
#endif
	
	rg = generateReebGraph(em, G.scene->toolsettings->skgen_resolution);

	verifyBuckets(rg);
	
	/* Remove arcs without embedding */
	filterNullReebGraph(rg);

	verifyBuckets(rg);


	i = 1;
	/* filter until there's nothing more to do */
	while (i == 1)
	{
		i = 0; /* no work done yet */
		
		if (G.scene->toolsettings->skgen_options & SKGEN_FILTER_EXTERNAL)
		{
			i |= filterExternalReebGraph(rg, G.scene->toolsettings->skgen_threshold_external * G.scene->toolsettings->skgen_resolution);
		}
	
		verifyBuckets(rg);
	
		if (G.scene->toolsettings->skgen_options & SKGEN_FILTER_INTERNAL)
		{
			i |= filterInternalReebGraph(rg, G.scene->toolsettings->skgen_threshold_internal * G.scene->toolsettings->skgen_resolution);
		}
	}

	verifyBuckets(rg);

	repositionNodes(rg);
	
	verifyBuckets(rg);

	/* Filtering might have created degree 2 nodes, so remove them */
	removeNormalNodes(rg);
	
	verifyBuckets(rg);

	for(i = 0; i <  G.scene->toolsettings->skgen_postpro_passes; i++)
	{
		postprocessGraph(rg, G.scene->toolsettings->skgen_postpro);
	}

	buildAdjacencyList(rg);
	
	sortNodes(rg);
	
	sortArcs(rg);
	
	generateSkeletonFromReebGraph(rg);

	freeGraph(rg);
}
