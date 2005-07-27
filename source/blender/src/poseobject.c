/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * support for animation modes - Reevan McKay
 */

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_editarmature.h"
#include "BIF_editaction.h"
#include "BIF_editconstraint.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_poseobject.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_screen.h"

#include "BDR_editobject.h"

#include "BSE_edit.h"

#include "mydevice.h"
#include "blendef.h"

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

	if (G.obedit) exit_editmode(1);
	G.f &= ~(G_VERTEXPAINT | G_FACESELECT | G_TEXTUREPAINT | G_WEIGHTPAINT);
}

void set_pose_keys (Object *ob)
{
	bPoseChannel *chan;

	if (ob->pose){
		for (chan=ob->pose->chanbase.first; chan; chan=chan->next){
			Bone *bone= chan->bone;
			if(bone && (bone->flag & BONE_SELECTED)) {
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

void pose_select_constraint_target(void)
{
	Object *ob= OBACT;
	bPoseChannel *pchan;
	bConstraint *con;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
			
			for(con= pchan->constraints.first; con; con= con->next) {
				char *subtarget;
				Object *target= get_constraint_target(con, &subtarget);
				
				if(ob==target) {
					if(subtarget) {
						bPoseChannel *pchanc= get_pose_channel(ob->pose, subtarget);
						pchanc->bone->flag |= BONE_SELECTED|BONE_TIPSEL|BONE_ROOTSEL;
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
	
	nr= pupmenu("Specials%t|Select Constraint Target%x1|Flip Left-Right Names");
	if(nr==1) {
		pose_select_constraint_target();
	}
	else if(nr==2) {
		pose_flip_names();
	}
}

/* context: active object, active channel, optional selected channel */
void pose_add_IK(void)
{
	Object *ob= OBACT;
	bPoseChannel *pchanact, *pchansel;
	bConstraint *con;
	short nr;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	/* find active */
	for(pchanact= ob->pose->chanbase.first; pchanact; pchanact= pchanact->next)
		if(pchanact->bone->flag & BONE_ACTIVE) break;
	if(pchanact==NULL) return;
	
	/* find selected */
	for(pchansel= ob->pose->chanbase.first; pchansel; pchansel= pchansel->next) {
		if(pchansel!=pchanact)
			if(pchansel->bone->flag & BONE_SELECTED) break;
	}
	
	for(con= pchanact->constraints.first; con; con= con->next) {
		if(con->type==CONSTRAINT_TYPE_KINEMATIC) break;
	}
	if(con) {
		error("Pose Channel already has IK");
		return;
	}
	
	if(pchansel)
		nr= pupmenu("Add IK Constraint%t|To new Empty Object%x1|To selected Bone%x2");
	else
		nr= pupmenu("Add IK Constraint%t|To new Empty Object%x1");

	if(nr<1) return;
	
	/* prevent weird chains... */
	if(nr==2) {
		bPoseChannel *pchan= pchanact;
		while(pchan) {
			if(pchan==pchansel) break;
			if(pchan->bone->flag & BONE_IK_TOPARENT)
				pchan= pchan->parent;
			else pchan= NULL;
		}
		if(pchan) {
			error("IK target should not be in the IK chain itself");
			return;
		}
	}

	con = add_new_constraint(CONSTRAINT_TYPE_KINEMATIC);
	BLI_addtail(&pchanact->constraints, con);
	pchanact->constflag |= PCHAN_HAS_IK;	// for draw, but also for detecting while pose solving
	
	/* add new empty as target */
	if(nr==1) {
		Base *base= BASACT, *newbase;
		Object *obt;
		
		obt= add_object(OB_EMPTY);
		/* set layers OK */
		newbase= BASACT;
		newbase->lay= base->lay;
		obt->lay= newbase->lay;
		
		/* transform cent to global coords for loc */
		VecMat4MulVecfl(obt->loc, ob->obmat, pchanact->pose_tail);
		
		set_constraint_target(con, obt, NULL);
		
		/* restore, add_object sets active */
		BASACT= base;
		base->flag |= SELECT;
	}
	else if(nr==2) {
		set_constraint_target(con, ob, pchansel->name);
	}
	
	ob->pose->flag |= POSE_RECALC;	// sort pose channels
	DAG_scene_sort(G.scene);		// sort order of objects
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);	// and all its relations
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWOOPS, 0);
	
	BIF_undo_push("Add IK constraint");
}

/* context: all selected channels */
void pose_clear_IK(void)
{
	Object *ob= OBACT;
	bPoseChannel *pchan;
	bConstraint *con;
	bConstraint *next;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	if(okee("Remove IK constraint(s)")==0) return;

	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
			
			for(con= pchan->constraints.first; con; con= next) {
				next= con->next;
				if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
					BLI_remlink(&pchan->constraints, con);
					free_constraint_data(con);
					MEM_freeN(con);
				}
			}
			pchan->constflag &= ~PCHAN_HAS_IK;
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
	bPoseChannel *pchan;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	if(okee("Remove Constraints")==0) return;
	
	/* find active */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
			free_constraints(&pchan->constraints);
			pchan->constflag= 0;
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
	bPoseChannel *pchan, *pchanact;
	short nr;
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	/* find active */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone->flag & BONE_ACTIVE) break;
	}
	
	if(pchan==NULL) return;
	pchanact= pchan;
	
	nr= pupmenu("Copy Pose Attributes %t|Location%x1|Rotation%x2|Size%x3|Constraints");
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone->flag & BONE_SELECTED) {
			if(pchan!=pchanact) {
				if(nr==1) {
					VECCOPY(pchan->loc, pchanact->loc);
				}
				else if(nr==2) {
					QUATCOPY(pchan->quat, pchanact->quat);
				}
				else if(nr==3) {
					VECCOPY(pchan->size, pchanact->size);
				}
				else if(nr==4) {
					free_constraints(&pchan->constraints);
					copy_constraints(&pchan->constraints, &pchanact->constraints);
					pchan->constflag = pchanact->constflag;
				}
			}
		}
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

static void flip_name (char *name)
{

	char	prefix[128]={""};	/* The part before the facing */
	char	suffix[128]={""};	/* The part after the facing */
	char	replace[128]={""};	/* The replacement string */

	char	*index=NULL;
	/* Find the last period */

	strcpy (prefix, name);

	/* Check for an instance of .Right */
	if (!index){
		index = strstr (prefix, "Right");
		if (index){
			*index=0;
			strcpy (replace, "Left");
			strcpy (suffix, index+6);
		}
	}

	/* Che ck for an instance of .RIGHT */
	if (!index){
		index = strstr (prefix, "RIGHT");
		if (index){
			*index=0;
			strcpy (replace, "LEFT");
			strcpy (suffix, index+6);
		}
	}


	/* Check for an instance of .right */
	if (!index){
		index = strstr (prefix, "right");
		if (index){
			*index=0;
			strcpy (replace, "left");
			strcpy (suffix, index+6);
		}
	}

	/* Check for an instance of .left */
	if (!index){
		index = strstr (prefix, "left");
		if (index){
			*index=0;
			strcpy (replace, "right");
			strcpy (suffix, index+5);
		}
	}

	/* Check for an instance of .LEFT */
	if (!index){
		index = strstr (prefix, "LEFT");
		if (index){
			*index=0;
			strcpy (replace, "RIGHT");
			strcpy (suffix, index+5);
		}
	}

	/* Check for an instance of .Left */
	if (!index){
		index = strstr (prefix, "Left");
		if (index){
			*index=0;
			strcpy (replace, "Right");
			strcpy (suffix, index+5);
		}
	}

	/* check for an instance of .L */
	if (!index){
		index = strstr (prefix, ".L");
		if (index){
			*index=0;
			strcpy (replace, ".R");
			strcpy (suffix, index+2);
		}
	}

	/* check for an instance of .l */
	if (!index){
		index = strstr (prefix, ".l");
		if (index){
			*index=0;
			strcpy (replace, ".r");
			strcpy (suffix, index+2);
		}
	}
	/* check for an instance of _l */
	if (!index){
		index = strstr (prefix, "_l");
		if (index){
			*index=0;
			strcpy (replace, "_r");
			strcpy (suffix, index+2);
		}
	}
	/* check for an instance of _L */
	if (!index){
		index = strstr (prefix, "_L");
		if (index){
			*index=0;
			strcpy (replace, "_R");
			strcpy (suffix, index+2);
		}
	}
	/* Checl for an instance of .R */
	if (!index){
		index = strstr (prefix, ".R");
		if (index){
			*index=0;
			strcpy (replace, ".L");
			strcpy (suffix, index+2);
		}
	}

	/* Checl for an instance of .r */
	if (!index){
		index = strstr (prefix, ".r");
		if (index){
			*index=0;
			strcpy (replace, ".l");
			strcpy (suffix, index+2);
		}
	}
	/* Checl for an instance of _r */
	if (!index){
		index = strstr (prefix, "_r");
		if (index){
			*index=0;
			strcpy (replace, "_l");
			strcpy (suffix, index+2);
		}
	}	
	/* Check for an instance of _R */
	if (!index){
		index = strstr (prefix, "_R");
		if (index){
			*index=0;
			strcpy (replace, "_L");
			strcpy (suffix, index+2);
		}
	}	
    if(strstr (suffix, "001")){
    	sprintf (name, "%s%s", prefix, replace);        
    } else {
    	sprintf (name, "%s%s%s", prefix, replace, suffix);
    }
}

void paste_posebuf (int flip)
{
	Object *ob= OBACT;
	bPoseChannel *chan, *pchan;
	float eul[4];
	int newchan = 0;
	char name[32];
	
	if (!ob || !ob->pose)
		return;

	if (!g_posebuf){
		error ("Copy buffer is empty");
		return;
	}
	
	/* Safely merge all of the channels in this pose into
	any existing pose */
	for (chan=g_posebuf->chanbase.first; chan; chan=chan->next){
		if (chan->flag & POSE_KEY) {
			BLI_strncpy(name, chan->name, sizeof(name));
			if (flip)
				flip_name (name);
				
			/* only copy when channel exists, poses are not meant to add random channels to anymore */
			pchan= get_pose_channel(ob->pose, name);
			
			if(pchan) {
				/* only loc rot size */
				/* only copies transform info for the pose */
				VECCOPY(pchan->loc, chan->loc);
				VECCOPY(pchan->size, chan->size);
				QUATCOPY(pchan->quat, chan->quat);
				pchan->flag= chan->flag;
				
				if (flip){
					pchan->loc[0]*= -1;

					QuatToEul(pchan->quat, eul);
					eul[1]*= -1;
					eul[2]*= -1;
					EulToQuat(eul, pchan->quat);
				}

				if (G.flags & G_RECORDKEYS){
					/* Set keys on pose */
					if (chan->flag & POSE_ROT){
						set_action_key(ob->action, pchan, AC_QUAT_X, newchan);
						set_action_key(ob->action, pchan, AC_QUAT_Y, newchan);
						set_action_key(ob->action, pchan, AC_QUAT_Z, newchan);
						set_action_key(ob->action, pchan, AC_QUAT_W, newchan);
					}
					if (chan->flag & POSE_SIZE){
						set_action_key(ob->action, pchan, AC_SIZE_X, newchan);
						set_action_key(ob->action, pchan, AC_SIZE_Y, newchan);
						set_action_key(ob->action, pchan, AC_SIZE_Z, newchan);
					}
					if (chan->flag & POSE_LOC){
						set_action_key(ob->action, pchan, AC_LOC_X, newchan);
						set_action_key(ob->action, pchan, AC_LOC_Y, newchan);
						set_action_key(ob->action, pchan, AC_LOC_Z, newchan);
					}
				}
			}
		}
	}

	/* Update event for pose and deformation children */
	ob->pose->ctime= -123456.0f;
	DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
	
	if (G.flags & G_RECORDKEYS) {
		remake_action_ipos(ob->action);
		allqueue (REDRAWIPO, 0);
		allqueue (REDRAWVIEW3D, 0);
		allqueue (REDRAWACTION, 0);		
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

/* context active object */
void pose_flip_names(void)
{
	Object *ob= OBACT;
	bPoseChannel *pchan;
	char newname[32];
	
	/* paranoia checks */
	if(!ob && !ob->pose) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)==0) return;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone->flag & (BONE_ACTIVE|BONE_SELECTED)) {
			BLI_strncpy(newname, pchan->name, sizeof(newname));
			flip_name(newname);
			armature_bone_rename(ob->data, pchan->name, newname);
		}
	}
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWACTION, 0);
	allqueue(REDRAWOOPS, 0);
	BIF_undo_push("Flip names");
	
}
