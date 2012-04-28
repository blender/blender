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
 * The Original Code is: all of this file.
 *
 * Original author: Benoit Bolsee
 * Contributor(s): 
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/ikplugin/intern/itasc_plugin.cpp
 *  \ingroup ikplugin
 */


#include <stdlib.h>
#include <string.h>
#include <vector>

// iTaSC headers
#ifdef WITH_IK_ITASC
#include "Armature.hpp"
#include "MovingFrame.hpp"
#include "CopyPose.hpp"
#include "WSDLSSolver.hpp"
#include "WDLSSolver.hpp"
#include "Scene.hpp"
#include "Cache.hpp"
#include "Distance.hpp"
#endif

#include "MEM_guardedalloc.h"

extern "C" {
#include "BIK_api.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_utildefines.h"
#include "BKE_constraint.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_armature_types.h"
#include "DNA_scene_types.h"
};

#include "itasc_plugin.h"

// default parameters
bItasc DefIKParam;

// in case of animation mode, feedback and timestep is fixed
#define ANIM_TIMESTEP	1.0
#define ANIM_FEEDBACK	0.8
#define ANIM_QMAX		0.52


// Structure pointed by bPose.ikdata
// It contains everything needed to simulate the armatures
// There can be several simulation islands independent to each other
struct IK_Data
{
	struct IK_Scene* first;
};

typedef float Vector3[3];
typedef float Vector4[4];
struct IK_Target;
typedef void (*ErrorCallback)(const iTaSC::ConstraintValues* values, unsigned int nvalues, IK_Target* iktarget);

// one structure for each target in the scene
struct IK_Target
{
	struct Scene			*blscene;
	iTaSC::MovingFrame*		target;
	iTaSC::ConstraintSet*	constraint;
	struct bConstraint*		blenderConstraint;
	struct bPoseChannel*    rootChannel;
	Object*					owner;			//for auto IK
	ErrorCallback			errorCallback;
	std::string				targetName;
	std::string				constraintName;
	unsigned short			controlType;
	short					channel;		//index in IK channel array of channel on which this target is defined
	short					ee;				//end effector number
	bool					simulation;		//true when simulation mode is used (update feedback)
	bool					eeBlend;		//end effector affected by enforce blending
	float					eeRest[4][4];	//end effector initial pose relative to armature

