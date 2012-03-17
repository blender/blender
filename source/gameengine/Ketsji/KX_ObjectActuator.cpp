/*
 * Do translation/rotation actions
 *
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

/** \file gameengine/Ketsji/KX_ObjectActuator.cpp
 *  \ingroup ketsji
 */


#include "KX_ObjectActuator.h"
#include "KX_GameObject.h"
#include "KX_PyMath.h" // For PyVecTo - should this include be put in PyObjectPlus?
#include "KX_IPhysicsController.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_ObjectActuator::
KX_ObjectActuator(
	SCA_IObject* gameobj,
	KX_GameObject* refobj,
	const MT_Vector3& force,
	const MT_Vector3& torque,
	const MT_Vector3& dloc,
	const MT_Vector3& drot,
	const MT_Vector3& linV,
	const MT_Vector3& angV,
	const short damping,
	const KX_LocalFlags& flag
) : 
	SCA_IActuator(gameobj, KX_ACT_OBJECT),
	m_force(force),
	m_torque(torque),
	m_dloc(dloc),
	m_drot(drot),
	m_linear_velocity(linV),
	m_angular_velocity(angV),
	m_linear_length2(0.0),
	m_current_linear_factor(0.0),
	m_current_angular_factor(0.0),
	m_damping(damping),
	m_previous_error(0.0,0.0,0.0),
	m_error_accumulator(0.0,0.0,0.0),
	m_bitLocalFlag (flag),
	m_reference(refobj),
	m_active_combined_velocity (false),
	m_linear_damping_active(false),
	m_angular_damping_active(false)
{
	if (m_bitLocalFlag.ServoControl)
	{
		// in servo motion, the force is local if the target velocity is local
		m_bitLocalFlag.Force = m_bitLocalFlag.LinearVelocity;

		m_pid = m_torque;
	}
	if (m_reference)
		m_reference->RegisterActuator(this);
	UpdateFuzzyFlags();
}

KX_ObjectActuator::~KX_ObjectActuator()
{
	if (m_reference)
		m_reference->UnregisterActuator(this);
}

