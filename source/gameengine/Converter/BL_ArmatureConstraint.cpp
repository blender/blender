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

/** \file gameengine/Converter/BL_ArmatureConstraint.cpp
 *  \ingroup bgeconv
 */


#include "DNA_constraint_types.h"
#include "DNA_action_types.h"
#include "BL_ArmatureConstraint.h"
#include "BL_ArmatureObject.h"
#include "BLI_math.h"
#include "BLI_string.h"

#ifdef WITH_PYTHON

PyTypeObject BL_ArmatureConstraint::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"BL_ArmatureConstraint",
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
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyObject* BL_ArmatureConstraint::py_repr(void)
{
	return PyUnicode_FromString(m_name);
}

#endif // WITH_PYTHON

BL_ArmatureConstraint::BL_ArmatureConstraint(
	BL_ArmatureObject *armature, 
	bPoseChannel *posechannel,
	bConstraint *constraint, 
	KX_GameObject* target,
	KX_GameObject* subtarget)
	: PyObjectPlus(), m_constraint(constraint), m_posechannel(posechannel), m_armature(armature)
{
	m_target = target;
	m_blendtarget = (target) ? target->GetBlenderObject() : NULL;
	m_subtarget = subtarget;
	m_blendsubtarget = (subtarget) ? subtarget->GetBlenderObject() : NULL;
	m_pose = m_subpose = NULL;
	if (m_blendtarget) {
		copy_m4_m4(m_blendmat, m_blendtarget->obmat);
		if (m_blendtarget->type == OB_ARMATURE)
			m_pose = m_blendtarget->pose;
	}
	if (m_blendsubtarget) {
		copy_m4_m4(m_blendsubmat, m_blendsubtarget->obmat);
		if (m_blendsubtarget->type == OB_ARMATURE)
			m_subpose = m_blendsubtarget->pose;
	}
	if (m_target)
		m_target->RegisterObject(m_armature);
	if (m_subtarget)
		m_subtarget->RegisterObject(m_armature);
	BLI_snprintf(m_name, sizeof(m_name), "%s:%s", m_posechannel->name, m_constraint->name);
}

BL_ArmatureConstraint::~BL_ArmatureConstraint()
{
	if (m_target)
		m_target->UnregisterObject(m_armature);
	if (m_subtarget)
		m_subtarget->UnregisterObject(m_armature);
}

BL_ArmatureConstraint* BL_ArmatureConstraint::GetReplica() const
{
	BL_ArmatureConstraint* replica = new BL_ArmatureConstraint(*this);
	replica->ProcessReplica();
	return replica;
}

void BL_ArmatureConstraint::ReParent(BL_ArmatureObject* armature)
{
	m_armature = armature;
	if (m_target)
		m_target->RegisterObject(armature);
	if (m_subtarget)
		m_subtarget->RegisterObject(armature);
	// find the corresponding constraint in the new armature object
	if (m_constraint) {
		bPose* newpose = armature->GetOrigPose();
		char* constraint = m_constraint->name;
		char* posechannel = m_posechannel->name;
		bPoseChannel* pchan;
		bConstraint* pcon;
		m_constraint = NULL;
		m_posechannel = NULL;
		// and locate the constraint
		for (pchan = (bPoseChannel*)newpose->chanbase.first; pchan; pchan=(bPoseChannel*)pchan->next) {
			if (!strcmp(pchan->name, posechannel)) {
				// now locate the constraint
				for (pcon = (bConstraint*)pchan->constraints.first; pcon; pcon=(bConstraint*)pcon->next) {
					if (!strcmp(pcon->name, constraint)) {
						m_constraint = pcon;
						m_posechannel = pchan;
						break;
					}
				}
				break;
			}
		}
	}
}

void BL_ArmatureConstraint::Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_target];
	if (h_obj) {
		m_target->UnregisterObject(m_armature);
		m_target = (KX_GameObject*)(*h_obj);
		m_target->RegisterObject(m_armature);
	}
	h_obj = (*obj_map)[m_subtarget];
	if (h_obj) {
		m_subtarget->UnregisterObject(m_armature);
		m_subtarget = (KX_GameObject*)(*h_obj);
		m_subtarget->RegisterObject(m_armature);
	}
}

