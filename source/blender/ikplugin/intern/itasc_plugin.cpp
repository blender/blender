/**
 * $Id$
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
 * Original author: Benoit Bolsee
 * Contributor(s): 
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include <string.h>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BIK_api.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_armature.h"
#include "BKE_utildefines.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_armature_types.h"

#include "itasc_plugin.h"

#include "Armature.hpp"

// Structure pointed by bArmature.ikdata
// It contains everything needed to simulate the armatures
// There can be several simulation islands independent to each other
struct IK_Data
{
	struct IK_Scene* first;
};

struct IK_Scene
{
	IK_Scene*			next;
	bPoseChannel*		chan;
	iTaSC::Armature*	armature;
	iTaSC::Cache*		cache;
	iTaSC::Scene*		scene;
	std::vector<iTaSC::ConstraintSet*>		constraints;
};

int initialize_scene(Object *ob, bPoseChannel *pchan_tip)
{
	bPoseChannel *curchan, *pchan_root=NULL, *chanlist[256], **oldchan;
	PoseTree *tree;
	PoseTarget *target;
	bConstraint *con;
	bKinematicConstraint *data;
	int a, segcount= 0, size, newsize, *oldparent, parent, rootbone, treecount;

	/* find IK constraint, and validate it */
	for(con= (bConstraint *)pchan_tip->constraints.first; con; con= (bConstraint *)con->next) {
		if(con->type==CONSTRAINT_TYPE_KINEMATIC) {
			data=(bKinematicConstraint*)con->data;
			if (data->flag & CONSTRAINT_IK_AUTO) break;
			if (data->tar==NULL) continue;
			if (data->tar->type==OB_ARMATURE && data->subtarget[0]==0) continue;
			if ((con->flag & CONSTRAINT_DISABLE)==0 && (con->enforce!=0.0)) break;
		}
	}
	if(con==NULL) return 0;
	
	/* exclude tip from chain? */
	if(!(data->flag & CONSTRAINT_IK_TIP))
		pchan_tip= pchan_tip->parent;
	
	rootbone = data->rootbone;
	/* Find the chain's root & count the segments needed */
	for (curchan = pchan_tip; curchan; curchan=curchan->parent){
		pchan_root = curchan;
		
		if (++segcount > 255)		// 255 is weak
			break;

		if(segcount==rootbone){
			// reached this end of the chain but if the chain is overlapping with a 
			// previous one, we must go back up to the root of the other chain
			if ((curchan->flag & POSE_CHAIN) && curchan->iktree.first == NULL){
				rootbone++;
				continue;
			}
			break; 
		}

		if (curchan->iktree.first != NULL)
			// Oh oh, there is already a chain starting from this channel and our chain is longer... 
			// Should handle this by moving the previous chain up to the begining of our chain
			// For now we just stop here
			break;
	}
	if (!segcount) return 0;
	// we reached a limit and still not the end of a previous chain, quit
	if ((pchan_root->flag & POSE_CHAIN) && pchan_root->iktree.first == NULL) return 0;

	// now that we know how many segment we have, set the flag
	for (rootbone = segcount, segcount = 0, curchan = pchan_tip; segcount < rootbone; curchan=curchan->parent)
		curchan->flag |= POSE_CHAIN;

	/* setup the chain data */
	/* create a target */
	target= (PoseTarget*)MEM_callocN(sizeof(PoseTarget), "posetarget");
	target->con= con;
	pchan_tip->flag &= ~POSE_CHAIN;
	// by contruction there can be only one tree per channel and each channel can be part of at most one tree.
	tree = (PoseTree*)pchan_root->iktree.first;

	if(tree==NULL) {
		/* make new tree */
		tree= (PoseTree*)MEM_callocN(sizeof(PoseTree), "posetree");

		tree->iterations= data->iterations;
		tree->totchannel= segcount;
		tree->stretch = (data->flag & CONSTRAINT_IK_STRETCH);
		
		tree->pchan= (bPoseChannel**)MEM_callocN(segcount*sizeof(void*), "ik tree pchan");
		tree->parent= (int*)MEM_callocN(segcount*sizeof(int), "ik tree parent");
		for(a=0; a<segcount; a++) {
			tree->pchan[a]= chanlist[segcount-a-1];
			tree->parent[a]= a-1;
		}
		target->tip= segcount-1;
		
		/* AND! link the tree to the root */
		BLI_addtail(&pchan_root->iktree, tree);
		// new tree
		treecount = 1;
	}
	else {
		tree->iterations= MAX2(data->iterations, tree->iterations);
		tree->stretch= tree->stretch && !(data->flag & CONSTRAINT_IK_STRETCH);

		/* skip common pose channels and add remaining*/
		size= MIN2(segcount, tree->totchannel);
		for(a=0; a<size && tree->pchan[a]==chanlist[segcount-a-1]; a++);
		parent= a-1;

		segcount= segcount-a;
		target->tip= tree->totchannel + segcount - 1;

		if (segcount > 0) {
			/* resize array */
			newsize= tree->totchannel + segcount;
			oldchan= tree->pchan;
			oldparent= tree->parent;

			tree->pchan= (bPoseChannel**)MEM_callocN(newsize*sizeof(void*), "ik tree pchan");
			tree->parent= (int*)MEM_callocN(newsize*sizeof(int), "ik tree parent");
			memcpy(tree->pchan, oldchan, sizeof(void*)*tree->totchannel);
			memcpy(tree->parent, oldparent, sizeof(int)*tree->totchannel);
			MEM_freeN(oldchan);
			MEM_freeN(oldparent);

			/* add new pose channels at the end, in reverse order */
			for(a=0; a<segcount; a++) {
				tree->pchan[tree->totchannel+a]= chanlist[segcount-a-1];
				tree->parent[tree->totchannel+a]= tree->totchannel+a-1;
			}
			tree->parent[tree->totchannel]= parent;
			
			tree->totchannel= newsize;
		}
		// reusing tree
		treecount = 0;
	}

	/* add target to the tree */
	BLI_addtail(&tree->targets, target);
	/* mark root channel having an IK tree */
	pchan_root->flag |= POSE_IKTREE;
	return treecount;
}