bool KX_ObjectActuator::Update()
{
	
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();
		
	KX_GameObject *parent = static_cast<KX_GameObject *>(GetParent()); 

	if (bNegativeEvent) {
		// If we previously set the linear velocity we now have to inform
		// the physics controller that we no longer wish to apply it and that
		// it should reconcile the externally set velocity with it's 
		// own velocity.
		if (m_active_combined_velocity) {
			if (parent)
				parent->ResolveCombinedVelocities(
						m_linear_velocity,
						m_angular_velocity,
						(m_bitLocalFlag.LinearVelocity) != 0,
						(m_bitLocalFlag.AngularVelocity) != 0
					);
			m_active_combined_velocity = false;
		} 
		m_linear_damping_active = false;
		m_angular_damping_active = false;
		m_error_accumulator.setValue(0.0,0.0,0.0);
		m_previous_error.setValue(0.0,0.0,0.0);
		return false; 

	} else if (parent)
	{
		if (m_bitLocalFlag.ServoControl) 
		{
			// In this mode, we try to reach a target speed using force
			// As we don't know the friction, we must implement a generic 
			// servo control to achieve the speed in a configurable
			// v = current velocity
			// V = target velocity
			// e = V-v = speed error
			// dt = time interval since previous update
			// I = sum(e(t)*dt)
			// dv = e(t) - e(t-1)
			// KP, KD, KI : coefficient
			// F = KP*e+KI*I+KD*dv
			MT_Scalar mass = parent->GetMass();
			if (mass < MT_EPSILON)
				return false;
			MT_Vector3 v = parent->GetLinearVelocity(m_bitLocalFlag.LinearVelocity);
			if (m_reference)
			{
				const MT_Point3& mypos = parent->NodeGetWorldPosition();
				const MT_Point3& refpos = m_reference->NodeGetWorldPosition();
				MT_Point3 relpos;
				relpos = (mypos-refpos);
				MT_Vector3 vel= m_reference->GetVelocity(relpos);
				if (m_bitLocalFlag.LinearVelocity)
					// must convert in local space
					vel = parent->NodeGetWorldOrientation().transposed()*vel;
				v -= vel;
			}
			MT_Vector3 e = m_linear_velocity - v;
			MT_Vector3 dv = e - m_previous_error;
			MT_Vector3 I = m_error_accumulator + e;

			m_force = m_pid.x()*e+m_pid.y()*I+m_pid.z()*dv;
			// to automatically adapt the PID coefficient to mass;
			m_force *= mass;
			if (m_bitLocalFlag.Torque) 
			{
				if (m_force[0] > m_dloc[0])
				{
					m_force[0] = m_dloc[0];
					I[0] = m_error_accumulator[0];
				} else if (m_force[0] < m_drot[0])
				{
					m_force[0] = m_drot[0];
					I[0] = m_error_accumulator[0];
				}
			}
			if (m_bitLocalFlag.DLoc) 
			{
				if (m_force[1] > m_dloc[1])
				{
					m_force[1] = m_dloc[1];
					I[1] = m_error_accumulator[1];
				} else if (m_force[1] < m_drot[1])
				{
					m_force[1] = m_drot[1];
					I[1] = m_error_accumulator[1];
				}
			}
			if (m_bitLocalFlag.DRot) 
			{
				if (m_force[2] > m_dloc[2])
				{
					m_force[2] = m_dloc[2];
					I[2] = m_error_accumulator[2];
				} else if (m_force[2] < m_drot[2])
				{
					m_force[2] = m_drot[2];
					I[2] = m_error_accumulator[2];
				}
			}
			m_previous_error = e;
			m_error_accumulator = I;
			parent->ApplyForce(m_force,(m_bitLocalFlag.LinearVelocity) != 0);
		} else
		{
			if (!m_bitLocalFlag.ZeroForce)
			{
				parent->ApplyForce(m_force,(m_bitLocalFlag.Force) != 0);
			}
			if (!m_bitLocalFlag.ZeroTorque)
			{
				parent->ApplyTorque(m_torque,(m_bitLocalFlag.Torque) != 0);
			}
			if (!m_bitLocalFlag.ZeroDLoc)
			{
				parent->ApplyMovement(m_dloc,(m_bitLocalFlag.DLoc) != 0);
			}
			if (!m_bitLocalFlag.ZeroDRot)
			{
				parent->ApplyRotation(m_drot,(m_bitLocalFlag.DRot) != 0);
			}
			if (!m_bitLocalFlag.ZeroLinearVelocity)
			{
				if (m_bitLocalFlag.AddOrSetLinV) {
					parent->addLinearVelocity(m_linear_velocity,(m_bitLocalFlag.LinearVelocity) != 0);
				} else {
					m_active_combined_velocity = true;
					if (m_damping > 0) {
						MT_Vector3 linV;
						if (!m_linear_damping_active) {
							// delta and the start speed (depends on the existing speed in that direction)
							linV = parent->GetLinearVelocity(m_bitLocalFlag.LinearVelocity);
							// keep only the projection along the desired direction
							m_current_linear_factor = linV.dot(m_linear_velocity)/m_linear_length2;
							m_linear_damping_active = true;
						}
						if (m_current_linear_factor < 1.0)
							m_current_linear_factor += 1.0/m_damping;
						if (m_current_linear_factor > 1.0)
							m_current_linear_factor = 1.0;
						linV = m_current_linear_factor * m_linear_velocity;
						parent->setLinearVelocity(linV,(m_bitLocalFlag.LinearVelocity) != 0);
					} else {
						parent->setLinearVelocity(m_linear_velocity,(m_bitLocalFlag.LinearVelocity) != 0);
					}
				}
			}
			if (!m_bitLocalFlag.ZeroAngularVelocity)
			{
				m_active_combined_velocity = true;
				if (m_damping > 0) {
					MT_Vector3 angV;
					if (!m_angular_damping_active) {
						// delta and the start speed (depends on the existing speed in that direction)
						angV = parent->GetAngularVelocity(m_bitLocalFlag.AngularVelocity);
						// keep only the projection along the desired direction
						m_current_angular_factor = angV.dot(m_angular_velocity)/m_angular_length2;
						m_angular_damping_active = true;
					}
					if (m_current_angular_factor < 1.0)
						m_current_angular_factor += 1.0/m_damping;
					if (m_current_angular_factor > 1.0)
						m_current_angular_factor = 1.0;
					angV = m_current_angular_factor * m_angular_velocity;
					parent->setAngularVelocity(angV,(m_bitLocalFlag.AngularVelocity) != 0);
				} else {
					parent->setAngularVelocity(m_angular_velocity,(m_bitLocalFlag.AngularVelocity) != 0);
				}
			}
		}
		
	}
	return true;
}



CValue* KX_ObjectActuator::GetReplica()
{
	KX_ObjectActuator* replica = new KX_ObjectActuator(*this);//m_float,GetName());
	replica->ProcessReplica();

	return replica;
}

void KX_ObjectActuator::ProcessReplica()
{
	SCA_IActuator::ProcessReplica();
	if (m_reference)
		m_reference->RegisterActuator(this);
}

