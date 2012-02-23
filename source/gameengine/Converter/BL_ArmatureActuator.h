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

/** \file BL_ArmatureActuator.h
 *  \ingroup bgeconv
 */

#ifndef __BL_ARMATUREACTUATOR_H__
#define __BL_ARMATUREACTUATOR_H__

#include "SCA_IActuator.h"
#include "BL_ArmatureConstraint.h"

/**
 * This class is the conversion of the Pose channel constraint.
 * It makes a link between the pose constraint and the KX scene.
 * The main purpose is to give access to the constraint target 
 * to link it to a game object. 
 * It also allows to activate/deactivate constraints during the game.
 * Later it will also be possible to create constraint on the fly
 */

class	BL_ArmatureActuator : public SCA_IActuator
{
	Py_Header
public:
	BL_ArmatureActuator(SCA_IObject* gameobj,
						int type,
						const char *posechannel,
						const char *constraintname,
						KX_GameObject* targetobj,
						KX_GameObject* subtargetobj,
						float weight);

	virtual ~BL_ArmatureActuator();

	virtual CValue* GetReplica() {
		BL_ArmatureActuator* replica = new BL_ArmatureActuator(*this);
		replica->ProcessReplica();
		return replica;
	};
	virtual void ProcessReplica();
	virtual bool UnlinkObject(SCA_IObject* clientobj);
	virtual void Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map);
	virtual bool Update(double curtime, bool frame);
	virtual void ReParent(SCA_IObject* parent);
	
#ifdef WITH_PYTHON

	/* These are used to get and set m_target */
	static PyObject* pyattr_get_constraint(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject* pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef);
	static int pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

#endif // WITH_PYTHON

private:
	// identify the constraint that this actuator controls
	void FindConstraint();

	BL_ArmatureConstraint*	m_constraint;
	KX_GameObject*	m_gametarget;
	KX_GameObject*	m_gamesubtarget;
	STR_String		m_posechannel;
	STR_String		m_constraintname;
	float			m_weight;
	int				m_type;
};

#endif //__BL_ARMATUREACTUATOR_H__


