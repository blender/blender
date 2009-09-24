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

#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_actuator_types.h"
#include "BKE_constraint.h"
#include "BLI_arithb.h"
#include "BL_ArmatureActuator.h"
#include "BL_ArmatureObject.h"

/**
 * This class is the conversion of the Pose channel constraint.
 * It makes a link between the pose constraint and the KX scene.
 * The main purpose is to give access to the constraint target 
 * to link it to a game object. 
 * It also allows to activate/deactivate constraints during the game.
 * Later it will also be possible to create constraint on the fly
 */

BL_ArmatureActuator::BL_ArmatureActuator(SCA_IObject* obj,
						int type,
						const char *posechannel,
						const char *constraintname,
						KX_GameObject* targetobj,
						KX_GameObject* subtargetobj,
						float weight) :
	SCA_IActuator(obj, KX_ACT_ARMATURE),
	m_constraint(NULL),
	m_gametarget(targetobj),
	m_gamesubtarget(subtargetobj),
	m_posechannel(posechannel),
	m_constraintname(constraintname),
	m_weight(weight),
	m_type(type)
{
	if (m_gametarget)
		m_gametarget->RegisterActuator(this);
	if (m_gamesubtarget)
		m_gamesubtarget->RegisterActuator(this);
	FindConstraint();
}

BL_ArmatureActuator::~BL_ArmatureActuator()
{
	if (m_gametarget)
		m_gametarget->UnregisterActuator(this);
	if (m_gamesubtarget)
		m_gamesubtarget->UnregisterActuator(this);
}

void BL_ArmatureActuator::ProcessReplica()
{
	// the replica is tracking the same object => register it (this may be changed in Relnk())
	if (m_gametarget)
		m_gametarget->RegisterActuator(this);
	if (m_gamesubtarget)
		m_gamesubtarget->UnregisterActuator(this);
	SCA_IActuator::ProcessReplica();
}

void BL_ArmatureActuator::ReParent(SCA_IObject* parent)
{
	SCA_IActuator::ReParent(parent);
	// must remap the constraint
	FindConstraint();
}

bool BL_ArmatureActuator::UnlinkObject(SCA_IObject* clientobj)
{
	bool res=false;
	if (clientobj == m_gametarget)
	{
		// this object is being deleted, we cannot continue to track it.
		m_gametarget = NULL;
		res = true;
	}
	if (clientobj == m_gamesubtarget)
	{
		// this object is being deleted, we cannot continue to track it.
		m_gamesubtarget = NULL;
		res = true;
	}
	return res;
}

void BL_ArmatureActuator::Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_gametarget];
	if (h_obj) {
		if (m_gametarget)
			m_gametarget->UnregisterActuator(this);
		m_gametarget = (KX_GameObject*)(*h_obj);
		m_gametarget->RegisterActuator(this);
	}
	h_obj = (*obj_map)[m_gamesubtarget];
	if (h_obj) {
		if (m_gamesubtarget)
			m_gamesubtarget->UnregisterActuator(this);
		m_gamesubtarget = (KX_GameObject*)(*h_obj);
		m_gamesubtarget->RegisterActuator(this);
	}
}

void BL_ArmatureActuator::FindConstraint()
{
	m_constraint = NULL;

	if (m_gameobj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE) {
		BL_ArmatureObject* armobj = (BL_ArmatureObject*)m_gameobj;
		m_constraint = armobj->GetConstraint(m_posechannel, m_constraintname);
	}
}

bool BL_ArmatureActuator::Update(double curtime, bool frame)
{
	// the only role of this actuator is to ensure that the armature pose will be evaluated
	bool result = false;	
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (!bNegativeEvent) {
		BL_ArmatureObject *obj = (BL_ArmatureObject*)GetParent();
		switch (m_type) {
		case ACT_ARM_RUN:
			result = true;
			obj->SetActiveAction(NULL, 0, curtime);
			break;
		case ACT_ARM_ENABLE:
			if (m_constraint)
				m_constraint->ClrConstraintFlag(CONSTRAINT_OFF);
			break;
		case ACT_ARM_DISABLE:
			if (m_constraint)
				m_constraint->SetConstraintFlag(CONSTRAINT_OFF);
			break;
		case ACT_ARM_SETTARGET:
			if (m_constraint) {
				m_constraint->SetTarget(m_gametarget);
				m_constraint->SetSubtarget(m_gamesubtarget);
			}
			break;
		case ACT_ARM_SETWEIGHT:
			if (m_constraint)
				m_constraint->SetWeight(m_weight);
			break;
		}
	}
	return result;
}

/* ------------------------------------------------------------------------- */
/* Python Integration Hooks					                                 */
/* ------------------------------------------------------------------------- */

PyTypeObject BL_ArmatureActuator::Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif
		"BL_ArmatureActuator",
		sizeof(PyObjectPlus_Proxy),
		0,
		py_base_dealloc,
		0,
		0,
		0,
		0,
		py_base_repr,
		0,0,0,0,0,0,0,0,0,
		Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
		0,0,0,0,0,0,0,
		Methods,
		0,
		0,
		&SCA_IActuator::Type,
		0,0,0,0,0,0,
		py_base_new
};


PyMethodDef BL_ArmatureActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef BL_ArmatureActuator::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("constraint", BL_ArmatureActuator, pyattr_get_constraint),
	KX_PYATTRIBUTE_RW_FUNCTION("target", BL_ArmatureActuator, pyattr_get_object, pyattr_set_object),
	KX_PYATTRIBUTE_RW_FUNCTION("subtarget", BL_ArmatureActuator, pyattr_get_object, pyattr_set_object),
	KX_PYATTRIBUTE_FLOAT_RW("weight",0.0f,1.0f,BL_ArmatureActuator,m_weight),
	KX_PYATTRIBUTE_INT_RW("type",0,ACT_ARM_MAXTYPE,false,BL_ArmatureActuator,m_type),
	{ NULL }	//Sentinel
};

PyObject* BL_ArmatureActuator::pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ArmatureActuator* actuator = static_cast<BL_ArmatureActuator*>(self);
	KX_GameObject *target = (!strcmp(attrdef->m_name, "target")) ? actuator->m_gametarget : actuator->m_gamesubtarget;
	if (!target)	
		Py_RETURN_NONE;
	else
		return target->GetProxy();
}

int BL_ArmatureActuator::pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	BL_ArmatureActuator* actuator = static_cast<BL_ArmatureActuator*>(self);
	KX_GameObject* &target = (!strcmp(attrdef->m_name, "target")) ? actuator->m_gametarget : actuator->m_gamesubtarget;
	KX_GameObject *gameobj;
		
	if (!ConvertPythonToGameObject(value, &gameobj, true, "actuator.object = value: BL_ArmatureActuator"))
		return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error
		
	if (target != NULL)
		target->UnregisterActuator(actuator);	

	target = gameobj;
		
	if (target)
		target->RegisterActuator(actuator);
		
	return PY_SET_ATTR_SUCCESS;
}

PyObject* BL_ArmatureActuator::pyattr_get_constraint(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ArmatureActuator* actuator = static_cast<BL_ArmatureActuator*>(self);
	BL_ArmatureConstraint* constraint = actuator->m_constraint;
	if (!constraint)	
		Py_RETURN_NONE;
	else
		return constraint->GetProxy();
}