bool KX_ObjectActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == (SCA_IObject*)m_reference)
	{
		// this object is being deleted, we cannot continue to use it as reference.
		m_reference = NULL;
		return true;
	}
	return false;
}

void KX_ObjectActuator::Relink(CTR_Map<CTR_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_reference];
	if (h_obj) {
		if (m_reference)
			m_reference->UnregisterActuator(this);
		m_reference = (KX_GameObject*)(*h_obj);
		m_reference->RegisterActuator(this);
	}
}

/* some 'standard' utilities... */
bool KX_ObjectActuator::isValid(KX_ObjectActuator::KX_OBJECT_ACT_VEC_TYPE type)
{
	bool res = false;
	res = (type > KX_OBJECT_ACT_NODEF) && (type < KX_OBJECT_ACT_MAX);
	return res;
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_ObjectActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_ObjectActuator",
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

PyMethodDef KX_ObjectActuator::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_ObjectActuator::Attributes[] = {
	KX_PYATTRIBUTE_VECTOR_RW_CHECK("force", -1000, 1000, false, KX_ObjectActuator, m_force, PyUpdateFuzzyFlags),
	KX_PYATTRIBUTE_BOOL_RW("useLocalForce", KX_ObjectActuator, m_bitLocalFlag.Force),
	KX_PYATTRIBUTE_VECTOR_RW_CHECK("torque", -1000, 1000, false, KX_ObjectActuator, m_torque, PyUpdateFuzzyFlags),
	KX_PYATTRIBUTE_BOOL_RW("useLocalTorque", KX_ObjectActuator, m_bitLocalFlag.Torque),
	KX_PYATTRIBUTE_VECTOR_RW_CHECK("dLoc", -1000, 1000, false, KX_ObjectActuator, m_dloc, PyUpdateFuzzyFlags),
	KX_PYATTRIBUTE_BOOL_RW("useLocalDLoc", KX_ObjectActuator, m_bitLocalFlag.DLoc),
	KX_PYATTRIBUTE_VECTOR_RW_CHECK("dRot", -1000, 1000, false, KX_ObjectActuator, m_drot, PyUpdateFuzzyFlags),
	KX_PYATTRIBUTE_BOOL_RW("useLocalDRot", KX_ObjectActuator, m_bitLocalFlag.DRot),
#ifdef USE_MATHUTILS
	KX_PYATTRIBUTE_RW_FUNCTION("linV", KX_ObjectActuator, pyattr_get_linV, pyattr_set_linV),
	KX_PYATTRIBUTE_RW_FUNCTION("angV", KX_ObjectActuator, pyattr_get_angV, pyattr_set_angV),
#else
	KX_PYATTRIBUTE_VECTOR_RW_CHECK("linV", -1000, 1000, false, KX_ObjectActuator, m_linear_velocity, PyUpdateFuzzyFlags),
	KX_PYATTRIBUTE_VECTOR_RW_CHECK("angV", -1000, 1000, false, KX_ObjectActuator, m_angular_velocity, PyUpdateFuzzyFlags),
#endif
	KX_PYATTRIBUTE_BOOL_RW("useLocalLinV", KX_ObjectActuator, m_bitLocalFlag.LinearVelocity),
	KX_PYATTRIBUTE_BOOL_RW("useLocalAngV", KX_ObjectActuator, m_bitLocalFlag.AngularVelocity),
	KX_PYATTRIBUTE_SHORT_RW("damping", 0, 1000, false, KX_ObjectActuator, m_damping),
	KX_PYATTRIBUTE_RW_FUNCTION("forceLimitX", KX_ObjectActuator, pyattr_get_forceLimitX, pyattr_set_forceLimitX),
	KX_PYATTRIBUTE_RW_FUNCTION("forceLimitY", KX_ObjectActuator, pyattr_get_forceLimitY, pyattr_set_forceLimitY),
	KX_PYATTRIBUTE_RW_FUNCTION("forceLimitZ", KX_ObjectActuator, pyattr_get_forceLimitZ, pyattr_set_forceLimitZ),
	KX_PYATTRIBUTE_VECTOR_RW_CHECK("pid", -100, 200, true, KX_ObjectActuator, m_pid, PyCheckPid),
	KX_PYATTRIBUTE_RW_FUNCTION("reference", KX_ObjectActuator,pyattr_get_reference,pyattr_set_reference),
	{ NULL }	//Sentinel
};

/* Attribute get/set functions */

#ifdef USE_MATHUTILS

/* These require an SGNode */
#define MATHUTILS_VEC_CB_LINV 1
#define MATHUTILS_VEC_CB_ANGV 2

static unsigned char mathutils_kxobactu_vector_cb_index = -1; /* index for our callbacks */

static int mathutils_obactu_generic_check(BaseMathObject *bmo)
{
	KX_ObjectActuator* self= static_cast<KX_ObjectActuator*>BGE_PROXY_REF(bmo->cb_user);
	if(self==NULL)
		return -1;

	return 0;
}

static int mathutils_obactu_vector_get(BaseMathObject *bmo, int subtype)
{
	KX_ObjectActuator* self= static_cast<KX_ObjectActuator*>BGE_PROXY_REF(bmo->cb_user);
	if(self==NULL)
		return -1;

	switch(subtype) {
		case MATHUTILS_VEC_CB_LINV:
			self->m_linear_velocity.getValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_ANGV:
			self->m_angular_velocity.getValue(bmo->data);
			break;
	}

	return 0;
}

static int mathutils_obactu_vector_set(BaseMathObject *bmo, int subtype)
{
	KX_ObjectActuator* self= static_cast<KX_ObjectActuator*>BGE_PROXY_REF(bmo->cb_user);
	if(self==NULL)
		return -1;

	switch(subtype) {
		case MATHUTILS_VEC_CB_LINV:
			self->m_linear_velocity.setValue(bmo->data);
			break;
		case MATHUTILS_VEC_CB_ANGV:
			self->m_angular_velocity.setValue(bmo->data);
			break;
	}

	return 0;
}

static int mathutils_obactu_vector_get_index(BaseMathObject *bmo, int subtype, int index)
{
	/* lazy, avoid repeteing the case statement */
	if(mathutils_obactu_vector_get(bmo, subtype) == -1)
		return -1;
	return 0;
}

static int mathutils_obactu_vector_set_index(BaseMathObject *bmo, int subtype, int index)
{
	float f= bmo->data[index];

	/* lazy, avoid repeteing the case statement */
	if(mathutils_obactu_vector_get(bmo, subtype) == -1)
		return -1;

	bmo->data[index]= f;
	return mathutils_obactu_vector_set(bmo, subtype);
}

Mathutils_Callback mathutils_obactu_vector_cb = {
	mathutils_obactu_generic_check,
	mathutils_obactu_vector_get,
	mathutils_obactu_vector_set,
	mathutils_obactu_vector_get_index,
	mathutils_obactu_vector_set_index
};

PyObject* KX_ObjectActuator::pyattr_get_linV(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return Vector_CreatePyObject_cb(BGE_PROXY_FROM_REF(self_v), 3, mathutils_kxobactu_vector_cb_index, MATHUTILS_VEC_CB_LINV);
}

int KX_ObjectActuator::pyattr_set_linV(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_ObjectActuator* self= static_cast<KX_ObjectActuator*>(self_v);
	if (!PyVecTo(value, self->m_linear_velocity))
		return PY_SET_ATTR_FAIL;

	self->UpdateFuzzyFlags();

	return PY_SET_ATTR_SUCCESS;
}

PyObject* KX_ObjectActuator::pyattr_get_angV(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return Vector_CreatePyObject_cb(BGE_PROXY_FROM_REF(self_v), 3, mathutils_kxobactu_vector_cb_index, MATHUTILS_VEC_CB_ANGV);
}

int KX_ObjectActuator::pyattr_set_angV(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_ObjectActuator* self= static_cast<KX_ObjectActuator*>(self_v);
	if (!PyVecTo(value, self->m_angular_velocity))
		return PY_SET_ATTR_FAIL;

	self->UpdateFuzzyFlags();

	return PY_SET_ATTR_SUCCESS;
}


void KX_ObjectActuator_Mathutils_Callback_Init(void)
{
	// register mathutils callbacks, ok to run more then once.
	mathutils_kxobactu_vector_cb_index = Mathutils_RegisterCallback(&mathutils_obactu_vector_cb);
}

#endif // USE_MATHUTILS

PyObject* KX_ObjectActuator::pyattr_get_forceLimitX(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_ObjectActuator* self = reinterpret_cast<KX_ObjectActuator*>(self_v);
	PyObject *retVal = PyList_New(3);

	PyList_SET_ITEM(retVal, 0, PyFloat_FromDouble(self->m_drot[0]));
	PyList_SET_ITEM(retVal, 1, PyFloat_FromDouble(self->m_dloc[0]));
	PyList_SET_ITEM(retVal, 2, PyBool_FromLong(self->m_bitLocalFlag.Torque));
	
	return retVal;
}

int KX_ObjectActuator::pyattr_set_forceLimitX(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_ObjectActuator* self = reinterpret_cast<KX_ObjectActuator*>(self_v);

	PyObject* seq = PySequence_Fast(value, "");
	if (seq && PySequence_Fast_GET_SIZE(seq) == 3)
	{
		self->m_drot[0] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 0));
		self->m_dloc[0] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 1));
		self->m_bitLocalFlag.Torque = (PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, 2)) != 0);

		if (!PyErr_Occurred())
		{
			Py_DECREF(seq);
			return PY_SET_ATTR_SUCCESS;
		}
	}

	Py_XDECREF(seq);

	PyErr_SetString(PyExc_ValueError, "expected a sequence of 2 floats and a bool");
	return PY_SET_ATTR_FAIL;
}

