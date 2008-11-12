/**
 * Do translation/rotation actions
 *
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

#include "KX_ObjectActuator.h"
#include "KX_GameObject.h"
#include "KX_IPhysicsController.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_ObjectActuator::
KX_ObjectActuator(
	SCA_IObject* gameobj,
	const MT_Vector3& force,
	const MT_Vector3& torque,
	const MT_Vector3& dloc,
	const MT_Vector3& drot,
	const MT_Vector3& linV,
	const MT_Vector3& angV,
	const short damping,
	const KX_LocalFlags& flag,
	PyTypeObject* T
) : 
	SCA_IActuator(gameobj,T),
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
	m_bitLocalFlag (flag),
	m_active_combined_velocity (false),
	m_linear_damping_active(false),
	m_angular_damping_active(false),
	m_error_accumulator(0.0,0.0,0.0),
	m_previous_error(0.0,0.0,0.0)
{
	if (m_bitLocalFlag.ServoControl)
	{
		// in servo motion, the force is local if the target velocity is local
		m_bitLocalFlag.Force = m_bitLocalFlag.LinearVelocity;
	}
	UpdateFuzzyFlags();
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
			MT_Vector3 e = m_linear_velocity - v;
			MT_Vector3 dv = e - m_previous_error;
			MT_Vector3 I = m_error_accumulator + e;

			m_force = m_torque.x()*e+m_torque.y()*I+m_torque.z()*dv;
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

	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



/* some 'standard' utilities... */
bool KX_ObjectActuator::isValid(KX_ObjectActuator::KX_OBJECT_ACT_VEC_TYPE type)
{
	bool res = false;
	res = (type > KX_OBJECT_ACT_NODEF) && (type < KX_OBJECT_ACT_MAX);
	return res;
}



/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_ObjectActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_ObjectActuator",
	sizeof(KX_ObjectActuator),
	0,
	PyDestructor,
	0,
	__getattr,
	__setattr,
	0, //&MyPyCompare,
	__repr,
	0, //&cvalue_as_number,
	0,
	0,
	0,
	0
};

PyParentObject KX_ObjectActuator::Parents[] = {
	&KX_ObjectActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_ObjectActuator::Methods[] = {
	{"getForce", (PyCFunction) KX_ObjectActuator::sPyGetForce, METH_NOARGS},
	{"setForce", (PyCFunction) KX_ObjectActuator::sPySetForce, METH_VARARGS},
	{"getTorque", (PyCFunction) KX_ObjectActuator::sPyGetTorque, METH_NOARGS},
	{"setTorque", (PyCFunction) KX_ObjectActuator::sPySetTorque, METH_VARARGS},
	{"getDLoc", (PyCFunction) KX_ObjectActuator::sPyGetDLoc, METH_NOARGS},
	{"setDLoc", (PyCFunction) KX_ObjectActuator::sPySetDLoc, METH_VARARGS},
	{"getDRot", (PyCFunction) KX_ObjectActuator::sPyGetDRot, METH_NOARGS},
	{"setDRot", (PyCFunction) KX_ObjectActuator::sPySetDRot, METH_VARARGS},
	{"getLinearVelocity", (PyCFunction) KX_ObjectActuator::sPyGetLinearVelocity, METH_NOARGS},
	{"setLinearVelocity", (PyCFunction) KX_ObjectActuator::sPySetLinearVelocity, METH_VARARGS},
	{"getAngularVelocity", (PyCFunction) KX_ObjectActuator::sPyGetAngularVelocity, METH_NOARGS},
	{"setAngularVelocity", (PyCFunction) KX_ObjectActuator::sPySetAngularVelocity, METH_VARARGS},
	{"setDamping", (PyCFunction) KX_ObjectActuator::sPySetDamping, METH_VARARGS},
	{"getDamping", (PyCFunction) KX_ObjectActuator::sPyGetDamping, METH_NOARGS},
	{"setForceLimitX", (PyCFunction) KX_ObjectActuator::sPySetForceLimitX, METH_VARARGS},
	{"getForceLimitX", (PyCFunction) KX_ObjectActuator::sPyGetForceLimitX, METH_NOARGS},
	{"setForceLimitY", (PyCFunction) KX_ObjectActuator::sPySetForceLimitY, METH_VARARGS},
	{"getForceLimitY", (PyCFunction) KX_ObjectActuator::sPyGetForceLimitY, METH_NOARGS},
	{"setForceLimitZ", (PyCFunction) KX_ObjectActuator::sPySetForceLimitZ, METH_VARARGS},
	{"getForceLimitZ", (PyCFunction) KX_ObjectActuator::sPyGetForceLimitZ, METH_NOARGS},
	{"setPID", (PyCFunction) KX_ObjectActuator::sPyGetPID, METH_NOARGS},
	{"getPID", (PyCFunction) KX_ObjectActuator::sPySetPID, METH_VARARGS},



	{NULL,NULL} //Sentinel
};

PyObject* KX_ObjectActuator::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IActuator);
};