bool BL_ArmatureConstraint::UnlinkObject(SCA_IObject* clientobj)
{
	bool res=false;
	if (clientobj == m_target) {
		m_target = NULL;
		res = true;
	}
	if (clientobj == m_subtarget) {
		m_subtarget = NULL;
		res = true;
	}
	return res;
}

void BL_ArmatureConstraint::UpdateTarget()
{
	if (m_constraint && !(m_constraint->flag&CONSTRAINT_OFF) && (!m_blendtarget || m_target)) {
		if (m_blendtarget) {
			// external target, must be updated
			m_target->UpdateBlenderObjectMatrix(m_blendtarget);
			if (m_pose && m_target->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE)
				// update the pose in case a bone is specified in the constraint target
				m_blendtarget->pose = ((BL_ArmatureObject*)m_target)->GetOrigPose();
		}
		if (m_blendsubtarget && m_subtarget) {
			m_subtarget->UpdateBlenderObjectMatrix(m_blendsubtarget);
			if (m_subpose && m_subtarget->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE)
				m_blendsubtarget->pose = ((BL_ArmatureObject*)m_target)->GetOrigPose();
		}
	}
}

void BL_ArmatureConstraint::RestoreTarget()
{
	if (m_constraint && !(m_constraint->flag&CONSTRAINT_OFF) && (!m_blendtarget || m_target)) {
		if (m_blendtarget) {
			copy_m4_m4(m_blendtarget->obmat, m_blendmat);
			if (m_pose)
				m_blendtarget->pose = m_pose;
		}
		if (m_blendsubtarget && m_subtarget) {
			copy_m4_m4(m_blendsubtarget->obmat, m_blendsubmat);
			if (m_subpose)
				m_blendsubtarget->pose = m_subpose;
		}
	}
}

bool BL_ArmatureConstraint::Match(const char* posechannel, const char* constraint)
{
	return (!strcmp(m_posechannel->name, posechannel) && !strcmp(m_constraint->name, constraint));
}

void BL_ArmatureConstraint::SetTarget(KX_GameObject* target)
{
	if (m_blendtarget) {
		if (target != m_target) {
			m_target->UnregisterObject(m_armature);
			m_target = target;
			if (m_target)
				m_target->RegisterObject(m_armature);
		}
	}

}

void BL_ArmatureConstraint::SetSubtarget(KX_GameObject* subtarget)
{
	if (m_blendsubtarget) {
		if (subtarget != m_subtarget) {
			m_subtarget->UnregisterObject(m_armature);
			m_subtarget = subtarget;
			if (m_subtarget)
				m_subtarget->RegisterObject(m_armature);
		}
	}

}

#ifdef WITH_PYTHON

// PYTHON

PyMethodDef BL_ArmatureConstraint::Methods[] = {
  {NULL,NULL} //Sentinel
};

// order of definition of attributes, must match Attributes[] array
#define BCA_TYPE		0
#define BCA_NAME		1
#define BCA_ENFORCE		2
#define BCA_HEADTAIL	3
#define BCA_LINERROR	4
#define BCA_ROTERROR	5
#define BCA_TARGET		6
#define BCA_SUBTARGET	7
#define BCA_ACTIVE		8
#define BCA_IKWEIGHT	9
#define BCA_IKTYPE		10
#define BCA_IKFLAG		11
#define BCA_IKDIST		12
#define BCA_IKMODE		13

