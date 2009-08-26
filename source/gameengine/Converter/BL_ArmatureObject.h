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
 */

#ifndef BL_ARMATUREOBJECT
#define BL_ARMATUREOBJECT

#include "KX_GameObject.h"

#include "SG_IObject.h"

struct bArmature;
struct Bone;
class BL_ActionActuator;
class MT_Matrix4x4;
struct Object;

class BL_ArmatureObject : public KX_GameObject  
{
public:
	double GetLastFrame ();
	short GetActivePriority();
	virtual void ProcessReplica();
	class BL_ActionActuator * GetActiveAction();
	
	BL_ArmatureObject(
		void* sgReplicationInfo,
		SG_Callbacks callbacks,
		Object *armature,
		Scene *scene
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

	/// Retrieve the pose matrix for the specified bone.
	/// Returns true on success.
	bool GetBoneMatrix(Bone* bone, MT_Matrix4x4& matrix);
	
	/// Returns the bone length.  The end of the bone is in the local y direction.
	float GetBoneLength(Bone* bone) const;

	virtual int GetGameObjectType() { return OBJ_ARMATURE; }
protected:
	Object				*m_objArma;
	struct bArmature	*m_armature;
	struct bPose		*m_pose;
	struct bPose		*m_armpose;
	struct bPose		*m_framePose;
	struct Scene		*m_scene; // need for where_is_pose 
	double	m_lastframe;
	class BL_ActionActuator *m_activeAct;
	short	m_activePriority;

	double			m_lastapplyframe;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:BL_ArmatureObject"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif

