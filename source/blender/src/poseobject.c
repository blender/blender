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
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

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

/* called by buttons to find a bone to display/edit values for */
bPoseChannel *get_active_posechannel (Object *ob)
{
	bPoseChannel *pchan;
	
	/* find active */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone && (pchan->bone->flag & BONE_ACTIVE))
			return pchan;
	}
	
	return NULL;
}

int pose_channel_in_IK_chain(Object *ob, bPoseChannel *pchan)
{
	bConstraint *con;
	Bone *bone;
	
	for(con= pchan->constraints.first; con; con= con->next) {
		if(con->type==CONSTRAINT_TYPE_KINEMATIC) return 1;
	}
	for(bone= pchan->bone->childbase.first; bone; bone= bone->next) {
		pchan= get_pose_channel(ob->pose, bone->name);
		if(pchan && pose_channel_in_IK_chain(ob, pchan))
			return 1;
	}
	return 0;
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
	
	nr= pupmenu("Specials%t|Select Constraint Target%x1|Flip Left-Right Names%x2");
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
			pchan= pchan->parent;
		}
		if(pchan) {
			error("IK root cannot be linked to IK tip");
			return;
		}
		pchan= pchansel;
		while(pchan) {
			if(pchan==pchanact) break;
			pchan= pchan->parent;
		}
		if(pchan) {
			error("IK tip cannot be linked to IK root");
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
	
	/* active flag */
	con->flag |= CONSTRAINT_ACTIVE;
	for(con= con->prev; con; con= con->prev)
		con->flag &= ~CONSTRAINT_ACTIVE;
	
	
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
	
	/* Safely merge all of the channels in this pose into
	any existing pose */
	for (chan=g_posebuf->chanbase.first; chan; chan=chan->next){
		if (chan->flag & POSE_KEY) {
			BLI_strncpy(name, chan->name, sizeof(name));
			if (flip)
				bone_flip_name (name, 0);		// 0 = don't strip off number extensions
				
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
					ID *id= &ob->id;
					/* Set keys on pose */
					if (chan->flag & POSE_ROT){
						insertkey(id, ID_AC, chan->name, NULL, AC_QUAT_X);
						insertkey(id, ID_AC, chan->name, NULL, AC_QUAT_Y);
						insertkey(id, ID_AC, chan->name, NULL, AC_QUAT_Z);
						insertkey(id, ID_AC, chan->name, NULL, AC_QUAT_W);
					}
					if (chan->flag & POSE_SIZE){
						insertkey(id, ID_AC, chan->name, NULL, AC_SIZE_X);
						insertkey(id, ID_AC, chan->name, NULL, AC_SIZE_Y);
						insertkey(id, ID_AC, chan->name, NULL, AC_SIZE_Z);
					}
					if (chan->flag & POSE_LOC){
						insertkey(id, ID_AC, pchan->name, NULL, AC_LOC_X);
						insertkey(id, ID_AC, pchan->name, NULL, AC_LOC_Y);
						insertkey(id, ID_AC, pchan->name, NULL, AC_LOC_Z);
					}
				}
			}
		}
	}

	/* Update event for pose and deformation children */
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

struct vgroup_map {
	float head[3], tail[3];
	Bone *bone;
	bDeformGroup *dg;
	Object *meshobj;
};

static void pose_adds_vgroups__mapFunc(void *userData, int index, float *co, float *no_f, short *no_s)
{
	struct vgroup_map *map= userData;
	float vec[3], fac;
	

	VECCOPY(vec, co);
	Mat4MulVecfl(map->meshobj->obmat, vec);
		
	/* get the distance-factor from the vertex to bone */
	fac= distfactor_to_bone (vec, map->head, map->tail, map->bone->rad_head, map->bone->rad_tail, map->bone->dist);
	
	/* add to vgroup. this call also makes me->dverts */
	if(fac!=0.0f) 
		add_vert_to_defgroup (map->meshobj, map->dg, index, fac, WEIGHT_REPLACE);
	else
		remove_vert_defgroup (map->meshobj, map->dg, index);
	
}


void pose_adds_vgroups(Object *meshobj)
{
	struct vgroup_map map;
	DerivedMesh *dm;
	Object *poseobj= meshobj->parent;
	bPoseChannel *pchan;
	Bone *bone;
	bDeformGroup *dg;
	int DMneedsFree;
	
	if(poseobj==NULL || (poseobj->flag & OB_POSEMODE)==0) return;
	
	dm = mesh_get_derived_final(meshobj, &DMneedsFree);
	
	map.meshobj= meshobj;
	
	for(pchan= poseobj->pose->chanbase.first; pchan; pchan= pchan->next) {
		bone= pchan->bone;
		if(bone->flag & (BONE_SELECTED)) {
			
			/* check if mesh has vgroups */
			dg= get_named_vertexgroup(meshobj, bone->name);
			if(dg==NULL)
				dg= add_defgroup_name(meshobj, bone->name);
			
			/* get the root of the bone in global coords */
			VECCOPY(map.head, bone->arm_head);
			Mat4MulVecfl(poseobj->obmat, map.head);
			
            /* get the tip of the bone in global coords */
			VECCOPY(map.tail, bone->arm_tail);
            Mat4MulVecfl(poseobj->obmat, map.tail);
			
			/* use the optimal vertices instead of mverts */
			map.dg= dg;
			map.bone= bone;
			if(dm->foreachMappedVert) 
				dm->foreachMappedVert(dm, pose_adds_vgroups__mapFunc, (void*) &map);
			else {
				Mesh *me= meshobj->data;
				int i;
				for(i=0; i<me->totvert; i++) 
					pose_adds_vgroups__mapFunc(&map, i, (me->mvert+i)->co, NULL, NULL);
			}
			
		}
	}
	
	if (DMneedsFree) dm->release(dm);

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSEDIT, 0);
	
	DAG_object_flush_update(G.scene, meshobj, OB_RECALC_DATA);	// and all its relations

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
			bone_flip_name(newname, 1);	// 1 = do strip off number extensions
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