/* 1. set ------------------------------------------------------------------ */
/* Removed! */

/* 2. getForce                                                               */
PyObject* KX_ObjectActuator::PyGetForce(PyObject* self)
{
	PyObject *retVal = PyList_New(4);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_force[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_force[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_force[2]));
	PyList_SetItem(retVal, 3, BoolToPyArg(m_bitLocalFlag.Force));
	
	return retVal;
}
/* 3. setForce                                                               */
PyObject* KX_ObjectActuator::PySetForce(PyObject* self, 
										PyObject* args, 
										PyObject* kwds)
{
	float vecArg[3];
	int bToggle = 0;
	if (!PyArg_ParseTuple(args, "fffi", &vecArg[0], &vecArg[1], 
						  &vecArg[2], &bToggle)) {
		return NULL;
	}
	m_force.setValue(vecArg);
	m_bitLocalFlag.Force = PyArgToBool(bToggle);
	UpdateFuzzyFlags();
	Py_Return;
}

/* 4. getTorque                                                              */
PyObject* KX_ObjectActuator::PyGetTorque(PyObject* self)
{
	PyObject *retVal = PyList_New(4);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_torque[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_torque[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_torque[2]));
	PyList_SetItem(retVal, 3, BoolToPyArg(m_bitLocalFlag.Torque));
	
	return retVal;
}
/* 5. setTorque                                                              */
PyObject* KX_ObjectActuator::PySetTorque(PyObject* self, 
										 PyObject* args, 
										 PyObject* kwds)
{
	float vecArg[3];
	int bToggle = 0;
	if (!PyArg_ParseTuple(args, "fffi", &vecArg[0], &vecArg[1], 
						  &vecArg[2], &bToggle)) {
		return NULL;
	}
	m_torque.setValue(vecArg);
	m_bitLocalFlag.Torque = PyArgToBool(bToggle);
	UpdateFuzzyFlags();
	Py_Return;
}

/* 6. getDLoc                                                                */
PyObject* KX_ObjectActuator::PyGetDLoc(PyObject* self)
{
	PyObject *retVal = PyList_New(4);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_dloc[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_dloc[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_dloc[2]));
	PyList_SetItem(retVal, 3, BoolToPyArg(m_bitLocalFlag.DLoc));
	
	return retVal;
}
/* 7. setDLoc                                                                */
PyObject* KX_ObjectActuator::PySetDLoc(PyObject* self, 
									   PyObject* args, 
									   PyObject* kwds)
{
	float vecArg[3];
	int bToggle = 0;
	if(!PyArg_ParseTuple(args, "fffi", &vecArg[0], &vecArg[1], 
						 &vecArg[2], &bToggle)) {
		return NULL;
	}
	m_dloc.setValue(vecArg);
	m_bitLocalFlag.DLoc = PyArgToBool(bToggle);
	UpdateFuzzyFlags();
	Py_Return;
}

/* 8. getDRot                                                                */
PyObject* KX_ObjectActuator::PyGetDRot(PyObject* self)
{
	PyObject *retVal = PyList_New(4);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_drot[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_drot[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_drot[2]));
	PyList_SetItem(retVal, 3, BoolToPyArg(m_bitLocalFlag.DRot));
	
	return retVal;
}
/* 9. setDRot                                                                */
PyObject* KX_ObjectActuator::PySetDRot(PyObject* self, 
									   PyObject* args, 
									   PyObject* kwds)
{
	float vecArg[3];
	int bToggle = 0;
	if (!PyArg_ParseTuple(args, "fffi", &vecArg[0], &vecArg[1], 
						  &vecArg[2], &bToggle)) {
		return NULL;
	}
	m_drot.setValue(vecArg);
	m_bitLocalFlag.DRot = PyArgToBool(bToggle);
	UpdateFuzzyFlags();
	Py_Return;
}

/* 10. getLinearVelocity                                                 */
PyObject* KX_ObjectActuator::PyGetLinearVelocity(PyObject* self) {
	PyObject *retVal = PyList_New(4);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_linear_velocity[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_linear_velocity[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_linear_velocity[2]));
	PyList_SetItem(retVal, 3, BoolToPyArg(m_bitLocalFlag.LinearVelocity));
	
	return retVal;
}

/* 11. setLinearVelocity                                                 */
PyObject* KX_ObjectActuator::PySetLinearVelocity(PyObject* self, 
												 PyObject* args, 
												 PyObject* kwds) {
	float vecArg[3];
	int bToggle = 0;
	if (!PyArg_ParseTuple(args, "fffi", &vecArg[0], &vecArg[1], 
						  &vecArg[2], &bToggle)) {
		return NULL;
	}
	m_linear_velocity.setValue(vecArg);
	m_bitLocalFlag.LinearVelocity = PyArgToBool(bToggle);
	UpdateFuzzyFlags();
	Py_Return;
}