static void create_scene(Object *ob)
{
	bPoseChannel *pchan;
	// create the scene

	// delete the tree structure after we are done
	for(pchan= (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan= (bPoseChannel *)pchan->next) {
		while(pchan->iktree.first) {
			PoseTree *tree= (PoseTree*)pchan->iktree.first;
			BLI_remlink(&pchan->iktree, tree);
			BLI_freelistN(&tree->targets);
			if(tree->pchan) MEM_freeN(tree->pchan);
			if(tree->parent) MEM_freeN(tree->parent);
			if(tree->basis_change) MEM_freeN(tree->basis_change);
			MEM_freeN(tree);
		}
	}
}

void init_scene(Object *ob)
{
	bPoseChannel *pchan;
	bArmature *arm = get_armature(ob);

	if (arm->ikdata) {
		for(IK_Scene* scene = ((IK_Data*)arm->ikdata)->first;
			scene != NULL;
			scene = scene->next) {
			scene->chan->flag |= POSE_IKTREE;
		}
	}
}

void itasc_initialize_tree(Object *ob, float ctime)
{
	bArmature *arm;
	bPoseChannel *pchan;
	int count = 0;

	arm = get_armature(ob);

	if (arm->ikdata != NULL && !(ob->pose->flag & POSE_WAS_REBUILT)) {
		init_scene(ob);
		return;
	}
	// first remove old scene
	itasc_remove_armature(arm);
	// we should handle all the constraint and mark them all disabled
	// for blender but we'll start with the IK constraint alone
	for(pchan= (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan= (bPoseChannel *)pchan->next) {
		if(pchan->constflag & PCHAN_HAS_IK)
			count += initialize_scene(ob, pchan);
	}
	// if at least one tree, create the scenes from the PoseTree stored in the channels 
	if (count)
		create_scene(ob);
	// make sure we don't rebuilt until the user changes something important
	ob->pose->flag &= ~POSE_WAS_REBUILT;
}

void itasc_execute_tree(struct Object *ob,  struct bPoseChannel *pchan, float ctime)
{

}

void itasc_release_tree(struct Object *ob,  float ctime)
{
}

void itasc_remove_armature(struct bArmature *arm)
{

}