PyAttributeDef BL_ArmatureConstraint::Attributes[] = {
	// Keep these attributes in order of BCA_ defines!!! used by py_attr_getattr and py_attr_setattr
	KX_PYATTRIBUTE_RO_FUNCTION("type",BL_ArmatureConstraint,py_attr_getattr),	
	KX_PYATTRIBUTE_RO_FUNCTION("name",BL_ArmatureConstraint,py_attr_getattr),	
	KX_PYATTRIBUTE_RW_FUNCTION("enforce",BL_ArmatureConstraint,py_attr_getattr,py_attr_setattr),
	KX_PYATTRIBUTE_RW_FUNCTION("headtail",BL_ArmatureConstraint,py_attr_getattr,py_attr_setattr),
	KX_PYATTRIBUTE_RO_FUNCTION("lin_error",BL_ArmatureConstraint,py_attr_getattr),
	KX_PYATTRIBUTE_RO_FUNCTION("rot_error",BL_ArmatureConstraint,py_attr_getattr),
	KX_PYATTRIBUTE_RW_FUNCTION("target",BL_ArmatureConstraint,py_attr_getattr,py_attr_setattr),
	KX_PYATTRIBUTE_RW_FUNCTION("subtarget",BL_ArmatureConstraint,py_attr_getattr,py_attr_setattr),
	KX_PYATTRIBUTE_RW_FUNCTION("active",BL_ArmatureConstraint,py_attr_getattr,py_attr_setattr),
	KX_PYATTRIBUTE_RW_FUNCTION("ik_weight",BL_ArmatureConstraint,py_attr_getattr,py_attr_setattr),
	KX_PYATTRIBUTE_RO_FUNCTION("ik_type",BL_ArmatureConstraint,py_attr_getattr),
	KX_PYATTRIBUTE_RO_FUNCTION("ik_flag",BL_ArmatureConstraint,py_attr_getattr),
	KX_PYATTRIBUTE_RW_FUNCTION("ik_dist",BL_ArmatureConstraint,py_attr_getattr,py_attr_setattr),
	KX_PYATTRIBUTE_RW_FUNCTION("ik_mode",BL_ArmatureConstraint,py_attr_getattr,py_attr_setattr),
	
	{ NULL }	//Sentinel
};


PyObject* BL_ArmatureConstraint::py_attr_getattr(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	BL_ArmatureConstraint* self= static_cast<BL_ArmatureConstraint*>(self_v);
	bConstraint* constraint = self->m_constraint;
	bKinematicConstraint* ikconstraint = (constraint && constraint->type == CONSTRAINT_TYPE_KINEMATIC) ? (bKinematicConstraint*)constraint->data : NULL;
	int attr_order = attrdef-Attributes;

	if (!constraint) {
		PyErr_SetString(PyExc_AttributeError, "constraint is NULL");
		return NULL;
	}

	switch (attr_order) {
	case BCA_TYPE:
		return PyLong_FromLong(constraint->type);
	case BCA_NAME:
		return PyUnicode_FromString(constraint->name);
	case BCA_ENFORCE:
		return PyFloat_FromDouble(constraint->enforce);
	case BCA_HEADTAIL:
		return PyFloat_FromDouble(constraint->headtail);
	case BCA_LINERROR:
		return PyFloat_FromDouble(constraint->lin_error);
	case BCA_ROTERROR:
		return PyFloat_FromDouble(constraint->rot_error);
	case BCA_TARGET:
		if (!self->m_target)	
			Py_RETURN_NONE;
		else
			return self->m_target->GetProxy();
	case BCA_SUBTARGET:
		if (!self->m_subtarget)	
			Py_RETURN_NONE;
		else
			return self->m_subtarget->GetProxy();
	case BCA_ACTIVE:
		return PyBool_FromLong(constraint->flag & CONSTRAINT_OFF);
	case BCA_IKWEIGHT:
	case BCA_IKTYPE:
	case BCA_IKFLAG:
	case BCA_IKDIST:
	case BCA_IKMODE:
		if (!ikconstraint) {
			PyErr_SetString(PyExc_AttributeError, "constraint is not of IK type");
			return NULL;
		}
		switch (attr_order) {
		case BCA_IKWEIGHT:
			return PyFloat_FromDouble((ikconstraint)?ikconstraint->weight : 0.0f);
		case BCA_IKTYPE:
			return PyLong_FromLong(ikconstraint->type);
		case BCA_IKFLAG:
			return PyLong_FromLong(ikconstraint->flag);
		case BCA_IKDIST:
			return PyFloat_FromDouble(ikconstraint->dist);
		case BCA_IKMODE:
			return PyLong_FromLong(ikconstraint->mode);
		}
		// should not come here
		break;
	}
	PyErr_SetString(PyExc_AttributeError, "constraint unknown attribute");
	return NULL;
}