/* 12. getAngularVelocity                                                */
PyObject* KX_ObjectActuator::PyGetAngularVelocity(PyObject* self) {
	PyObject *retVal = PyList_New(4);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_angular_velocity[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_angular_velocity[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_angular_velocity[2]));
	PyList_SetItem(retVal, 3, BoolToPyArg(m_bitLocalFlag.AngularVelocity));
	
	return retVal;
}
/* 13. setAngularVelocity                                                */
PyObject* KX_ObjectActuator::PySetAngularVelocity(PyObject* self, 
												  PyObject* args, 
												  PyObject* kwds) {
	float vecArg[3];
	int bToggle = 0;
	if (!PyArg_ParseTuple(args, "fffi", &vecArg[0], &vecArg[1], 
						  &vecArg[2], &bToggle)) {
		return NULL;
	}
	m_angular_velocity.setValue(vecArg);
	m_bitLocalFlag.AngularVelocity = PyArgToBool(bToggle);
	UpdateFuzzyFlags();
	Py_Return;
}

/* 13. setDamping                                                */
PyObject* KX_ObjectActuator::PySetDamping(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) {
	int damping = 0;
	if (!PyArg_ParseTuple(args, "i", &damping) || damping < 0 || damping > 1000) {
		return NULL;
	}
	m_damping = damping;
	Py_Return;
}

/* 13. getVelocityDamping                                                */
PyObject* KX_ObjectActuator::PyGetDamping(PyObject* self) {
	return Py_BuildValue("i",m_damping);
}
/* 6. getForceLimitX                                                                */
PyObject* KX_ObjectActuator::PyGetForceLimitX(PyObject* self)
{
	PyObject *retVal = PyList_New(3);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_drot[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_dloc[0]));
	PyList_SetItem(retVal, 2, BoolToPyArg(m_bitLocalFlag.Torque));
	
	return retVal;
}
/* 7. setForceLimitX                                                         */
PyObject* KX_ObjectActuator::PySetForceLimitX(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds)
{
	float vecArg[2];
	int bToggle = 0;
	if(!PyArg_ParseTuple(args, "ffi", &vecArg[0], &vecArg[1], &bToggle)) {
		return NULL;
	}
	m_drot[0] = vecArg[0];
	m_dloc[0] = vecArg[1];
	m_bitLocalFlag.Torque = PyArgToBool(bToggle);
	Py_Return;
}

/* 6. getForceLimitY                                                                */
PyObject* KX_ObjectActuator::PyGetForceLimitY(PyObject* self)
{
	PyObject *retVal = PyList_New(3);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_drot[1]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_dloc[1]));
	PyList_SetItem(retVal, 2, BoolToPyArg(m_bitLocalFlag.DLoc));
	
	return retVal;
}
/* 7. setForceLimitY                                                                */
PyObject* KX_ObjectActuator::PySetForceLimitY(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds)
{
	float vecArg[2];
	int bToggle = 0;
	if(!PyArg_ParseTuple(args, "ffi", &vecArg[0], &vecArg[1], &bToggle)) {
		return NULL;
	}
	m_drot[1] = vecArg[0];
	m_dloc[1] = vecArg[1];
	m_bitLocalFlag.DLoc = PyArgToBool(bToggle);
	Py_Return;
}

/* 6. getForceLimitZ                                                                */
PyObject* KX_ObjectActuator::PyGetForceLimitZ(PyObject* self)
{
	PyObject *retVal = PyList_New(3);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_drot[2]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_dloc[2]));
	PyList_SetItem(retVal, 2, BoolToPyArg(m_bitLocalFlag.DRot));
	
	return retVal;
}
/* 7. setForceLimitZ                                                                */
PyObject* KX_ObjectActuator::PySetForceLimitZ(PyObject* self, 
											  PyObject* args, 
											  PyObject* kwds)
{
	float vecArg[2];
	int bToggle = 0;
	if(!PyArg_ParseTuple(args, "ffi", &vecArg[0], &vecArg[1], &bToggle)) {
		return NULL;
	}
	m_drot[2] = vecArg[0];
	m_dloc[2] = vecArg[1];
	m_bitLocalFlag.DRot = PyArgToBool(bToggle);
	Py_Return;
}

/* 4. getPID                                                              */
PyObject* KX_ObjectActuator::PyGetPID(PyObject* self)
{
	PyObject *retVal = PyList_New(3);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_torque[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_torque[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_torque[2]));
	
	return retVal;
}
/* 5. setPID                                                              */
PyObject* KX_ObjectActuator::PySetPID(PyObject* self, 
									  PyObject* args, 
									  PyObject* kwds)
{
	float vecArg[3];
	if (!PyArg_ParseTuple(args, "fff", &vecArg[0], &vecArg[1], &vecArg[2])) {
		return NULL;
	}
	m_torque.setValue(vecArg);
	Py_Return;
}





/* eof */
