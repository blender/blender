/**
 * Apply a constraint to a position or rotation value
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

#include "SCA_IActuator.h"
#include "KX_ConstraintActuator.h"
#include "SCA_IObject.h"
#include "MT_Point3.h"
#include "MT_Matrix3x3.h"
#include "KX_GameObject.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_ConstraintActuator::KX_ConstraintActuator(SCA_IObject *gameobj, 
											 int dampTime,
											 float minBound,
											 float maxBound,
											 int locrotxyz,
											 PyTypeObject* T)
	: SCA_IActuator(gameobj, T)
{
	m_dampTime = dampTime;
	m_locrot   = locrotxyz;
	/* The units of bounds are determined by the type of constraint. To      */
	/* make the constraint application easier and more transparent later on, */
	/* I think converting the bounds to the applicable domain makes more     */
	/* sense.                                                                */
	switch (m_locrot) {
	case KX_ACT_CONSTRAINT_LOCX:
	case KX_ACT_CONSTRAINT_LOCY:
	case KX_ACT_CONSTRAINT_LOCZ:
		m_minimumBound = minBound;
		m_maximumBound = maxBound;
		break;
	case KX_ACT_CONSTRAINT_ROTX:
	case KX_ACT_CONSTRAINT_ROTY:
	case KX_ACT_CONSTRAINT_ROTZ:
		/* The user interface asks for degrees, we are radian.               */ 
		m_minimumBound = MT_radians(minBound);
		m_maximumBound = MT_radians(maxBound);
		break;
	default:
		; /* error */
	}

} /* End of constructor */

KX_ConstraintActuator::~KX_ConstraintActuator()
{ 
	// there's nothing to be done here, really....
} /* end of destructor */

bool KX_ConstraintActuator::Update(double curtime, bool frame)
{

	bool result = false;	
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events

	/* Constraint clamps the values to the specified range, with a sort of    */
	/* low-pass filtered time response, if the damp time is unequal to 0.     */

	/* Having to retrieve location/rotation and setting it afterwards may not */
	/* be efficient enough... Somthing to look at later.                      */
	KX_GameObject  *parent = (KX_GameObject*) GetParent();
	MT_Point3    position = parent->NodeGetWorldPosition();
	MT_Matrix3x3 rotation = parent->NodeGetWorldOrientation();
//	MT_Vector3	eulerrot = rotation.getEuler();
	
	switch (m_locrot) {
	case KX_ACT_CONSTRAINT_LOCX:
		Clamp(position[0], m_minimumBound, m_maximumBound);
		result = true;
		break;
	case KX_ACT_CONSTRAINT_LOCY:
		Clamp(position[1], m_minimumBound, m_maximumBound);
		result = true;
		break;
	case KX_ACT_CONSTRAINT_LOCZ:
		Clamp(position[2], m_minimumBound, m_maximumBound);
		result = true;
		break;
	
//	case KX_ACT_CONSTRAINT_ROTX:
//		/* The angles are Euler angles (I think that's what they are called) */
//		/* but we need to convert from/to the MT_Matrix3x3.                  */
//		Clamp(eulerrot[0], m_minimumBound, m_maximumBound);
//		break;
//	case KX_ACT_CONSTRAINT_ROTY:
//		Clamp(eulerrot[1], m_minimumBound, m_maximumBound);
//		break;
//	case KX_ACT_CONSTRAINT_ROTZ:
//		Clamp(eulerrot[2], m_minimumBound, m_maximumBound);
//		break;
//	default:
//		; /* error */
	}

	/* Will be replaced by a filtered clamp. */
	

	switch (m_locrot) {
	case KX_ACT_CONSTRAINT_LOCX:
	case KX_ACT_CONSTRAINT_LOCY:
	case KX_ACT_CONSTRAINT_LOCZ:
		parent->NodeSetLocalPosition(position);
		break;


//	case KX_ACT_CONSTRAINT_ROTX:
//	case KX_ACT_CONSTRAINT_ROTY:
//	case KX_ACT_CONSTRAINT_ROTZ:
//		rotation.setEuler(eulerrot);
//		parent->NodeSetLocalOrientation(rotation);
		break;

	default:
		; /* error */
	}

	return result;
} /* end of KX_ConstraintActuator::Update(double curtime,double deltatime)   */

void KX_ConstraintActuator::Clamp(MT_Scalar &var, 
								  float min, 
								  float max) {
	if (var < min) {
		var = min;
	} else if (var > max) {
		var = max;
	}
}


bool KX_ConstraintActuator::IsValidMode(KX_ConstraintActuator::KX_CONSTRAINTTYPE m) 
{
	bool res = false;

	if ( (m > KX_ACT_CONSTRAINT_NODEF) && (m < KX_ACT_CONSTRAINT_MAX)) {
		res = true;
	}

	return res;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_ConstraintActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_ConstraintActuator",
	sizeof(KX_ConstraintActuator),
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

PyParentObject KX_ConstraintActuator::Parents[] = {
	&KX_ConstraintActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_ConstraintActuator::Methods[] = {
	{"setDamp", (PyCFunction) KX_ConstraintActuator::sPySetDamp, METH_VARARGS, SetDamp_doc},
	{"getDamp", (PyCFunction) KX_ConstraintActuator::sPyGetDamp, METH_VARARGS, GetDamp_doc},
	{"setMin", (PyCFunction) KX_ConstraintActuator::sPySetMin, METH_VARARGS, SetMin_doc},
	{"getMin", (PyCFunction) KX_ConstraintActuator::sPyGetMin, METH_VARARGS, GetMin_doc},
	{"setMax", (PyCFunction) KX_ConstraintActuator::sPySetMax, METH_VARARGS, SetMax_doc},
	{"getMax", (PyCFunction) KX_ConstraintActuator::sPyGetMax, METH_VARARGS, GetMax_doc},
	{"setLimit", (PyCFunction) KX_ConstraintActuator::sPySetLimit, METH_VARARGS, SetLimit_doc},
	{"getLimit", (PyCFunction) KX_ConstraintActuator::sPyGetLimit, METH_VARARGS, GetLimit_doc},
	{NULL,NULL} //Sentinel
};

PyObject* KX_ConstraintActuator::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IActuator);
}

