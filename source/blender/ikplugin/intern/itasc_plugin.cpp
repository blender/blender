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

// iTaSC headers
#include "Armature.hpp"
#include "MovingFrame.hpp"
#include "CopyPose.hpp"
#include "WSDLSSolver.hpp"
#include "Scene.hpp"
#include "Cache.hpp"

// Structure pointed by bArmature.ikdata
// It contains everything needed to simulate the armatures
// There can be several simulation islands independent to each other
struct IK_Data
{
	struct IK_Scene* first;
};

typedef float Vector3[3];
typedef float Vector4[4];

// one structure for each target in the scene
struct IK_Target
{
	iTaSC::MovingFrame*		target;
	iTaSC::ConstraintSet*	constraint;
	struct bConstraint*		blenderConstraint;
	std::string				targetName;
	std::string				constraintName;
	int						ee;				//end effector number
	KDL::Frame				eeRest;			//end effector initial pose relative to armature

	IK_Target() {
		target = NULL;
		constraint = NULL;
		blenderConstraint = NULL;
		ee = 0;
		targetName.reserve(32);
		constraintName.reserve(32);
	}
	~IK_Target() {
		if (constraint)
			delete constraint;
		if (target)
			delete target;
	}
};

struct IK_Scene
{
	IK_Scene*			next;
	int					numchan;	// number of channel in pchan
	bPoseChannel**		pchan;		// list of pose in tree, same index as bones and parents
	iTaSC::Armature*	armature;
	iTaSC::Cache*		cache;
	iTaSC::Scene*		scene;
	iTaSC::MovingFrame* base;
	iTaSC::WSDLSSolver* solver;
	
	std::vector<std::string>	bones;		// bones[i] = segment name of the joint from which we get the bone i tail and head
	std::vector<IK_Target*>		targets;

	IK_Scene() {
		next = NULL;
		pchan = NULL;
		armature = NULL;
		cache = NULL;
		scene = NULL;
		base = NULL;
		solver = NULL;
	}

	~IK_Scene() {
		if (pchan)
			MEM_freeN(pchan);
		// delete scene first
		if (scene)
			delete scene;
		for(std::vector<IK_Target*>::iterator it = targets.begin();	it != targets.end(); ++it)
			delete (*it);
		targets.clear();
		if (solver)
			delete solver;
		if (armature)
			delete armature;
		if (base)
			delete base;
		// delete cache last
		if (cache)
			delete cache;
	}
};

enum IK_SegmentFlag {
	IK_XDOF = 1,
	IK_YDOF = 2,
	IK_ZDOF = 4,
	IK_TRANS_XDOF = 8,
	IK_TRANS_YDOF = 16,
	IK_TRANS_ZDOF = 32
};

enum IK_SegmentAxis {
	IK_X = 0,
	IK_Y = 1,
	IK_Z = 2,
	IK_TRANS_X = 3,
	IK_TRANS_Y = 4,
	IK_TRANS_Z = 5
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
	for (rootbone = segcount, segcount = 0, curchan = pchan_tip; segcount < rootbone; segcount++, curchan=curchan->parent) {
		chanlist[segcount]=curchan;
		curchan->flag |= POSE_CHAIN;
	}