int BL_ArmatureConstraint::py_attr_setattr(void *self_v, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	BL_ArmatureConstraint* self= static_cast<BL_ArmatureConstraint*>(self_v);
	bConstraint* constraint = self->m_constraint;
	bKinematicConstraint* ikconstraint = (constraint && constraint->type == CONSTRAINT_TYPE_KINEMATIC) ? (bKinematicConstraint*)constraint->data : NULL;
	int attr_order = attrdef-Attributes;
	int ival;
	double dval;
//	char* sval;
	KX_GameObject *oval;

	if (!constraint) {
		PyErr_SetString(PyExc_AttributeError, "constraint is NULL");
		return PY_SET_ATTR_FAIL;
	}
	
	switch (attr_order) {
	case BCA_ENFORCE:
		dval = PyFloat_AsDouble(value);
		if (dval < 0.0 || dval > 1.0) { /* also accounts for non float */
			PyErr_SetString(PyExc_AttributeError, "constraint.enforce = float: BL_ArmatureConstraint, expected a float between 0 and 1");
			return PY_SET_ATTR_FAIL;
		}
		constraint->enforce = dval;
		return PY_SET_ATTR_SUCCESS;

	case BCA_HEADTAIL:
		dval = PyFloat_AsDouble(value);
		if (dval < 0.0 || dval > 1.0) { /* also accounts for non float */
			PyErr_SetString(PyExc_AttributeError, "constraint.headtail = float: BL_ArmatureConstraint, expected a float between 0 and 1");
			return PY_SET_ATTR_FAIL;
		}
		constraint->headtail = dval;
		return PY_SET_ATTR_SUCCESS;

	case BCA_TARGET:
		if (!ConvertPythonToGameObject(value, &oval, true, "constraint.target = value: BL_ArmatureConstraint"))
			return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error
		self->SetTarget(oval);
		return PY_SET_ATTR_SUCCESS;

	case BCA_SUBTARGET:
		if (!ConvertPythonToGameObject(value, &oval, true, "constraint.subtarget = value: BL_ArmatureConstraint"))
			return PY_SET_ATTR_FAIL; // ConvertPythonToGameObject sets the error
		self->SetSubtarget(oval);
		return PY_SET_ATTR_SUCCESS;

	case BCA_ACTIVE:
		ival = PyObject_IsTrue( value );
		if (ival == -1) {
			PyErr_SetString(PyExc_AttributeError, "constraint.active = bool: BL_ArmatureConstraint, expected True or False");
			return PY_SET_ATTR_FAIL;
		}
		self->m_constraint->flag = (self->m_constraint->flag & ~CONSTRAINT_OFF) | ((ival)?0:CONSTRAINT_OFF);
		return PY_SET_ATTR_SUCCESS;

	case BCA_IKWEIGHT:
	case BCA_IKDIST:
	case BCA_IKMODE:
		if (!ikconstraint) {
			PyErr_SetString(PyExc_AttributeError, "constraint is not of IK type");
			return PY_SET_ATTR_FAIL;
		}
		switch (attr_order) {
		case BCA_IKWEIGHT:
			dval = PyFloat_AsDouble(value);
			if (dval < 0.0 || dval > 1.0) { /* also accounts for non float */
				PyErr_SetString(PyExc_AttributeError, "constraint.weight = float: BL_ArmatureConstraint, expected a float between 0 and 1");
				return PY_SET_ATTR_FAIL;
			}
			ikconstraint->weight = dval;
			return PY_SET_ATTR_SUCCESS;

		case BCA_IKDIST:
			dval = PyFloat_AsDouble(value);
			if (dval < 0.0) {  /* also accounts for non float */
				PyErr_SetString(PyExc_AttributeError, "constraint.ik_dist = float: BL_ArmatureConstraint, expected a positive float");
				return PY_SET_ATTR_FAIL;
			}
			ikconstraint->dist = dval;
			return PY_SET_ATTR_SUCCESS;

		case BCA_IKMODE:
			ival = PyLong_AsLong(value);
			if (ival < 0) {
				PyErr_SetString(PyExc_AttributeError, "constraint.ik_mode = integer: BL_ArmatureConstraint, expected a positive integer");
				return PY_SET_ATTR_FAIL;
			}
			ikconstraint->mode = ival;
			return PY_SET_ATTR_SUCCESS;
		}
		// should not come here
		break;

	}

	PyErr_SetString(PyExc_AttributeError, "constraint unknown attribute");
	return PY_SET_ATTR_FAIL;
}

#endif // WITH_PYTHON