PyObject* KX_ObjectActuator::pyattr_get_forceLimitY(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_ObjectActuator* self = reinterpret_cast<KX_ObjectActuator*>(self_v);
	PyObject *retVal = PyList_New(3);

	PyList_SET_ITEM(retVal, 0, PyFloat_FromDouble(self->m_drot[1]));
	PyList_SET_ITEM(retVal, 1, PyFloat_FromDouble(self->m_dloc[1]));
	PyList_SET_ITEM(retVal, 2, PyBool_FromLong(self->m_bitLocalFlag.DLoc));
	
	return retVal;
}

int	KX_ObjectActuator::pyattr_set_forceLimitY(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_ObjectActuator* self = reinterpret_cast<KX_ObjectActuator*>(self_v);

	PyObject* seq = PySequence_Fast(value, "");
	if (seq && PySequence_Fast_GET_SIZE(seq) == 3)
	{
		self->m_drot[1] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 0));
		self->m_dloc[1] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 1));
		self->m_bitLocalFlag.DLoc = (PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, 2)) != 0);

		if (!PyErr_Occurred())
		{
			Py_DECREF(seq);
			return PY_SET_ATTR_SUCCESS;
		}
	}

	Py_XDECREF(seq);

	PyErr_SetString(PyExc_ValueError, "expected a sequence of 2 floats and a bool");
	return PY_SET_ATTR_FAIL;
}

