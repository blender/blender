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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BL_ArmatureObject.h
 *  \ingroup bgeconv
 */

#ifndef __BL_ARMATUREOBJECT_H__
#define __BL_ARMATUREOBJECT_H__

#include "KX_GameObject.h"
#include "BL_ArmatureConstraint.h"
#include "BL_ArmatureChannel.h"

#include "SG_IObject.h"
#include <vector>
#include <algorithm>

struct bArmature;
struct Bone;
struct bConstraint;
class BL_ActionActuator;
class BL_ArmatureActuator;
class MT_Matrix4x4;
struct Object;
class KX_BlenderSceneConverter;

class BL_ArmatureObject : public KX_GameObject  
{
	Py_Header
public:

	double GetLastFrame ();
	short GetActivePriority();
	virtual void ProcessReplica();
	virtual void ReParentLogic();
	virtual void Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map);
	virtual bool UnlinkObject(SCA_IObject* clientobj);

	class BL_ActionActuator * GetActiveAction();
	
	BL_ArmatureObject(
		void* sgReplicationInfo,
		SG_Callbacks callbacks,
		Object *armature,
		Scene *scene,
		int vert_deform_type
	);
	virtual ~BL_ArmatureObject();

	virtual CValue*	GetReplica();
	void GetMRDPose(struct bPose **pose);
	void GetPose(struct bPose **pose);
	void SetPose (struct bPose *pose);
	struct bPose *GetOrigPose() {return m_pose;} // never edit this, only for accessing names

	void ApplyPose();
	void RestorePose();

	bool SetActiveAction(class BL_ActionActuator *act, short priority, double curtime);
	
	struct bArmature * GetArmature() { return m_armature; }
	const struct bArmature * GetArmature() const { return m_armature; }
	const struct Scene * GetScene() const { return m_scene; }
	
	Object* GetArmatureObject() {return m_objArma;}

	int GetVertDeformType() {return m_vert_deform_type;}

	// for constraint python API
	void LoadConstraints(KX_BlenderSceneConverter* converter);
	size_t GetConstraintNumber() const { return m_constraintNumber; }
	BL_ArmatureConstraint* GetConstraint(const char* posechannel, const char* constraint);
	BL_ArmatureConstraint* GetConstraint(const char* posechannelconstraint);
	BL_ArmatureConstraint* GetConstraint(int index);
	// for pose channel python API
	void LoadChannels();
	size_t GetChannelNumber() const { return m_channelNumber; }
	BL_ArmatureChannel* GetChannel(bPoseChannel* channel);
	BL_ArmatureChannel* GetChannel(const char* channel);
	BL_ArmatureChannel* GetChannel(int index);

	/// Retrieve the pose matrix for the specified bone.
	/// Returns true on success.
	bool GetBoneMatrix(Bone* bone, MT_Matrix4x4& matrix);
	
	/// Returns the bone length.  The end of the bone is in the local y direction.
	float GetBoneLength(Bone* bone) const;

	virtual int GetGameObjectType() { return OBJ_ARMATURE; }

#ifdef WITH_PYTHON

	// PYTHON
	static PyObject* pyattr_get_constraints(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_channels(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	KX_PYMETHOD_DOC_NOARGS(BL_ArmatureObject, update);

#endif // WITH_PYTHON

protected:
	/* list element: BL_ArmatureConstraint. Use SG_DListHead to have automatic list replication */
	SG_DListHead<BL_ArmatureConstraint>	 m_controlledConstraints;
	/* list element: BL_ArmatureChannel. Use SG_DList to avoid list replication */
	SG_DList			m_poseChannels;
	Object				*m_objArma;
	struct bArmature	*m_armature;
	struct bPose		*m_pose;
	struct bPose		*m_armpose;
	struct bPose		*m_framePose;
	struct Scene		*m_scene; // need for BKE_pose_where_is 
	double	m_lastframe;
	double  m_timestep;		// delta since last pose evaluation.
	class BL_ActionActuator *m_activeAct;
	short	m_activePriority;
	int		m_vert_deform_type;
	size_t  m_constraintNumber;
	size_t  m_channelNumber;
	// store the original armature object matrix
	float m_obmat[4][4];

	double			m_lastapplyframe;
};

/* Pose function specific to the game engine */
void game_blend_poses(struct bPose *dst, struct bPose *src, float srcweight/*, short mode*/); /* was blend_poses */
//void extract_pose_from_pose(struct bPose *pose, const struct bPose *src);
void game_copy_pose(struct bPose **dst, struct bPose *src, int copy_con);
void game_free_pose(struct bPose *pose);


#endif