	/* setup the chain data */
	/* create a target */
	target= (PoseTarget*)MEM_callocN(sizeof(PoseTarget), "posetarget");
	target->con= con;
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

static IK_Data* get_ikdata(bArmature *arm)
{
	if (arm->ikdata)
		return (IK_Data*)arm->ikdata;
	arm->ikdata = MEM_callocN(sizeof(IK_Data), "iTaSC ikdata");
	// init ikdata if needed
	return (IK_Data*)arm->ikdata;
}
static double EulerAngleFromMatrix(const KDL::Rotation& R, int axis)
{
	double t = sqrt(R(0,0)*R(0,0) + R(0,1)*R(0,1));

	if (t > 16.0*KDL::epsilon) {
		if (axis == 0) return -atan2(R(1,2), R(2,2));
        else if(axis == 1) return atan2(-R(0,2), t);
        else return -atan2(R(0,1), R(0,0));
    } else {
		if (axis == 0) return -atan2(-R(2,1), R(1,1));
        else if(axis == 1) return atan2(-R(0,2), t);
        else return 0.0f;
    }
}

static double ComputeTwist(const KDL::Rotation& R)
{
	// qy and qw are the y and w components of the quaternion from R
	double qy = R(0,2) - R(2,0);
	double qw = R(0,0) + R(1,1) + R(2,2) + 1;

	double tau = 2*atan2(qy, qw);

	return tau;
}

static void RemoveEulerAngleFromMatrix(KDL::Rotation& R, double angle, int axis)
{
	// compute twist parameter
	KDL::Rotation T;
	switch (axis) {
	case 0:
		T = KDL::Rotation::RotX(-angle);
		break;
	case 1:
		T = KDL::Rotation::RotY(-angle);
		break;
	case 2:
		T = KDL::Rotation::RotZ(-angle);
		break;
	default:
		return;
	}
	// remove angle
	R = R*T;
}

static void GetEulerXZY(const KDL::Rotation& R, double& X,double& Z,double& Y)
{
	if (fabs(R(0,1)) > 1.0 - KDL::epsilon ) {
        X = -KDL::sign(R(0,1)) * KDL::atan2(R(1,2), R(1,0));
        Z = -KDL::sign(R(0,1)) * KDL::PI / 2;
        Y = 0.0 ;
    } else {
        X = KDL::atan2(R(2,1), R(1,1));
        Z = KDL::atan2(-R(0,1), sqrt( KDL::sqr(R(0,0)) + KDL::sqr(R(0,2))));
        Y = KDL::atan2(R(0,2), R(0,0));
    }
}

static bool target_callback(const iTaSC::Timestamp& timestamp, const iTaSC::Frame& current, iTaSC::Frame& next, void *param)
{
	IK_Target* target = (IK_Target*)param;
	// compute next target position
	// get target matrix from constraint.
	return false;
}

static bool base_callback(const iTaSC::Timestamp& timestamp, const iTaSC::Frame& current, iTaSC::Frame& next, void *param)
{
	IK_Scene* ikscene = (IK_Scene*)param;
	// compute next armature base pose
	// algorithm: 
	// ikscene->pchan[0] is the root channel of the tree
	// if it has a parent, get the pose matrix from it and replace [3] by parent pchan->tail
	// then multiply by the armature matrix to get ikscene->armature base position
	return false;
}

static IK_Scene* convert_tree(Object *ob, bPoseChannel *pchan)
{
	PoseTree *tree = (PoseTree*)pchan->iktree.first;
	PoseTarget *target;
	bKinematicConstraint *condata;
	iTaSC::Armature* arm;
	iTaSC::Scene* scene;
	IK_Scene* ikscene;
	Bone *bone;
	int a, flag, hasstretch=0;
	float length;
	bool ret = true;
	float restpose[3][3];

	if (tree->totchannel == 0)
		return NULL;

	ikscene = new IK_Scene;
	arm = new iTaSC::Armature();
	scene = new iTaSC::Scene();
	ikscene->pchan = (bPoseChannel**)MEM_mallocN(sizeof(void*)*tree->totchannel, "iTaSC chan list");
	ikscene->numchan = tree->totchannel;
	ikscene->armature = arm;
	ikscene->scene = scene;
	ikscene->cache = new iTaSC::Cache();;
	ikscene->solver = new iTaSC::WSDLSSolver();
	ikscene->bones.resize(tree->totchannel);

	std::string  joint;
	std::string  root("root");
	std::string  parent;
	// assume uniform scaling and take Y scale as general scale for the armature
	float scale = VecLength(ob->obmat[1]);
	double X, Y, Z;

	for(a=0; a<tree->totchannel; a++) {
		pchan= tree->pchan[a];
		bone= pchan->bone;
		ikscene->pchan[a] = pchan;
		
		/* set DoF flag */
		flag= 0;
		if(!(pchan->ikflag & BONE_IK_NO_XDOF) && !(pchan->ikflag & BONE_IK_NO_XDOF_TEMP))
			flag |= IK_XDOF;
		if(!(pchan->ikflag & BONE_IK_NO_YDOF) && !(pchan->ikflag & BONE_IK_NO_YDOF_TEMP))
			flag |= IK_YDOF;
		if(!(pchan->ikflag & BONE_IK_NO_ZDOF) && !(pchan->ikflag & BONE_IK_NO_ZDOF_TEMP))
			flag |= IK_ZDOF;
		
		if(tree->stretch && (pchan->ikstretch > 0.0)) {
			hasstretch = 1;
		}
		/*
		Logic to create the segments:
		RX,RY,RZ = rotational joints with no length
		RY(tip) = rotational joints with a fixed length arm = (0,length,0)
		TY = translational joint on Y axis
		F(pos) = fixed joint with an arm at position pos 
		Conversion rule of the above flags:
		-   ==> F(tip)
		X   ==> RX(tip)
		Y   ==> RY(tip)
		Z   ==> RZ(tip)
		XY  ==> RX+RY(tip)
		XZ  ==> RX+RZ(tip)
		YZ  ==> RZ+RY(tip)
		XYZ ==> RX+RZ+RY(tip)
		In case of stretch, tip=(0,0,0) and there is an additional TY joint
		The frame at last of these joints represents the tail of the bone. 
		The head is computed by a reverse translation on Y axis of the bone length
		or in case of TY joint, by the frame at previous joint.
		In case of separation of bones, there is an additional F(head) joint

		Computing rest pose and length is complicated: the solver works in world space
		Here is the logic:
		rest position is computed only from bone->bone_mat.
		bone length is computed from bone->length multiplied by the scaling factor of
		the armature. Non-uniform scaling will give bad result!

		*/
		KDL::Frame tip(iTaSC::F_identity);
		Vector3 *fl = bone->bone_mat;
		KDL::Rotation bonerot(
			fl[0][0], fl[1][0], fl[2][0],
			fl[0][1], fl[1][1], fl[2][1],
			fl[0][2], fl[1][2], fl[2][2]);
		
		// take scaling into account
		length= bone->length*scale;
		parent = (a > 0) ? ikscene->bones[tree->parent[a]] : root;
		// first the fixed segment to the bone head
		if (VecLength(bone->head) > KDL::epsilon) {
			KDL::Frame head(KDL::Vector(bone->head[0], bone->head[1], bone->head[2]));
			joint = bone->name;
			joint += ":H";
			ret = arm->addSegment(joint, parent, KDL::Joint::None, 0.0, head);
			parent = joint;
		}
		if (!hasstretch) {
			// fixed length, put it in tip
			tip.p[1] = length;
		}
		joint = bone->name;
		switch (flag)
		{
		case 0:
			// fixed bone
			joint += ":F";
			ret = arm->addSegment(joint, parent, KDL::Joint::None, 0.0, tip);
			break;
		case IK_XDOF:
			// RX only, get the X rotation
			X = EulerAngleFromMatrix(bonerot, 0);
			joint += ":RX";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotX, X, tip);
			break;
		case IK_YDOF:
			// RY only, get the Y rotation
			Y = ComputeTwist(bonerot);
			joint += ":RY";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotY, Y, tip);
			break;
		case IK_ZDOF:
			// RX only, get the X rotation
			Z = EulerAngleFromMatrix(bonerot, 2);
			joint += ":RZ";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotZ, Z, tip);
			break;
		case IK_XDOF|IK_YDOF:
			Y = ComputeTwist(bonerot);
			RemoveEulerAngleFromMatrix(bonerot, Y, 1);
			X = EulerAngleFromMatrix(bonerot, 0);
			joint += ":RX";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotX, X);
			if (ret) {
				parent = joint;
				joint = bone->name;
				joint += ":RY";
				ret = arm->addSegment(joint, parent, KDL::Joint::RotY, Y);
			}
			break;
		case IK_XDOF|IK_ZDOF:
			// RX+RZ
			Z = EulerAngleFromMatrix(bonerot, 2);
			RemoveEulerAngleFromMatrix(bonerot, Z, 2);
			X = EulerAngleFromMatrix(bonerot, 0);
			joint += ":RX";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotX, X);
			if (ret) {
				parent = joint;
				joint = bone->name;
				joint += ":RZ";
				ret = arm->addSegment(joint, parent, KDL::Joint::RotZ, Z);
			}
			break;
		case IK_YDOF|IK_ZDOF:
			// RZ+RY
			Y = ComputeTwist(bonerot);
			RemoveEulerAngleFromMatrix(bonerot, Y, 1);
			Z = EulerAngleFromMatrix(bonerot, 2);
			joint += ":RZ";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotZ, Z);
			if (ret) {
				parent = joint;
				joint = bone->name;
				joint += ":RY";
				ret = arm->addSegment(joint, parent, KDL::Joint::RotY, Y);
			}
			break;
		case IK_XDOF|IK_YDOF|IK_ZDOF:
			// RX+RZ+RY
			GetEulerXZY(bonerot, X, Z, Y);
			joint += ":RX";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotX, X);
			if (ret) {
				parent = joint;
				joint = bone->name;
				joint += ":RZ";
				ret = arm->addSegment(joint, parent, KDL::Joint::RotZ, Z);
				if (ret) {
					parent = joint;
					joint = bone->name;
					joint += ":RY";
					ret = arm->addSegment(joint, parent, KDL::Joint::RotY, Y, tip);
				}
			}
			break;
		}
		if (ret && hasstretch) {
			parent = joint;
			joint = bone->name;
			joint += ":TY";
			ret = arm->addSegment(joint, parent, KDL::Joint::TransY, length);
		}
		if (!ret)
			// error making the armature??
			break;
		// joint points to the segment that correspond to the bone per say
		ikscene->bones[a] = joint;
		
		//if (pchan->ikflag & BONE_IK_XLIMIT)
		//	IK_SetLimit(seg, IK_X, pchan->limitmin[0], pchan->limitmax[0]);
		//if (pchan->ikflag & BONE_IK_YLIMIT)
		//	IK_SetLimit(seg, IK_Y, pchan->limitmin[1], pchan->limitmax[1]);
		//if (pchan->ikflag & BONE_IK_ZLIMIT)
		//	IK_SetLimit(seg, IK_Z, pchan->limitmin[2], pchan->limitmax[2]);
		
		//IK_SetStiffness(seg, IK_X, pchan->stiffness[0]);
		//IK_SetStiffness(seg, IK_Y, pchan->stiffness[1]);
		//IK_SetStiffness(seg, IK_Z, pchan->stiffness[2]);
		
		//if(tree->stretch && (pchan->ikstretch > 0.0)) {
		//	float ikstretch = pchan->ikstretch*pchan->ikstretch;
		//	IK_SetStiffness(seg, IK_TRANS_Y, MIN2(1.0-ikstretch, 0.99));
		//	IK_SetLimit(seg, IK_TRANS_Y, 0.001, 1e10);
		//}
	}
	if (!ret) {
		delete ikscene;
		return NULL;
	}
	// for each target, we need to add an end effector in the armature
	for (ret = true, target=(PoseTarget*)tree->targets.first; target; target=(PoseTarget*)target->next) {
		condata= (bKinematicConstraint*)target->con->data;
		pchan = tree->pchan[target->tip];

		// add the end effector
		IK_Target* iktarget = new IK_Target();
		ikscene->targets.push_back(iktarget);
		iktarget->ee = arm->addEndEffector(ikscene->bones[target->tip]);
		if (iktarget->ee == -1) {
			ret = false;
			break;
		}
		// same for target, we don't know the rest position make it world
		iktarget->target = new iTaSC::MovingFrame;
		iktarget->target->setCallback(target_callback, iktarget);
		iktarget->targetName = pchan->bone->name;
		iktarget->targetName += ":T:";
		iktarget->targetName + target->con->name;
		if (!scene->addObject(iktarget->targetName, iktarget->target)) {
			ret = false;
			break;
		}
	}
	// we can now add the armature
	// the armature is based on a moving frame. As this time we don't know where
	// this frame will be located, just take the world origin. In any case,
	// the solver will be called with correct position over time
	ikscene->base = new iTaSC::MovingFrame;
	ikscene->base->setCallback(base_callback, ikscene);
	std::string armname;
	armname = ob->id.name;
	armname += ":B";
	ret = scene->addObject(armname, ikscene->base);
	armname = ob->id.name;
	armname += ":AR";
	if (ret)
		ret = scene->addObject(armname, ikscene->armature, ikscene->base);
	if (!ret) {
		delete ikscene;
		return NULL;
	}
	// get the inverse rest pose frame of the base to compute relative rest pose of end effectors
	// this is needed to handle the enforce parameter
	// ikscene->pchan[0] is the root channel of the tree
	// if it has no parent, then it's just the identify Frame
	KDL::Frame invBaseFrame;
	pchan = ikscene->pchan[0];
	if (pchan->parent) {
		// it has a parent, get the pose matrix from and move to the tail
		pchan = pchan->parent;
		Mat3CpyMat4(restpose, pchan->bone->arm_mat);
		Mat3Ortho(restpose);
		Vector4 *fl = pchan->bone->arm_mat;
		// get the inverse base frame
		invBaseFrame = KDL::Frame(
			KDL::Rotation(
				restpose[0][0], restpose[1][0], restpose[2][0],
				restpose[0][1], restpose[1][1], restpose[2][1],
				restpose[0][2], restpose[1][2], restpose[2][2]),
			KDL::Vector(fl[3][0], fl[3][1], fl[3][2])).Inverse();
	} 
	// finally add the constraint
	for (a=0, target=(PoseTarget*)tree->targets.first; target; target=(PoseTarget*)target->next, a++) {
		condata= (bKinematicConstraint*)target->con->data;
		pchan = tree->pchan[target->tip];

		// add the end effector
		IK_Target* iktarget = ikscene->targets[a];

		unsigned int controltype = 0;
		if ((condata->flag & CONSTRAINT_IK_ROT) && (condata->orientweight != 0.0))
			controltype |= iTaSC::CopyPose::CTL_ROTATION;
		if ((condata->weight != 0.0))
			controltype |= iTaSC::CopyPose::CTL_POSITION;
		iktarget->constraint = new iTaSC::CopyPose(controltype);
		iktarget->blenderConstraint = target->con;
		// set the gain
		if (controltype & iTaSC::CopyPose::CTL_POSITION)
			iktarget->constraint->setControlParameter(iTaSC::CopyPose::ID_POSITION, iTaSC::ACT_ALPHA, condata->weight);
		if (controltype & iTaSC::CopyPose::CTL_ROTATION)
			iktarget->constraint->setControlParameter(iTaSC::CopyPose::ID_ROTATION, iTaSC::ACT_ALPHA, condata->orientweight);
		iktarget->constraintName = pchan->bone->name;
		iktarget->constraintName += ":C:";
		iktarget->constraintName + target->con->name;
		// add the constraint
		ret = scene->addConstraintSet(iktarget->constraintName, iktarget->constraint, armname, iktarget->targetName, ikscene->bones[target->tip]);
		if (!ret)
			break;

		// store the rest pose of the end effector to compute enforce target
		Mat3CpyMat4(restpose, pchan->bone->arm_mat);
		Mat3Ortho(restpose);
		iktarget->eeRest.M = KDL::Rotation(
			restpose[0][0], restpose[1][0], restpose[2][0],
			restpose[0][1], restpose[1][1], restpose[2][1],
			restpose[0][2], restpose[1][2], restpose[2][2]);
		Vector4 *fl = pchan->bone->arm_mat;
		iktarget->eeRest.p = KDL::Vector(fl[3][0], fl[3][1], fl[3][2]);
		// must remove the frame of the armature base
		iktarget->eeRest = invBaseFrame * iktarget->eeRest;
	}
	if (!ret ||
		!scene->addCache(ikscene->cache) ||
		!scene->addSolver(ikscene->solver) ||
		!scene->initialize()) {
		delete ikscene;
		ikscene = NULL;
	}
	return ikscene;
}

static void create_scene(Object *ob)
{
	bArmature *arm = get_armature(ob);
	bPoseChannel *pchan;

	// create the scene
	for(pchan= (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan= (bPoseChannel *)pchan->next) {
		// by construction there is only one tree
		PoseTree *tree = (PoseTree*)pchan->iktree.first;
		if (tree) {
			// convert tree in iTaSC::Scene
			IK_Scene* scene = convert_tree(ob, pchan);
			if (scene) {
				IK_Data* ikdata = get_ikdata(arm);
				scene->next = ikdata->first;
				ikdata->first = scene;
			}
			// delete the trees once we are done
			while(tree) {
				BLI_remlink(&pchan->iktree, tree);
				BLI_freelistN(&tree->targets);
				if(tree->pchan) MEM_freeN(tree->pchan);
				if(tree->parent) MEM_freeN(tree->parent);
				if(tree->basis_change) MEM_freeN(tree->basis_change);
				MEM_freeN(tree);
				tree = (PoseTree*)pchan->iktree.first;
			}
		}
	}
}

static void init_scene(Object *ob)
{
	bPoseChannel *pchan;
	bArmature *arm = get_armature(ob);

	if (arm->ikdata) {
		for(IK_Scene* scene = ((IK_Data*)arm->ikdata)->first;
			scene != NULL;
			scene = scene->next) {
			scene->pchan[0]->flag |= POSE_IKTREE;
		}
	}
}

static void execute_scene(IK_Scene* ikscene, float ctime)
{
	// TBD. For now, just mark the channel done
	for (int i=0; i<ikscene->numchan; i++) {
		ikscene->pchan[i]->flag |= POSE_DONE;
	}
}

//---------------------------------------------------
// plugin interface
//
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
	bArmature *arm;

	arm = get_armature(ob);
	if (arm->ikdata) {
		IK_Data* ikdata = (IK_Data*)arm->ikdata;
		for (IK_Scene* scene = ikdata->first; scene; scene = ikdata->first) {
			if (scene->pchan[0] == pchan) {
				execute_scene(scene, ctime);
				break;
			}
		}
	}
}

void itasc_release_tree(struct Object *ob,  float ctime)
{
	// not used for iTaSC
}

void itasc_remove_armature(struct bArmature *arm)
{
	if (arm->ikdata) {
		IK_Data* ikdata = (IK_Data*)arm->ikdata;
		for (IK_Scene* scene = ikdata->first; scene; scene = ikdata->first) {
			ikdata->first = scene->next;
			delete scene;
		}
		MEM_freeN(ikdata);
		arm->ikdata = NULL;
	}
}