PyObject* KX_ObjectActuator::pyattr_get_forceLimitZ(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_ObjectActuator* self = reinterpret_cast<KX_ObjectActuator*>(self_v);
	PyObject *retVal = PyList_New(3);

	PyList_SET_ITEM(retVal, 0, PyFloat_FromDouble(self->m_drot[2]));
	PyList_SET_ITEM(retVal, 1, PyFloat_FromDouble(self->m_dloc[2]));
	PyList_SET_ITEM(retVal, 2, PyBool_FromLong(self->m_bitLocalFlag.DRot));
	
	return retVal;
}

int	KX_ObjectActuator::pyattr_set_forceLimitZ(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_ObjectActuator* self = reinterpret_cast<KX_ObjectActuator*>(self_v);

	PyObject* seq = PySequence_Fast(value, "");
	if (seq && PySequence_Fast_GET_SIZE(seq) == 3)
	{
		self->m_drot[2] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 0));
		self->m_dloc[2] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, 1));
		self->m_bitLocalFlag.DRot = (PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, 2)) != 0);

		if (!PyErr_Occurred())
		{
			Py_DECREF(seq);
			return PY_SET_ATTR_SUCCESS;
		}
	}

	Py_XDECREF(seq);

	PyErr_SetString(PyExc_ValueError, "expected a sequence of 2 floats and a bool");
	return PY_SET_ATTR_FAIL;
}

PyObject* KX_ObjectActuator::pyattr_get_reference(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_ObjectActuator* actuator = static_cast<KX_ObjectActuator*>(self);
	if (!actuator->m_reference)
		Py_RETURN_NONE;
	
	return actuator->m_reference->GetProxy();
}

int KX_ObjectActuator::pyattr_set_reference(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_ObjectActuator* actuator = static_cast<KX_ObjectActuator*>(self);
	KX_GameObject *refOb;
	
	if (!ConvertPythonToGameObject(value, &refOb, true, "actu.reference = value: KX_ObjectActuator"))
		return PY_SET_ATTR_FAIL;
	
	if (actuator->m_reference)
		actuator->m_reference->UnregisterActuator(actuator);
	
	if(refOb==NULL) {
		actuator->m_reference= NULL;
	}
	else {	
		actuator->m_reference = refOb;
		actuator->m_reference->RegisterActuator(actuator);
	}
	
	return PY_SET_ATTR_SUCCESS;
}

#endif // WITH_PYTHON

/* eof */