	IK_Target() {
		blscene = NULL;
		target = NULL;
		constraint = NULL;
		blenderConstraint = NULL;
		rootChannel = NULL;
		owner = NULL;
		controlType = 0;
		channel = 0;
		ee = 0;
		eeBlend = true;
		simulation = true;
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

struct IK_Channel {
	bPoseChannel*	pchan;		// channel where we must copy matrix back
	KDL::Frame		frame;		// frame of the bone relative to object base, not armature base
	std::string		tail;		// segment name of the joint from which we get the bone tail
	std::string     head;		// segment name of the joint from which we get the bone head
	int				parent;		// index in this array of the parent channel
	short			jointType;	// type of joint, combination of IK_SegmentFlag
	char			ndof;		// number of joint angles for this channel
	char			jointValid;	// set to 1 when jointValue has been computed
	// for joint constraint
	Object*			owner;				// for pose and IK param
	double			jointValue[4];		// computed joint value

	IK_Channel() {
		pchan = NULL;
		parent = -1;
		jointType = 0;
		ndof = 0;
		jointValid = 0;
		owner = NULL;
		jointValue[0] = 0.0;
		jointValue[1] = 0.0;
		jointValue[2] = 0.0;
		jointValue[3] = 0.0;
	}
};

struct IK_Scene
{
	struct Scene		*blscene;
	IK_Scene*			next;
	int					numchan;	// number of channel in pchan
	int					numjoint;	// number of joint in jointArray
	// array of bone information, one per channel in the tree
	IK_Channel*			channels;
	iTaSC::Armature*	armature;
	iTaSC::Cache*		cache;
	iTaSC::Scene*		scene;
	iTaSC::MovingFrame* base;		// armature base object
	KDL::Frame			baseFrame;	// frame of armature base relative to blArmature
	KDL::JntArray		jointArray;	// buffer for storing temporary joint array
	iTaSC::Solver*		solver;
	Object*				blArmature;
	struct bConstraint*	polarConstraint;
	std::vector<IK_Target*>		targets;

	IK_Scene() {
		blscene = NULL;
		next = NULL;
		channels = NULL;
		armature = NULL;
		cache = NULL;
		scene = NULL;
		base = NULL;
		solver = NULL;
		blArmature = NULL;
		numchan = 0;
		numjoint = 0;
		polarConstraint = NULL;
	}

	~IK_Scene() {
		// delete scene first
		if (scene)
			delete scene;
		for (std::vector<IK_Target*>::iterator it = targets.begin();	it != targets.end(); ++it)
			delete (*it);
		targets.clear();
		if (channels)
			delete [] channels;
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

// type of IK joint, can be combined to list the joints corresponding to a bone
enum IK_SegmentFlag {
	IK_XDOF = 1,
	IK_YDOF = 2,
	IK_ZDOF = 4,
	IK_SWING = 8,
	IK_REVOLUTE = 16,
	IK_TRANSY = 32,
};

enum IK_SegmentAxis {
	IK_X = 0,
	IK_Y = 1,
	IK_Z = 2,
	IK_TRANS_X = 3,
	IK_TRANS_Y = 4,
	IK_TRANS_Z = 5
};

static int initialize_chain(Object *ob, bPoseChannel *pchan_tip, bConstraint *con)
{
	bPoseChannel *curchan, *pchan_root=NULL, *chanlist[256], **oldchan;
	PoseTree *tree;
	PoseTarget *target;
	bKinematicConstraint *data;
	int a, t, segcount= 0, size, newsize, *oldparent, parent, rootbone, treecount;

	data=(bKinematicConstraint*)con->data;
	
	/* exclude tip from chain? */
	if (!(data->flag & CONSTRAINT_IK_TIP))
		pchan_tip= pchan_tip->parent;
	
	rootbone = data->rootbone;
	/* Find the chain's root & count the segments needed */
	for (curchan = pchan_tip; curchan; curchan=curchan->parent) {
		pchan_root = curchan;
		
		if (++segcount > 255)		// 255 is weak
			break;

		if (segcount==rootbone) {
			// reached this end of the chain but if the chain is overlapping with a 
			// previous one, we must go back up to the root of the other chain
			if ((curchan->flag & POSE_CHAIN) && curchan->iktree.first == NULL) {
				rootbone++;
				continue;
			}
			break; 
		}

		if (curchan->iktree.first != NULL)
			// Oh oh, there is already a chain starting from this channel and our chain is longer... 
			// Should handle this by moving the previous chain up to the beginning of our chain
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

	if (tree==NULL) {
		/* make new tree */
		tree= (PoseTree*)MEM_callocN(sizeof(PoseTree), "posetree");

		tree->iterations= data->iterations;
		tree->totchannel= segcount;
		tree->stretch = (data->flag & CONSTRAINT_IK_STRETCH);
		
		tree->pchan= (bPoseChannel**)MEM_callocN(segcount*sizeof(void*), "ik tree pchan");
		tree->parent= (int*)MEM_callocN(segcount*sizeof(int), "ik tree parent");
		for (a=0; a<segcount; a++) {
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
		a = t = 0;
		while (a<size && t<tree->totchannel) {
			// locate first matching channel
			for (;t<tree->totchannel && tree->pchan[t]!=chanlist[segcount-a-1];t++);
			if (t>=tree->totchannel)
				break;
			for (; a<size && t<tree->totchannel && tree->pchan[t]==chanlist[segcount-a-1]; a++, t++);
		}

		segcount= segcount-a;
		target->tip= tree->totchannel + segcount - 1;

		if (segcount > 0) {
			for (parent = a - 1; parent < tree->totchannel; parent++)
				if (tree->pchan[parent] == chanlist[segcount-1]->parent)
					break;
			
			/* shouldn't happen, but could with dependency cycles */
			if (parent == tree->totchannel)
				parent = a - 1;

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
			for (a=0; a<segcount; a++) {
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

static bool is_cartesian_constraint(bConstraint *con)
{
	//bKinematicConstraint* data=(bKinematicConstraint*)con->data;

	return true;
}

static bool constraint_valid(bConstraint *con)
{
	bKinematicConstraint* data=(bKinematicConstraint*)con->data;

	if (data->flag & CONSTRAINT_IK_AUTO)
		return true;
	if (con->flag & CONSTRAINT_DISABLE)
		return false;
	if (is_cartesian_constraint(con)) {
		/* cartesian space constraint */
		if (data->tar==NULL) 
			return false;
		if (data->tar->type==OB_ARMATURE && data->subtarget[0]==0) 
			return false;
	}
	return true;
}

int initialize_scene(Object *ob, bPoseChannel *pchan_tip)
{
	bConstraint *con;
	int treecount;

	/* find all IK constraints and validate them */
	treecount = 0;
	for (con= (bConstraint *)pchan_tip->constraints.first; con; con= (bConstraint *)con->next) {
		if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
			if (constraint_valid(con))
				treecount += initialize_chain(ob, pchan_tip, con);
		}
	}
	return treecount;
}

static IK_Data* get_ikdata(bPose *pose)
{
	if (pose->ikdata)
		return (IK_Data*)pose->ikdata;
	pose->ikdata = MEM_callocN(sizeof(IK_Data), "iTaSC ikdata");
	// here init ikdata if needed
	// now that we have scene, make sure the default param are initialized
	if (!DefIKParam.iksolver)
		init_pose_itasc(&DefIKParam);

	return (IK_Data*)pose->ikdata;
}
static double EulerAngleFromMatrix(const KDL::Rotation& R, int axis)
{
	double t = KDL::sqrt(R(0,0)*R(0,0) + R(0,1)*R(0,1));

	if (t > 16.0*KDL::epsilon) {
		if (axis == 0) return -KDL::atan2(R(1,2), R(2,2));
		else if (axis == 1) return KDL::atan2(-R(0,2), t);
		else return -KDL::atan2(R(0,1), R(0,0));
	}
	else {
		if (axis == 0) return -KDL::atan2(-R(2,1), R(1,1));
		else if (axis == 1) return KDL::atan2(-R(0,2), t);
		else return 0.0f;
	}
}

static double ComputeTwist(const KDL::Rotation& R)
{
	// qy and qw are the y and w components of the quaternion from R
	double qy = R(0,2) - R(2,0);
	double qw = R(0,0) + R(1,1) + R(2,2) + 1;

	double tau = 2*KDL::atan2(qy, qw);

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

#if 0
static void GetEulerXZY(const KDL::Rotation& R, double& X,double& Z,double& Y)
{
	if (fabs(R(0,1)) > 1.0 - KDL::epsilon ) {
		X = -KDL::sign(R(0,1)) * KDL::atan2(R(1,2), R(1,0));
		Z = -KDL::sign(R(0,1)) * KDL::PI / 2;
		Y = 0.0;
	}
	else {
		X = KDL::atan2(R(2,1), R(1,1));
		Z = KDL::atan2(-R(0,1), KDL::sqrt( KDL::sqr(R(0,0)) + KDL::sqr(R(0,2))));
		Y = KDL::atan2(R(0,2), R(0,0));
	}
}

static void GetEulerXYZ(const KDL::Rotation& R, double& X,double& Y,double& Z)
{
	if (fabs(R(0,2)) > 1.0 - KDL::epsilon ) {
		X = KDL::sign(R(0,2)) * KDL::atan2(-R(1,0), R(1,1));
		Y = KDL::sign(R(0,2)) * KDL::PI / 2;
		Z = 0.0;
	}
	else {
		X = KDL::atan2(-R(1,2), R(2,2));
		Y = KDL::atan2(R(0,2), KDL::sqrt( KDL::sqr(R(0,0)) + KDL::sqr(R(0,1))));
		Z = KDL::atan2(-R(0,1), R(0,0));
	}
}
#endif

static void GetJointRotation(KDL::Rotation& boneRot, int type, double* rot)
{
	switch (type & ~IK_TRANSY) {
	default:
		// fixed bone, no joint
		break;
	case IK_XDOF:
		// RX only, get the X rotation
		rot[0] = EulerAngleFromMatrix(boneRot, 0);
		break;
	case IK_YDOF:
		// RY only, get the Y rotation
		rot[0] = ComputeTwist(boneRot);
		break;
	case IK_ZDOF:
		// RZ only, get the Z rotation
		rot[0] = EulerAngleFromMatrix(boneRot, 2);
		break;
	case IK_XDOF|IK_YDOF:
		rot[1] = ComputeTwist(boneRot);
		RemoveEulerAngleFromMatrix(boneRot, rot[1], 1);
		rot[0] = EulerAngleFromMatrix(boneRot, 0);
		break;
	case IK_SWING:
		// RX+RZ
		boneRot.GetXZRot().GetValue(rot);
		break;
	case IK_YDOF|IK_ZDOF:
		// RZ+RY
		rot[1] = ComputeTwist(boneRot);
		RemoveEulerAngleFromMatrix(boneRot, rot[1], 1);
		rot[0] = EulerAngleFromMatrix(boneRot, 2);
		break;
	case IK_SWING|IK_YDOF:
		rot[2] = ComputeTwist(boneRot);
		RemoveEulerAngleFromMatrix(boneRot, rot[2], 1);
		boneRot.GetXZRot().GetValue(rot);
		break;
	case IK_REVOLUTE:
		boneRot.GetRot().GetValue(rot);
		break;
	}
}

static bool target_callback(const iTaSC::Timestamp& timestamp, const iTaSC::Frame& current, iTaSC::Frame& next, void *param)
{
	IK_Target* target = (IK_Target*)param;
	// compute next target position
	// get target matrix from constraint.
	bConstraint* constraint = (bConstraint*)target->blenderConstraint;
	float tarmat[4][4];

	get_constraint_target_matrix(target->blscene, constraint, 0, CONSTRAINT_OBTYPE_OBJECT, target->owner, tarmat, 1.0);

	// rootmat contains the target pose in world coordinate
	// if enforce is != 1.0, blend the target position with the end effector position
	// if the armature was in rest position. This information is available in eeRest
	if (constraint->enforce != 1.0f && target->eeBlend) {
		// eeRest is relative to the reference frame of the IK root
		// get this frame in world reference
		float restmat[4][4];
		bPoseChannel* pchan = target->rootChannel;
		if (pchan->parent) {
			pchan = pchan->parent;
			float chanmat[4][4];
			copy_m4_m4(chanmat, pchan->pose_mat);
			copy_v3_v3(chanmat[3], pchan->pose_tail);
			mul_serie_m4(restmat, target->owner->obmat, chanmat, target->eeRest, NULL, NULL, NULL, NULL, NULL);
		} 
		else {
			mult_m4_m4m4(restmat, target->owner->obmat, target->eeRest);
		}
		// blend the target
		blend_m4_m4m4(tarmat, restmat, tarmat, constraint->enforce);
	}
	next.setValue(&tarmat[0][0]);
	return true;
}

static bool base_callback(const iTaSC::Timestamp& timestamp, const iTaSC::Frame& current, iTaSC::Frame& next, void *param)
{
	IK_Scene* ikscene = (IK_Scene*)param;
	// compute next armature base pose
	// algorithm: 
	// ikscene->pchan[0] is the root channel of the tree
	// if it has a parent, get the pose matrix from it and replace [3] by parent pchan->tail
	// then multiply by the armature matrix to get ikscene->armature base position
	bPoseChannel* pchan = ikscene->channels[0].pchan;
	float rootmat[4][4];
	if (pchan->parent) {
		pchan = pchan->parent;
		float chanmat[4][4];
		copy_m4_m4(chanmat, pchan->pose_mat);
		copy_v3_v3(chanmat[3], pchan->pose_tail);
		// save the base as a frame too so that we can compute deformation
		// after simulation
		ikscene->baseFrame.setValue(&chanmat[0][0]);
		mult_m4_m4m4(rootmat, ikscene->blArmature->obmat, chanmat);
	} 
	else {
		copy_m4_m4(rootmat, ikscene->blArmature->obmat);
		ikscene->baseFrame = iTaSC::F_identity;
	}
	next.setValue(&rootmat[0][0]);
	// if there is a polar target (only during solving otherwise we don't have end efffector)
	if (ikscene->polarConstraint && timestamp.update) {
		// compute additional rotation of base frame so that armature follows the polar target
		float imat[4][4];		// IK tree base inverse matrix
		float polemat[4][4];	// polar target in IK tree base frame
		float goalmat[4][4];	// target in IK tree base frame
		float mat[4][4];		// temp matrix
		bKinematicConstraint* poledata = (bKinematicConstraint*)ikscene->polarConstraint->data;

		invert_m4_m4(imat, rootmat);
		// polar constraint imply only one target
		IK_Target *iktarget = ikscene->targets[0];
		// root channel from which we take the bone initial orientation
		IK_Channel &rootchan = ikscene->channels[0];

		// get polar target matrix in world space
		get_constraint_target_matrix(ikscene->blscene, ikscene->polarConstraint, 1, CONSTRAINT_OBTYPE_OBJECT, ikscene->blArmature, mat, 1.0);
		// convert to armature space
		mult_m4_m4m4(polemat, imat, mat);
		// get the target in world space (was computed before as target object are defined before base object)
		iktarget->target->getPose().getValue(mat[0]);
		// convert to armature space
		mult_m4_m4m4(goalmat, imat, mat);
		// take position of target, polar target, end effector, in armature space
		KDL::Vector goalpos(goalmat[3]);
		KDL::Vector polepos(polemat[3]);
		KDL::Vector endpos = ikscene->armature->getPose(iktarget->ee).p;
		// get root bone orientation
		KDL::Frame rootframe;
		ikscene->armature->getRelativeFrame(rootframe, rootchan.tail);
		KDL::Vector rootx = rootframe.M.UnitX();
		KDL::Vector rootz = rootframe.M.UnitZ();
		// and compute root bone head
		double q_rest[3], q[3], length;
		const KDL::Joint* joint;
		const KDL::Frame* tip;
		ikscene->armature->getSegment(rootchan.tail, 3, joint, q_rest[0], q[0], tip);
		length = (joint->getType() == KDL::Joint::TransY) ? q[0] : tip->p(1);
		KDL::Vector rootpos = rootframe.p - length*rootframe.M.UnitY();

		// compute main directions 
		KDL::Vector dir = KDL::Normalize(endpos - rootpos);
		KDL::Vector poledir = KDL::Normalize(goalpos-rootpos);
		// compute up directions
		KDL::Vector poleup = KDL::Normalize(polepos-rootpos);
		KDL::Vector up = rootx*KDL::cos(poledata->poleangle) + rootz*KDL::sin(poledata->poleangle);
		// from which we build rotation matrix
		KDL::Rotation endrot, polerot;
		// for the armature, using the root bone orientation
		KDL::Vector x = KDL::Normalize(dir*up);
		endrot.UnitX(x);
		endrot.UnitY(KDL::Normalize(x*dir));
		endrot.UnitZ(-dir);
		// for the polar target 
		x = KDL::Normalize(poledir*poleup);
		polerot.UnitX(x);
		polerot.UnitY(KDL::Normalize(x*poledir));
		polerot.UnitZ(-poledir);
		// the difference between the two is the rotation we want to apply
		KDL::Rotation result(polerot*endrot.Inverse());
		// apply on base frame as this is an artificial additional rotation
		next.M = next.M*result;
		ikscene->baseFrame.M = ikscene->baseFrame.M*result;
	}
	return true;
}

static bool copypose_callback(const iTaSC::Timestamp& timestamp, iTaSC::ConstraintValues* const _values, unsigned int _nvalues, void* _param)
{
	IK_Target* iktarget =(IK_Target*)_param;
	bKinematicConstraint *condata = (bKinematicConstraint *)iktarget->blenderConstraint->data;
	iTaSC::ConstraintValues* values = _values;
	bItasc* ikparam = (bItasc*) iktarget->owner->pose->ikparam;

	// we need default parameters
	if (!ikparam) 
		ikparam = &DefIKParam;

	if (iktarget->blenderConstraint->flag & CONSTRAINT_OFF) {
		if (iktarget->controlType & iTaSC::CopyPose::CTL_POSITION) {
			values->alpha = 0.0;
			values->action = iTaSC::ACT_ALPHA;
			values++;
		}
		if (iktarget->controlType & iTaSC::CopyPose::CTL_ROTATION) {
			values->alpha = 0.0;
			values->action = iTaSC::ACT_ALPHA;
			values++;
		}
	}
	else {
		if (iktarget->controlType & iTaSC::CopyPose::CTL_POSITION) {
			// update error
			values->alpha = condata->weight;
			values->action = iTaSC::ACT_ALPHA|iTaSC::ACT_FEEDBACK;
			values->feedback = (iktarget->simulation) ? ikparam->feedback : ANIM_FEEDBACK;
			values++;
		}
		if (iktarget->controlType & iTaSC::CopyPose::CTL_ROTATION) {
			// update error
			values->alpha = condata->orientweight;
			values->action = iTaSC::ACT_ALPHA|iTaSC::ACT_FEEDBACK;
			values->feedback = (iktarget->simulation) ? ikparam->feedback : ANIM_FEEDBACK;
			values++;
		}
	}
	return true;
}

static void copypose_error(const iTaSC::ConstraintValues* values, unsigned int nvalues, IK_Target* iktarget)
{
	iTaSC::ConstraintSingleValue* value;
	double error;
	int i;

	if (iktarget->controlType & iTaSC::CopyPose::CTL_POSITION) {
		// update error
		for (i=0, error=0.0, value=values->values; i<values->number; ++i, ++value)
			error += KDL::sqr(value->y - value->yd);
		iktarget->blenderConstraint->lin_error = (float)KDL::sqrt(error);
		values++;
	}
	if (iktarget->controlType & iTaSC::CopyPose::CTL_ROTATION) {
		// update error
		for (i=0, error=0.0, value=values->values; i<values->number; ++i, ++value)
			error += KDL::sqr(value->y - value->yd);
		iktarget->blenderConstraint->rot_error = (float)KDL::sqrt(error);
		values++;
	}
}

static bool distance_callback(const iTaSC::Timestamp& timestamp, iTaSC::ConstraintValues* const _values, unsigned int _nvalues, void* _param)
{
	IK_Target* iktarget =(IK_Target*)_param;
	bKinematicConstraint *condata = (bKinematicConstraint *)iktarget->blenderConstraint->data;
	iTaSC::ConstraintValues* values = _values;
	bItasc* ikparam = (bItasc*) iktarget->owner->pose->ikparam;
	// we need default parameters
	if (!ikparam) 
		ikparam = &DefIKParam;

	// update weight according to mode
	if (iktarget->blenderConstraint->flag & CONSTRAINT_OFF) {
		values->alpha = 0.0;
	}
	else {
		switch (condata->mode) {
		case LIMITDIST_INSIDE:
			values->alpha = (values->values[0].y > condata->dist) ? condata->weight : 0.0;
			break;
		case LIMITDIST_OUTSIDE:
			values->alpha = (values->values[0].y < condata->dist) ? condata->weight : 0.0;
			break;
		default:
			values->alpha = condata->weight;
			break;
		}	
		if (!timestamp.substep) {
			// only update value on first timestep
			switch (condata->mode) {
			case LIMITDIST_INSIDE:
				values->values[0].yd = condata->dist*0.95;
				break;
			case LIMITDIST_OUTSIDE:
				values->values[0].yd = condata->dist*1.05;
				break;
			default:
				values->values[0].yd = condata->dist;
				break;
			}
			values->values[0].action = iTaSC::ACT_VALUE|iTaSC::ACT_FEEDBACK;
			values->feedback = (iktarget->simulation) ? ikparam->feedback : ANIM_FEEDBACK;
		}
	}
	values->action |= iTaSC::ACT_ALPHA;
	return true;
}

static void distance_error(const iTaSC::ConstraintValues* values, unsigned int _nvalues, IK_Target* iktarget)
{
	iktarget->blenderConstraint->lin_error = (float)(values->values[0].y - values->values[0].yd);
}

static bool joint_callback(const iTaSC::Timestamp& timestamp, iTaSC::ConstraintValues* const _values, unsigned int _nvalues, void* _param)
{
	IK_Channel* ikchan = (IK_Channel*)_param;
	bItasc* ikparam = (bItasc*)ikchan->owner->pose->ikparam;
	bPoseChannel* chan = ikchan->pchan;
	int dof;

	// a channel can be splitted into multiple joints, so we get called multiple
	// times for one channel (this callback is only for 1 joint in the armature)
	// the IK_JointTarget structure is shared between multiple joint constraint
	// and the target joint values is computed only once, remember this in jointValid
	// Don't forget to reset it before each frame
	if (!ikchan->jointValid) {
		float rmat[3][3];

		if (chan->rotmode > 0) {
			/* euler rotations (will cause gimble lock, but this can be alleviated a bit with rotation orders) */
			eulO_to_mat3( rmat,chan->eul, chan->rotmode);
		}
		else if (chan->rotmode == ROT_MODE_AXISANGLE) {
			/* axis-angle - stored in quaternion data, but not really that great for 3D-changing orientations */
			axis_angle_to_mat3( rmat,&chan->quat[1], chan->quat[0]);
		}
		else {
			/* quats are normalised before use to eliminate scaling issues */
			normalize_qt(chan->quat);
			quat_to_mat3( rmat,chan->quat);
		}
		KDL::Rotation jointRot(
			rmat[0][0], rmat[1][0], rmat[2][0],
			rmat[0][1], rmat[1][1], rmat[2][1],
			rmat[0][2], rmat[1][2], rmat[2][2]);
		GetJointRotation(jointRot, ikchan->jointType, ikchan->jointValue);
		ikchan->jointValid = 1;
	}
	// determine which part of jointValue is used for this joint
	// closely related to the way the joints are defined
	switch (ikchan->jointType & ~IK_TRANSY) {
	case IK_XDOF:
	case IK_YDOF:
	case IK_ZDOF:
		dof = 0;
		break;
	case IK_XDOF|IK_YDOF:
		// X + Y
		dof = (_values[0].id == iTaSC::Armature::ID_JOINT_RX) ? 0 : 1;
		break;
	case IK_SWING:
		// XZ
		dof = 0;
		break;
	case IK_YDOF|IK_ZDOF:
		// Z + Y
		dof = (_values[0].id == iTaSC::Armature::ID_JOINT_RZ) ? 0 : 1;
		break;
	case IK_SWING|IK_YDOF:
		// XZ + Y
		dof = (_values[0].id == iTaSC::Armature::ID_JOINT_RY) ? 2 : 0;
		break;
	case IK_REVOLUTE:
		dof = 0;
		break;
	default:
		dof = -1;
		break;
	}
	if (dof >= 0) {
		for (unsigned int i=0; i<_nvalues; i++, dof++) {
			_values[i].values[0].yd = ikchan->jointValue[dof];
			_values[i].alpha = chan->ikrotweight;
			_values[i].feedback = ikparam->feedback;
		}
	}
	return true;
}

// build array of joint corresponding to IK chain
static int convert_channels(IK_Scene *ikscene, PoseTree *tree)
{
	IK_Channel *ikchan;
	bPoseChannel *pchan;
	int a, flag, njoint;

	njoint = 0;
	for (a=0, ikchan = ikscene->channels; a<ikscene->numchan; ++a, ++ikchan) {
		pchan= tree->pchan[a];
		ikchan->pchan = pchan;
		ikchan->parent = (a>0) ? tree->parent[a] : -1;
		ikchan->owner = ikscene->blArmature;
		
		/* set DoF flag */
		flag = 0;
		if (!(pchan->ikflag & BONE_IK_NO_XDOF) && !(pchan->ikflag & BONE_IK_NO_XDOF_TEMP) &&
		    (!(pchan->ikflag & BONE_IK_XLIMIT) || pchan->limitmin[0]<0.f || pchan->limitmax[0]>0.f))
		{
			flag |= IK_XDOF;
		}
		if (!(pchan->ikflag & BONE_IK_NO_YDOF) && !(pchan->ikflag & BONE_IK_NO_YDOF_TEMP) &&
		    (!(pchan->ikflag & BONE_IK_YLIMIT) || pchan->limitmin[1]<0.f || pchan->limitmax[1]>0.f))
		{
			flag |= IK_YDOF;
		}
		if (!(pchan->ikflag & BONE_IK_NO_ZDOF) && !(pchan->ikflag & BONE_IK_NO_ZDOF_TEMP) &&
		    (!(pchan->ikflag & BONE_IK_ZLIMIT) || pchan->limitmin[2]<0.f || pchan->limitmax[2]>0.f))
		{
			flag |= IK_ZDOF;
		}
		
		if (tree->stretch && (pchan->ikstretch > 0.0)) {
			flag |= IK_TRANSY;
		}
		/*
		 * Logic to create the segments:
		 * RX,RY,RZ = rotational joints with no length
		 * RY(tip) = rotational joints with a fixed length arm = (0,length,0)
		 * TY = translational joint on Y axis
		 * F(pos) = fixed joint with an arm at position pos
		 * Conversion rule of the above flags:
		 * -   ==> F(tip)
		 * X   ==> RX(tip)
		 * Y   ==> RY(tip)
		 * Z   ==> RZ(tip)
		 * XY  ==> RX+RY(tip)
		 * XZ  ==> RX+RZ(tip)
		 * YZ  ==> RZ+RY(tip)
		 * XYZ ==> full spherical unless there are limits, in which case RX+RZ+RY(tip)
		 * In case of stretch, tip=(0,0,0) and there is an additional TY joint
		 * The frame at last of these joints represents the tail of the bone.
		 * The head is computed by a reverse translation on Y axis of the bone length
		 * or in case of TY joint, by the frame at previous joint.
		 * In case of separation of bones, there is an additional F(head) joint
		 *
		 * Computing rest pose and length is complicated: the solver works in world space
		 * Here is the logic:
		 * rest position is computed only from bone->bone_mat.
		 * bone length is computed from bone->length multiplied by the scaling factor of
		 * the armature. Non-uniform scaling will give bad result!
		 */
		switch (flag & (IK_XDOF|IK_YDOF|IK_ZDOF)) {
		default:
			ikchan->jointType = 0;
			ikchan->ndof = 0;
			break;
		case IK_XDOF:
			// RX only, get the X rotation
			ikchan->jointType = IK_XDOF;
			ikchan->ndof = 1;
			break;
		case IK_YDOF:
			// RY only, get the Y rotation
			ikchan->jointType = IK_YDOF;
			ikchan->ndof = 1;
			break;
		case IK_ZDOF:
			// RZ only, get the Zz rotation
			ikchan->jointType = IK_ZDOF;
			ikchan->ndof = 1;
			break;
		case IK_XDOF|IK_YDOF:
			ikchan->jointType = IK_XDOF|IK_YDOF;
			ikchan->ndof = 2;
			break;
		case IK_XDOF|IK_ZDOF:
			// RX+RZ
			ikchan->jointType = IK_SWING;
			ikchan->ndof = 2;
			break;
		case IK_YDOF|IK_ZDOF:
			// RZ+RY
			ikchan->jointType = IK_ZDOF|IK_YDOF;
			ikchan->ndof = 2;
			break;
		case IK_XDOF|IK_YDOF|IK_ZDOF:
			// spherical joint
			if (pchan->ikflag & (BONE_IK_XLIMIT|BONE_IK_YLIMIT|BONE_IK_ZLIMIT))
				// decompose in a Swing+RotY joint
				ikchan->jointType = IK_SWING|IK_YDOF;
			else
				ikchan->jointType = IK_REVOLUTE;
			ikchan->ndof = 3;
			break;
		}
		if (flag & IK_TRANSY) {
			ikchan->jointType |= IK_TRANSY;
			ikchan->ndof++;
		}
		njoint += ikchan->ndof;
	}
	// njoint is the joint coordinate, create the Joint Array
	ikscene->jointArray.resize(njoint);
	ikscene->numjoint = njoint;
	return njoint;
}

// compute array of joint value corresponding to current pose
static void convert_pose(IK_Scene *ikscene)
{
	KDL::Rotation boneRot;
	bPoseChannel *pchan;
	IK_Channel *ikchan;
	Bone *bone;
	float rmat[4][4];	// rest pose of bone with parent taken into account
	float bmat[4][4];	// difference
	float scale;
	double *rot;
	int a, joint;

	// assume uniform scaling and take Y scale as general scale for the armature
	scale = len_v3(ikscene->blArmature->obmat[1]);
	rot = &ikscene->jointArray(0);
	for (joint=a=0, ikchan = ikscene->channels; a<ikscene->numchan && joint<ikscene->numjoint; ++a, ++ikchan) {
		pchan= ikchan->pchan;
		bone= pchan->bone;
		
		if (pchan->parent) {
			unit_m4(bmat);
			mul_m4_m4m3(bmat, pchan->parent->pose_mat, bone->bone_mat);
		}
		else {
			copy_m4_m4(bmat, bone->arm_mat);
		}
		invert_m4_m4(rmat, bmat);
		mult_m4_m4m4(bmat, rmat, pchan->pose_mat);
		normalize_m4(bmat);
		boneRot.setValue(bmat[0]);
		GetJointRotation(boneRot, ikchan->jointType, rot);
		if (ikchan->jointType & IK_TRANSY) {
			// compute actual length 
			rot[ikchan->ndof-1] = len_v3v3(pchan->pose_tail, pchan->pose_head) * scale;
		} 
		rot += ikchan->ndof;
		joint += ikchan->ndof;
	}
}

// compute array of joint value corresponding to current pose
static void rest_pose(IK_Scene *ikscene)
{
	bPoseChannel *pchan;
	IK_Channel *ikchan;
	Bone *bone;
	float scale;
	double *rot;
	int a, joint;

	// assume uniform scaling and take Y scale as general scale for the armature
	scale = len_v3(ikscene->blArmature->obmat[1]);
	// rest pose is 0 
	SetToZero(ikscene->jointArray);
	// except for transY joints
	rot = &ikscene->jointArray(0);
	for (joint=a=0, ikchan = ikscene->channels; a<ikscene->numchan && joint<ikscene->numjoint; ++a, ++ikchan) {
		pchan= ikchan->pchan;
		bone= pchan->bone;

		if (ikchan->jointType & IK_TRANSY)
			rot[ikchan->ndof-1] = bone->length*scale;
		rot += ikchan->ndof;
		joint += ikchan->ndof;
	}
}

static IK_Scene* convert_tree(Scene *blscene, Object *ob, bPoseChannel *pchan)
{
	PoseTree *tree = (PoseTree*)pchan->iktree.first;
	PoseTarget *target;
	bKinematicConstraint *condata;
	bConstraint *polarcon;
	bItasc *ikparam;
	iTaSC::Armature* arm;
	iTaSC::Scene* scene;
	IK_Scene* ikscene;
	IK_Channel* ikchan;
	KDL::Frame initPose;
	KDL::Rotation boneRot;
	Bone *bone;
	int a, numtarget;
	unsigned int t;
	float length;
	bool ret = true, ingame;
	double *rot;

	if (tree->totchannel == 0)
		return NULL;

	ikscene = new IK_Scene;
	ikscene->blscene = blscene;
	arm = new iTaSC::Armature();
	scene = new iTaSC::Scene();
	ikscene->channels = new IK_Channel[tree->totchannel];
	ikscene->numchan = tree->totchannel;
	ikscene->armature = arm;
	ikscene->scene = scene;
	ikparam = (bItasc*)ob->pose->ikparam;
	ingame = (ob->pose->flag & POSE_GAME_ENGINE);
	if (!ikparam) {
		// you must have our own copy
		ikparam = &DefIKParam;
	}
	else if (ingame) {
		// tweak the param when in game to have efficient stepping
		// using fixed substep is not effecient since frames in the GE are often
		// shorter than in animation => move to auto step automatically and set
		// the target substep duration via min/max
		if (!(ikparam->flag & ITASC_AUTO_STEP)) {
			float timestep = blscene->r.frs_sec_base/blscene->r.frs_sec;
			if (ikparam->numstep > 0)
				timestep /= ikparam->numstep;
			// with equal min and max, the algorythm will take this step and the indicative substep most of the time
			ikparam->minstep = ikparam->maxstep = timestep;
			ikparam->flag |= ITASC_AUTO_STEP;
		}
	}
	if ((ikparam->flag & ITASC_SIMULATION) && !ingame)
		// no cache in animation mode
		ikscene->cache = new iTaSC::Cache();

	switch (ikparam->solver) {
	case ITASC_SOLVER_SDLS:
		ikscene->solver = new iTaSC::WSDLSSolver();
		break;
	case ITASC_SOLVER_DLS:
		ikscene->solver = new iTaSC::WDLSSolver();
		break;
	default:
		delete ikscene;
		return NULL;
	}
	ikscene->blArmature = ob;

	std::string  joint;
	std::string  root("root");
	std::string  parent;
	std::vector<double> weights;
	double weight[3];
	// assume uniform scaling and take Y scale as general scale for the armature
	float scale = len_v3(ob->obmat[1]);
	// build the array of joints corresponding to the IK chain
	convert_channels(ikscene, tree);
	if (ingame) {
		// in the GE, set the initial joint angle to match the current pose
		// this will update the jointArray in ikscene
		convert_pose(ikscene);
	}
	else {
		// in Blender, the rest pose is always 0 for joints
		rest_pose(ikscene);
	}
	rot = &ikscene->jointArray(0);
	for (a=0, ikchan = ikscene->channels; a<tree->totchannel; ++a, ++ikchan) {
		pchan= ikchan->pchan;
		bone= pchan->bone;

		KDL::Frame tip(iTaSC::F_identity);
		Vector3 *fl = bone->bone_mat;
		KDL::Rotation brot(
						   fl[0][0], fl[1][0], fl[2][0],
						   fl[0][1], fl[1][1], fl[2][1],
						   fl[0][2], fl[1][2], fl[2][2]);
		KDL::Vector bpos(bone->head[0], bone->head[1], bone->head[2]);
		bpos = bpos*scale;
		KDL::Frame head(brot, bpos);
		
		// rest pose length of the bone taking scaling into account
		length= bone->length*scale;
		parent = (a > 0) ? ikscene->channels[tree->parent[a]].tail : root;
		// first the fixed segment to the bone head
		if (head.p.Norm() > KDL::epsilon || head.M.GetRot().Norm() > KDL::epsilon) {
			joint = bone->name;
			joint += ":H";
			ret = arm->addSegment(joint, parent, KDL::Joint::None, 0.0, head);
			parent = joint;
		}
		if (!(ikchan->jointType & IK_TRANSY)) {
			// fixed length, put it in tip
			tip.p[1] = length;
		}
		joint = bone->name;
		weight[0] = (1.0-pchan->stiffness[0]);
		weight[1] = (1.0-pchan->stiffness[1]);
		weight[2] = (1.0-pchan->stiffness[2]);
		switch (ikchan->jointType & ~IK_TRANSY) {
		case 0:
			// fixed bone
			if (!(ikchan->jointType & IK_TRANSY)) {
				joint += ":F";
				ret = arm->addSegment(joint, parent, KDL::Joint::None, 0.0, tip);
			}
			break;
		case IK_XDOF:
			// RX only, get the X rotation
			joint += ":RX";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotX, rot[0], tip);
			weights.push_back(weight[0]);
			break;
		case IK_YDOF:
			// RY only, get the Y rotation
			joint += ":RY";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotY, rot[0], tip);
			weights.push_back(weight[1]);
			break;
		case IK_ZDOF:
			// RZ only, get the Zz rotation
			joint += ":RZ";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotZ, rot[0], tip);
			weights.push_back(weight[2]);
			break;
		case IK_XDOF|IK_YDOF:
			joint += ":RX";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotX, rot[0]);
			weights.push_back(weight[0]);
			if (ret) {
				parent = joint;
				joint = bone->name;
				joint += ":RY";
				ret = arm->addSegment(joint, parent, KDL::Joint::RotY, rot[1], tip);
				weights.push_back(weight[1]);
			}
			break;
		case IK_SWING:
			joint += ":SW";
			ret = arm->addSegment(joint, parent, KDL::Joint::Swing, rot[0], tip);
			weights.push_back(weight[0]);
			weights.push_back(weight[2]);
			break;
		case IK_YDOF|IK_ZDOF:
			// RZ+RY
			joint += ":RZ";
			ret = arm->addSegment(joint, parent, KDL::Joint::RotZ, rot[0]);
			weights.push_back(weight[2]);
			if (ret) {
				parent = joint;
				joint = bone->name;
				joint += ":RY";
				ret = arm->addSegment(joint, parent, KDL::Joint::RotY, rot[1], tip);
				weights.push_back(weight[1]);
			}
			break;
		case IK_SWING|IK_YDOF:
			// decompose in a Swing+RotY joint
			joint += ":SW";
			ret = arm->addSegment(joint, parent, KDL::Joint::Swing, rot[0]);
			weights.push_back(weight[0]);
			weights.push_back(weight[2]);
			if (ret) {
				parent = joint;
				joint = bone->name;
				joint += ":RY";
				ret = arm->addSegment(joint, parent, KDL::Joint::RotY, rot[2], tip);
				weights.push_back(weight[1]);
			}
			break;
		case IK_REVOLUTE:
			joint += ":SJ";
			ret = arm->addSegment(joint, parent, KDL::Joint::Sphere, rot[0], tip);
			weights.push_back(weight[0]);
			weights.push_back(weight[1]);
			weights.push_back(weight[2]);
			break;
		}
		if (ret && (ikchan->jointType & IK_TRANSY)) {
			parent = joint;
			joint = bone->name;
			joint += ":TY";
			ret = arm->addSegment(joint, parent, KDL::Joint::TransY, rot[ikchan->ndof-1]);
			float ikstretch = pchan->ikstretch*pchan->ikstretch;
			weight[1] = (1.0-MIN2(1.0-ikstretch, 0.99));
			weights.push_back(weight[1]);
		}
		if (!ret)
			// error making the armature??
			break;
		// joint points to the segment that correspond to the bone per say
		ikchan->tail = joint;
		ikchan->head = parent;
		// in case of error
		ret = false;
		if ((ikchan->jointType & IK_XDOF) && (pchan->ikflag & (BONE_IK_XLIMIT|BONE_IK_ROTCTL))) {
			joint = bone->name;
			joint += ":RX";
			if (pchan->ikflag & BONE_IK_XLIMIT) {
				if (arm->addLimitConstraint(joint, 0, pchan->limitmin[0], pchan->limitmax[0]) < 0)
					break;
			}
			if (pchan->ikflag & BONE_IK_ROTCTL) {
				if (arm->addConstraint(joint, joint_callback, ikchan, false, false) < 0)
					break;
			}
		}
		if ((ikchan->jointType & IK_YDOF) && (pchan->ikflag & (BONE_IK_YLIMIT|BONE_IK_ROTCTL))) {
			joint = bone->name;
			joint += ":RY";
			if (pchan->ikflag & BONE_IK_YLIMIT) {
				if (arm->addLimitConstraint(joint, 0, pchan->limitmin[1], pchan->limitmax[1]) < 0)
					break;
			}
			if (pchan->ikflag & BONE_IK_ROTCTL) {
				if (arm->addConstraint(joint, joint_callback, ikchan, false, false) < 0)
					break;
			}
		}
		if ((ikchan->jointType & IK_ZDOF) && (pchan->ikflag & (BONE_IK_ZLIMIT|BONE_IK_ROTCTL))) {
			joint = bone->name;
			joint += ":RZ";
			if (pchan->ikflag & BONE_IK_ZLIMIT) {
				if (arm->addLimitConstraint(joint, 0, pchan->limitmin[2], pchan->limitmax[2]) < 0)
					break;
			}
			if (pchan->ikflag & BONE_IK_ROTCTL) {
				if (arm->addConstraint(joint, joint_callback, ikchan, false, false) < 0)
					break;
			}
		}
		if ((ikchan->jointType & IK_SWING) && (pchan->ikflag & (BONE_IK_XLIMIT|BONE_IK_ZLIMIT|BONE_IK_ROTCTL))) {
			joint = bone->name;
			joint += ":SW";
			if (pchan->ikflag & BONE_IK_XLIMIT) {
				if (arm->addLimitConstraint(joint, 0, pchan->limitmin[0], pchan->limitmax[0]) < 0)
					break;
			}
			if (pchan->ikflag & BONE_IK_ZLIMIT) {
				if (arm->addLimitConstraint(joint, 1, pchan->limitmin[2], pchan->limitmax[2]) < 0)
					break;
			}
			if (pchan->ikflag & BONE_IK_ROTCTL) {
				if (arm->addConstraint(joint, joint_callback, ikchan, false, false) < 0)
					break;
			}
		}
		if ((ikchan->jointType & IK_REVOLUTE) && (pchan->ikflag & BONE_IK_ROTCTL)) {
			joint = bone->name;
			joint += ":SJ";
			if (arm->addConstraint(joint, joint_callback, ikchan, false, false) < 0)
				break;
		}
		//  no error, so restore
		ret = true;
		rot += ikchan->ndof;
	}
	if (!ret) {
		delete ikscene;
		return NULL;
	}
	// for each target, we need to add an end effector in the armature
	for (numtarget=0, polarcon=NULL, ret = true, target=(PoseTarget*)tree->targets.first; target; target=(PoseTarget*)target->next) {
		condata= (bKinematicConstraint*)target->con->data;
		pchan = tree->pchan[target->tip];

		if (is_cartesian_constraint(target->con)) {
			// add the end effector
			IK_Target* iktarget = new IK_Target();
			ikscene->targets.push_back(iktarget);
			iktarget->ee = arm->addEndEffector(ikscene->channels[target->tip].tail);
			if (iktarget->ee == -1) {
				ret = false;
				break;
			}
			// initialize all the fields that we can set at this time
			iktarget->blenderConstraint = target->con;
			iktarget->channel = target->tip;
			iktarget->simulation = (ikparam->flag & ITASC_SIMULATION);
			iktarget->rootChannel = ikscene->channels[0].pchan;
			iktarget->owner = ob;
			iktarget->targetName = pchan->bone->name;
			iktarget->targetName += ":T:";
			iktarget->targetName += target->con->name;
			iktarget->constraintName = pchan->bone->name;
			iktarget->constraintName += ":C:";
			iktarget->constraintName += target->con->name;
			numtarget++;
			if (condata->poletar)
				// this constraint has a polar target
				polarcon = target->con;
		}
	}
	// deal with polar target if any
	if (numtarget == 1 && polarcon) {
		ikscene->polarConstraint = polarcon;
	}
	// we can now add the armature
	// the armature is based on a moving frame. 
	// initialize with the correct position in case there is no cache
	base_callback(iTaSC::Timestamp(), iTaSC::F_identity, initPose, ikscene);
	ikscene->base = new iTaSC::MovingFrame(initPose);
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
	// set the weight
	e_matrix& Wq = arm->getWq();
	assert(Wq.cols() == (int)weights.size());
	for (int q=0; q<Wq.cols(); q++)
		Wq(q,q)=weights[q];
	// get the inverse rest pose frame of the base to compute relative rest pose of end effectors
	// this is needed to handle the enforce parameter
	// ikscene->pchan[0] is the root channel of the tree
	// if it has no parent, then it's just the identify Frame
	float invBaseFrame[4][4];
	pchan = ikscene->channels[0].pchan;
	if (pchan->parent) {
		// it has a parent, get the pose matrix from it 
		float baseFrame[4][4];
		pchan = pchan->parent;	
		copy_m4_m4(baseFrame, pchan->bone->arm_mat);
		// move to the tail and scale to get rest pose of armature base
		copy_v3_v3(baseFrame[3], pchan->bone->arm_tail);
		invert_m4_m4(invBaseFrame, baseFrame);
	}
	else {
		unit_m4(invBaseFrame);
	}
	// finally add the constraint
	for (t=0; t<ikscene->targets.size(); t++) {
		IK_Target* iktarget = ikscene->targets[t];
		iktarget->blscene = blscene;
		condata= (bKinematicConstraint*)iktarget->blenderConstraint->data;
		pchan = tree->pchan[iktarget->channel];
		unsigned int controltype, bonecnt;
		double bonelen;
		float mat[4][4];

		// add the end effector
		// estimate the average bone length, used to clamp feedback error
		for (bonecnt=0, bonelen=0.f, a=iktarget->channel; a>=0; a=tree->parent[a], bonecnt++)
			bonelen += scale*tree->pchan[a]->bone->length;
		bonelen /= bonecnt;		

		// store the rest pose of the end effector to compute enforce target
		copy_m4_m4(mat, pchan->bone->arm_mat);
		copy_v3_v3(mat[3], pchan->bone->arm_tail);
		// get the rest pose relative to the armature base
		mult_m4_m4m4(iktarget->eeRest, invBaseFrame, mat);
		iktarget->eeBlend = (!ikscene->polarConstraint && condata->type==CONSTRAINT_IK_COPYPOSE) ? true : false;
		// use target_callback to make sure the initPose includes enforce coefficient
		target_callback(iTaSC::Timestamp(), iTaSC::F_identity, initPose, iktarget);
		iktarget->target = new iTaSC::MovingFrame(initPose);
		iktarget->target->setCallback(target_callback, iktarget);
		ret = scene->addObject(iktarget->targetName, iktarget->target);
		if (!ret)
			break;

		switch (condata->type) {
		case CONSTRAINT_IK_COPYPOSE:
			controltype = 0;
			if (condata->flag & CONSTRAINT_IK_ROT) {
				if (!(condata->flag & CONSTRAINT_IK_NO_ROT_X))
					controltype |= iTaSC::CopyPose::CTL_ROTATIONX;
				if (!(condata->flag & CONSTRAINT_IK_NO_ROT_Y))
					controltype |= iTaSC::CopyPose::CTL_ROTATIONY;
				if (!(condata->flag & CONSTRAINT_IK_NO_ROT_Z))
					controltype |= iTaSC::CopyPose::CTL_ROTATIONZ;
			}
			if (condata->flag & CONSTRAINT_IK_POS) {
				if (!(condata->flag & CONSTRAINT_IK_NO_POS_X))
					controltype |= iTaSC::CopyPose::CTL_POSITIONX;
				if (!(condata->flag & CONSTRAINT_IK_NO_POS_Y))
					controltype |= iTaSC::CopyPose::CTL_POSITIONY;
				if (!(condata->flag & CONSTRAINT_IK_NO_POS_Z))
					controltype |= iTaSC::CopyPose::CTL_POSITIONZ;
			}
			if (controltype) {
				iktarget->constraint = new iTaSC::CopyPose(controltype, controltype, bonelen);
				// set the gain
				if (controltype & iTaSC::CopyPose::CTL_POSITION)
					iktarget->constraint->setControlParameter(iTaSC::CopyPose::ID_POSITION, iTaSC::ACT_ALPHA, condata->weight);
				if (controltype & iTaSC::CopyPose::CTL_ROTATION)
					iktarget->constraint->setControlParameter(iTaSC::CopyPose::ID_ROTATION, iTaSC::ACT_ALPHA, condata->orientweight);
				iktarget->constraint->registerCallback(copypose_callback, iktarget);
				iktarget->errorCallback = copypose_error;
				iktarget->controlType = controltype;
				// add the constraint
				if (condata->flag & CONSTRAINT_IK_TARGETAXIS)
					ret = scene->addConstraintSet(iktarget->constraintName, iktarget->constraint, iktarget->targetName, armname, "", ikscene->channels[iktarget->channel].tail);
				else
					ret = scene->addConstraintSet(iktarget->constraintName, iktarget->constraint, armname, iktarget->targetName, ikscene->channels[iktarget->channel].tail);
			}
			break;
		case CONSTRAINT_IK_DISTANCE:
			iktarget->constraint = new iTaSC::Distance(bonelen);
			iktarget->constraint->setControlParameter(iTaSC::Distance::ID_DISTANCE, iTaSC::ACT_VALUE, condata->dist);
			iktarget->constraint->registerCallback(distance_callback, iktarget);
			iktarget->errorCallback = distance_error;
			// we can update the weight on each substep
			iktarget->constraint->substep(true);
			// add the constraint
			ret = scene->addConstraintSet(iktarget->constraintName, iktarget->constraint, armname, iktarget->targetName, ikscene->channels[iktarget->channel].tail);
			break;
		}
		if (!ret)
			break;
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

static void create_scene(Scene *scene, Object *ob)
{
	bPoseChannel *pchan;

	// create the IK scene
	for (pchan= (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan= (bPoseChannel *)pchan->next) {
		// by construction there is only one tree
		PoseTree *tree = (PoseTree*)pchan->iktree.first;
		if (tree) {
			IK_Data* ikdata = get_ikdata(ob->pose);
			// convert tree in iTaSC::Scene
			IK_Scene* ikscene = convert_tree(scene, ob, pchan);
			if (ikscene) {
				ikscene->next = ikdata->first;
				ikdata->first = ikscene;
			}
			// delete the trees once we are done
			while (tree) {
				BLI_remlink(&pchan->iktree, tree);
				BLI_freelistN(&tree->targets);
				if (tree->pchan) MEM_freeN(tree->pchan);
				if (tree->parent) MEM_freeN(tree->parent);
				if (tree->basis_change) MEM_freeN(tree->basis_change);
				MEM_freeN(tree);
				tree = (PoseTree*)pchan->iktree.first;
			}
		}
	}
}

static void init_scene(Object *ob)
{
	if (ob->pose->ikdata) {
		for (IK_Scene* scene = ((IK_Data*)ob->pose->ikdata)->first;
			scene != NULL;
			scene = scene->next) {
			scene->channels[0].pchan->flag |= POSE_IKTREE;
		}
	}
}

static void execute_scene(Scene* blscene, IK_Scene* ikscene, bItasc* ikparam, float ctime, float frtime)
{
	int i;
	IK_Channel* ikchan;
	if (ikparam->flag & ITASC_SIMULATION) {
		for (i=0, ikchan=ikscene->channels; i<ikscene->numchan; i++, ++ikchan) {
			// In simulation mode we don't allow external contraint to change our bones, mark the channel done
			// also tell Blender that this channel is part of IK tree (cleared on each where_is_pose()
			ikchan->pchan->flag |= (POSE_DONE|POSE_CHAIN);
			ikchan->jointValid = 0;
		}
	}
	else {
		// in animation mode, we must get the bone position from action and constraints
		for (i=0, ikchan=ikscene->channels; i<ikscene->numchan; i++, ++ikchan) {
			if (!(ikchan->pchan->flag & POSE_DONE))
				where_is_pose_bone(blscene, ikscene->blArmature, ikchan->pchan, ctime, 1);
			// tell blender that this channel was controlled by IK, it's cleared on each where_is_pose()
			ikchan->pchan->flag |= (POSE_DONE|POSE_CHAIN);
			ikchan->jointValid = 0;
		}
	}
	// only run execute the scene if at least one of our target is enabled
	for (i=ikscene->targets.size(); i > 0; --i) {
		IK_Target* iktarget = ikscene->targets[i-1];
		if (!(iktarget->blenderConstraint->flag & CONSTRAINT_OFF))
			break;
	}
	if (i == 0 && ikscene->armature->getNrOfConstraints() == 0)
		// all constraint disabled
		return;

	// compute timestep
	double timestamp = ctime * frtime + 2147483.648;
	double timestep = frtime;
	bool reiterate = (ikparam->flag & ITASC_REITERATION) ? true : false;
	int numstep = (ikparam->flag & ITASC_AUTO_STEP) ? 0 : ikparam->numstep;
	bool simulation = true;

	if (ikparam->flag & ITASC_SIMULATION) {
		ikscene->solver->setParam(iTaSC::Solver::DLS_QMAX, ikparam->maxvel);
	} 
	else {
		// in animation mode we start from the pose after action and constraint
		convert_pose(ikscene);
		ikscene->armature->setJointArray(ikscene->jointArray);
		// and we don't handle velocity
		reiterate = true;
		simulation = false;
		// time is virtual, so take fixed value for velocity parameters (see itasc_update_param)
		// and choose 1s timestep to allow having velocity parameters in radiant 
		timestep = 1.0;
		// use auto setup to let the solver test the variation of the joints
		numstep = 0;
	}
		
	if (ikscene->cache && !reiterate && simulation) {
		iTaSC::CacheTS sts, cts;
		sts = cts = (iTaSC::CacheTS)(timestamp*1000.0+0.5);
		if (ikscene->cache->getPreviousCacheItem(ikscene->armature, 0, &cts) == NULL || cts == 0) {
			// the cache is empty before this time, reiterate
			if (ikparam->flag & ITASC_INITIAL_REITERATION)
				reiterate = true;
		}
		else {
			// can take the cache as a start point.
			sts -= cts;
			timestep = sts/1000.0;
		}
	}
	// don't cache if we are reiterating because we don't want to distroy the cache unnecessarily
	ikscene->scene->update(timestamp, timestep, numstep, false, !reiterate, simulation);
	if (reiterate) {
		// how many times do we reiterate?
		for (i = 0; i<ikparam->numiter; i++) {
			if (ikscene->armature->getMaxJointChange() < ikparam->precision ||
			    ikscene->armature->getMaxEndEffectorChange() < ikparam->precision)
			{
				break;
			}
			ikscene->scene->update(timestamp, timestep, numstep, true, false, simulation);
		}
		if (simulation) {
			// one more fake iteration to cache
			ikscene->scene->update(timestamp, 0.0, 1, true, true, true);
		}
	}
	// compute constraint error
	for (i=ikscene->targets.size(); i > 0; --i) {
		IK_Target* iktarget = ikscene->targets[i-1];
		if (!(iktarget->blenderConstraint->flag & CONSTRAINT_OFF)) {
			unsigned int nvalues;
			const iTaSC::ConstraintValues* values;
			values = iktarget->constraint->getControlParameters(&nvalues);
			iktarget->errorCallback(values, nvalues, iktarget);
		}
	}
	// Apply result to bone:
	// walk the ikscene->channels
	// for each, get the Frame of the joint corresponding to the bone relative to its parent
	// combine the parent and the joint frame to get the frame relative to armature
	// a backward translation of the bone length gives the head
	// if TY, compute the scale as the ratio of the joint length with rest pose length
	iTaSC::Armature* arm = ikscene->armature;
	KDL::Frame frame;
	double q_rest[3], q[3];
	const KDL::Joint* joint;
	const KDL::Frame* tip;
	bPoseChannel* pchan;
	float scale;
	float length;
	float yaxis[3];
	for (i=0, ikchan=ikscene->channels; i<ikscene->numchan; ++i, ++ikchan) {
		if (i == 0) {
			if (!arm->getRelativeFrame(frame, ikchan->tail))
				break;
			// this frame is relative to base, make it relative to object
			ikchan->frame = ikscene->baseFrame * frame;
		} 
		else {
			if (!arm->getRelativeFrame(frame, ikchan->tail, ikscene->channels[ikchan->parent].tail))
				break;
			// combine with parent frame to get frame relative to object
			ikchan->frame = ikscene->channels[ikchan->parent].frame * frame;
		}
		// ikchan->frame is the tail frame relative to object
		// get bone length
		if (!arm->getSegment(ikchan->tail, 3, joint, q_rest[0], q[0], tip))
			break;
		if (joint->getType() == KDL::Joint::TransY) {
			// stretch bones have a TY joint, compute the scale
			scale = (float)(q[0]/q_rest[0]);
			// the length is the joint itself
			length = (float)q[0];
		} 
		else {
			scale = 1.0f;
			// for fixed bone, the length is in the tip (always along Y axis)
			length = tip->p(1);
		}
		// ready to compute the pose mat
		pchan = ikchan->pchan;
		// tail mat
		ikchan->frame.getValue(&pchan->pose_mat[0][0]);
		copy_v3_v3(pchan->pose_tail, pchan->pose_mat[3]);
		// shift to head
		copy_v3_v3(yaxis, pchan->pose_mat[1]);
		mul_v3_fl(yaxis, length);
		sub_v3_v3v3(pchan->pose_mat[3], pchan->pose_mat[3], yaxis);
		copy_v3_v3(pchan->pose_head, pchan->pose_mat[3]);
		// add scale
		mul_v3_fl(pchan->pose_mat[0], scale);
		mul_v3_fl(pchan->pose_mat[1], scale);
		mul_v3_fl(pchan->pose_mat[2], scale);
	}
	if (i<ikscene->numchan) {
		// big problem
		;
	}
}

//---------------------------------------------------
// plugin interface
//
void itasc_initialize_tree(struct Scene *scene, Object *ob, float ctime)
{
	bPoseChannel *pchan;
	int count = 0;

	if (ob->pose->ikdata != NULL && !(ob->pose->flag & POSE_WAS_REBUILT)) {
		init_scene(ob);
		return;
	}
	// first remove old scene
	itasc_clear_data(ob->pose);
	// we should handle all the constraint and mark them all disabled
	// for blender but we'll start with the IK constraint alone
	for (pchan= (bPoseChannel *)ob->pose->chanbase.first; pchan; pchan= (bPoseChannel *)pchan->next) {
		if (pchan->constflag & PCHAN_HAS_IK)
			count += initialize_scene(ob, pchan);
	}
	// if at least one tree, create the scenes from the PoseTree stored in the channels 
	if (count)
		create_scene(scene, ob);
	itasc_update_param(ob->pose);
	// make sure we don't rebuilt until the user changes something important
	ob->pose->flag &= ~POSE_WAS_REBUILT;
}

void itasc_execute_tree(struct Scene *scene, struct Object *ob,  struct bPoseChannel *pchan, float ctime)
{
	if (ob->pose->ikdata) {
		IK_Data* ikdata = (IK_Data*)ob->pose->ikdata;
		bItasc* ikparam = (bItasc*) ob->pose->ikparam;
		// we need default parameters
		if (!ikparam) ikparam = &DefIKParam;

		for (IK_Scene* ikscene = ikdata->first; ikscene; ikscene = ikscene->next) {
			if (ikscene->channels[0].pchan == pchan) {
				float timestep = scene->r.frs_sec_base/scene->r.frs_sec;
				if (ob->pose->flag & POSE_GAME_ENGINE) {
					timestep = ob->pose->ctime;
					// limit the timestep to avoid excessive number of iteration
					if (timestep > 0.2f)
						timestep = 0.2f;
				}
				execute_scene(scene, ikscene, ikparam, ctime, timestep);
				break;
			}
		}
	}
}

void itasc_release_tree(struct Scene *scene, struct Object *ob,  float ctime)
{
	// not used for iTaSC
}

void itasc_clear_data(struct bPose *pose)
{
	if (pose->ikdata) {
		IK_Data* ikdata = (IK_Data*)pose->ikdata;
		for (IK_Scene* scene = ikdata->first; scene; scene = ikdata->first) {
			ikdata->first = scene->next;
			delete scene;
		}
		MEM_freeN(ikdata);
		pose->ikdata = NULL;
	}
}

void itasc_clear_cache(struct bPose *pose)
{
	if (pose->ikdata) {
		IK_Data* ikdata = (IK_Data*)pose->ikdata;
		for (IK_Scene* scene = ikdata->first; scene; scene = scene->next) {
			if (scene->cache)
				// clear all cache but leaving the timestamp 0 (=rest pose)
				scene->cache->clearCacheFrom(NULL, 1);
		}
	}
}

void itasc_update_param(struct bPose *pose)
{
	if (pose->ikdata && pose->ikparam) {
		IK_Data* ikdata = (IK_Data*)pose->ikdata;
		bItasc* ikparam = (bItasc*)pose->ikparam;
		for (IK_Scene* ikscene = ikdata->first; ikscene; ikscene = ikscene->next) {
			double armlength = ikscene->armature->getArmLength();
			ikscene->solver->setParam(iTaSC::Solver::DLS_LAMBDA_MAX, ikparam->dampmax*armlength);
			ikscene->solver->setParam(iTaSC::Solver::DLS_EPSILON, ikparam->dampeps*armlength);
			if (ikparam->flag & ITASC_SIMULATION) {
				ikscene->scene->setParam(iTaSC::Scene::MIN_TIMESTEP, ikparam->minstep);
				ikscene->scene->setParam(iTaSC::Scene::MAX_TIMESTEP, ikparam->maxstep);
				ikscene->solver->setParam(iTaSC::Solver::DLS_QMAX, ikparam->maxvel);
				ikscene->armature->setControlParameter(CONSTRAINT_ID_ALL, iTaSC::Armature::ID_JOINT, iTaSC::ACT_FEEDBACK, ikparam->feedback);
			}
			else {
				// in animation mode timestep is 1s by convention => 
				// qmax becomes radiant and feedback becomes fraction of error gap corrected in one iteration
				ikscene->scene->setParam(iTaSC::Scene::MIN_TIMESTEP, 1.0);
				ikscene->scene->setParam(iTaSC::Scene::MAX_TIMESTEP, 1.0);
				ikscene->solver->setParam(iTaSC::Solver::DLS_QMAX, 0.52);
				ikscene->armature->setControlParameter(CONSTRAINT_ID_ALL, iTaSC::Armature::ID_JOINT, iTaSC::ACT_FEEDBACK, 0.8);
			}
		}
	}
}

void itasc_test_constraint(struct Object *ob, struct bConstraint *cons)
{
	struct bKinematicConstraint *data = (struct bKinematicConstraint *)cons->data;

	/* only for IK constraint */
	if (cons->type != CONSTRAINT_TYPE_KINEMATIC || data == NULL)
		return;

	switch (data->type) {
	case CONSTRAINT_IK_COPYPOSE:
	case CONSTRAINT_IK_DISTANCE:
		/* cartesian space constraint */
		break;
	}
}

