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

/** \file BL_ArmatureConstraint.h
 *  \ingroup bgeconv
 */

#ifndef __BL_ARMATURECONSTRAINT_H__
#define __BL_ARMATURECONSTRAINT_H__

#include "DNA_constraint_types.h"
#include "CTR_HashedPtr.h"
#include "CTR_Map.h"
#include "EXP_PyObjectPlus.h"

class SCA_IObject;
class KX_GameObject;
class BL_ArmatureObject;
struct bConstraint;
struct bPoseChannel;
struct Object;
struct bPose;

/**
 * SG_DList : element of controlled constraint list 
 *            head = BL_ArmatureObject::m_controlledConstraints
 * SG_QList : not used
 */
class BL_ArmatureConstraint	: public PyObjectPlus
{
	Py_Header

private:
	struct bConstraint* m_constraint;
	struct bPoseChannel* m_posechannel;
	class BL_ArmatureObject* m_armature;
	char m_name[64];
	KX_GameObject*  m_target;
	KX_GameObject*  m_subtarget;
	struct Object*	m_blendtarget;
	struct Object*	m_blendsubtarget;
	float		m_blendmat[4][4];
	float		m_blendsubmat[4][4];
	struct bPose*	m_pose;
	struct bPose*	m_subpose;

public:
	BL_ArmatureConstraint(class BL_ArmatureObject *armature, 
						struct bPoseChannel *posechannel, 
						struct bConstraint *constraint, 
						KX_GameObject* target,
						KX_GameObject* subtarget);
	virtual ~BL_ArmatureConstraint();

	BL_ArmatureConstraint* GetReplica() const;
	void ReParent(BL_ArmatureObject* armature);
	void Relink(CTR_Map<CTR_HashedPtr, void*> *map);
	bool UnlinkObject(SCA_IObject* clientobj);

	void UpdateTarget();
	void RestoreTarget();

	bool Match(const char* posechannel, const char* constraint);
	const char* GetName() { return m_name; }

	void SetConstraintFlag(int flag)
	{
		if (m_constraint)
			m_constraint->flag |= flag;
	}
	void ClrConstraintFlag(int flag)
	{
		if (m_constraint)
			m_constraint->flag &= ~flag;
	}
	void SetWeight(float weight)
	{
		if (m_constraint && m_constraint->type == CONSTRAINT_TYPE_KINEMATIC && m_constraint->data) {
			bKinematicConstraint* con = (bKinematicConstraint*)m_constraint->data;
			con->weight = weight;
		}
	}
	void SetInfluence(float influence)
	{
		if (m_constraint)
			m_constraint->enforce = influence;
	}
	void SetTarget(KX_GameObject* target);
	void SetSubtarget(KX_GameObject* subtarget);

#ifdef WITH_PYTHON

	// Python access
	virtual PyObject *py_repr(void);

	static PyObject *py_attr_getattr(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int py_attr_setattr(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
#endif  /* WITH_PYTHON */
};

#endif  /* __BL_ARMATURECONSTRAINT_H__ */