/* 2. setDamp                                                                */
char KX_ConstraintActuator::SetDamp_doc[] = 
"setDamp(duration)\n"
"\t- duration: integer\n"
"\tSets the time with which the constraint application is delayed.\n"
"\tIf the duration is negative, it is set to 0.\n";
PyObject* KX_ConstraintActuator::PySetDamp(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds) {
	int dampArg;
	if(!PyArg_ParseTuple(args, "i", &dampArg)) {
		return NULL;		
	}
	
	m_dampTime = dampArg;
	if (m_dampTime < 0) m_dampTime = 0;

	Py_Return;
}
/* 3. getDamp                                                                */
char KX_ConstraintActuator::GetDamp_doc[] = 
"GetDamp()\n"
"\tReturns the damping time for application of the constraint.\n";
PyObject* KX_ConstraintActuator::PyGetDamp(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds){
	return PyInt_FromLong(m_dampTime);
}

/* 4. setMin                                                                 */
char KX_ConstraintActuator::SetMin_doc[] = 
"setMin(lower_bound)\n"
"\t- lower_bound: float\n"
"\tSets the lower value of the interval to which the value\n"
"\tis clipped.\n";
PyObject* KX_ConstraintActuator::PySetMin(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) {
	float minArg;
	if(!PyArg_ParseTuple(args, "f", &minArg)) {
		return NULL;		
	}

	switch (m_locrot) {
	case KX_ACT_CONSTRAINT_LOCX:
	case KX_ACT_CONSTRAINT_LOCY:
	case KX_ACT_CONSTRAINT_LOCZ:
		m_minimumBound = minArg;
		break;
	case KX_ACT_CONSTRAINT_ROTX:
	case KX_ACT_CONSTRAINT_ROTY:
	case KX_ACT_CONSTRAINT_ROTZ:
		m_minimumBound = MT_radians(minArg);
		break;
	default:
		; /* error */
	}

	Py_Return;
}
/* 5. getMin                                                                 */
char KX_ConstraintActuator::GetMin_doc[] = 
"getMin()\n"
"\tReturns the lower value of the interval to which the value\n"
"\tis clipped.\n";
PyObject* KX_ConstraintActuator::PyGetMin(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) {
	return PyFloat_FromDouble(m_minimumBound);
}

/* 6. setMax                                                                 */
char KX_ConstraintActuator::SetMax_doc[] = 
"setMax(upper_bound)\n"
"\t- upper_bound: float\n"
"\tSets the upper value of the interval to which the value\n"
"\tis clipped.\n";
PyObject* KX_ConstraintActuator::PySetMax(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds){
	float maxArg;
	if(!PyArg_ParseTuple(args, "f", &maxArg)) {
		return NULL;		
	}

	switch (m_locrot) {
	case KX_ACT_CONSTRAINT_LOCX:
	case KX_ACT_CONSTRAINT_LOCY:
	case KX_ACT_CONSTRAINT_LOCZ:
		m_maximumBound = maxArg;
		break;
	case KX_ACT_CONSTRAINT_ROTX:
	case KX_ACT_CONSTRAINT_ROTY:
	case KX_ACT_CONSTRAINT_ROTZ:
		m_maximumBound = MT_radians(maxArg);
		break;
	default:
		; /* error */
	}

	Py_Return;
}
/* 7. getMax                                                                 */
char KX_ConstraintActuator::GetMax_doc[] = 
"getMax()\n"
"\tReturns the upper value of the interval to which the value\n"
"\tis clipped.\n";
PyObject* KX_ConstraintActuator::PyGetMax(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) {
	return PyFloat_FromDouble(m_maximumBound);
}


/* This setter/getter probably for the constraint type                       */
/* 8. setLimit                                                               */
char KX_ConstraintActuator::SetLimit_doc[] = 
"setLimit(type)\n"
"\t- type: KX_CONSTRAINTACT_LOCX, KX_CONSTRAINTACT_LOCY,\n"
"\t        KX_CONSTRAINTACT_LOCZ, KX_CONSTRAINTACT_ROTX,\n"
"\t        KX_CONSTRAINTACT_ROTY, or KX_CONSTRAINTACT_ROTZ.\n"
"\tSets the type of constraint.\n";
PyObject* KX_ConstraintActuator::PySetLimit(PyObject* self, 
											PyObject* args, 
											PyObject* kwds) {
	int locrotArg;
	if(!PyArg_ParseTuple(args, "i", &locrotArg)) {
		return NULL;		
	}
	
	if (IsValidMode((KX_CONSTRAINTTYPE)locrotArg)) m_locrot = locrotArg;

	Py_Return;
}
/* 9. getLimit                                                               */
char KX_ConstraintActuator::GetLimit_doc[] = 
"getLimit(type)\n"
"\tReturns the type of constraint.\n";
PyObject* KX_ConstraintActuator::PyGetLimit(PyObject* self, 
											PyObject* args, 
											PyObject* kwds) {
	return PyInt_FromLong(m_locrot);
}

/* eof */
