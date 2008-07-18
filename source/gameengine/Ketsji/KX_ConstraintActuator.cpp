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
#include "KX_RayCast.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_ConstraintActuator::KX_ConstraintActuator(SCA_IObject *gameobj, 
											 int posDampTime,
											 int rotDampTime,
											 float minBound,
											 float maxBound,
											 float refDir[3],
											 int locrotxyz,
											 int time,
											 int option,
											 char *property,
											 PyTypeObject* T) : 
	m_refDirection(refDir),
	m_currentTime(0),
	SCA_IActuator(gameobj, T)
{
	m_posDampTime = posDampTime;
	m_rotDampTime = rotDampTime;
	m_locrot   = locrotxyz;
	m_option = option;
	m_activeTime = time;
	if (property) {
		strncpy(m_property, property, sizeof(m_property));
		m_property[sizeof(m_property)-1] = 0;
	} else {
		m_property[0] = 0;
	}
	/* The units of bounds are determined by the type of constraint. To      */
	/* make the constraint application easier and more transparent later on, */
	/* I think converting the bounds to the applicable domain makes more     */
	/* sense.                                                                */
	switch (m_locrot) {
	case KX_ACT_CONSTRAINT_ORIX:
	case KX_ACT_CONSTRAINT_ORIY:
	case KX_ACT_CONSTRAINT_ORIZ:
		{
			MT_Scalar len = m_refDirection.length();
			if (MT_fuzzyZero(len)) {
				// missing a valid direction
				std::cout << "WARNING: Constraint actuator " << GetName() << ":  There is no valid reference direction!" << std::endl;
				m_locrot = KX_ACT_CONSTRAINT_NODEF;
			} else {
				m_refDirection /= len;
			}
		}
		break;
	default:
		m_minimumBound = minBound;
		m_maximumBound = maxBound;
		break;
	}

} /* End of constructor */

KX_ConstraintActuator::~KX_ConstraintActuator()
{ 
	// there's nothing to be done here, really....
} /* end of destructor */

bool KX_ConstraintActuator::RayHit(KX_ClientObjectInfo* client, MT_Point3& hit_point, MT_Vector3& hit_normal, void * const data)
{

	KX_GameObject* hitKXObj = client->m_gameobject;
	
	if (client->m_type > KX_ClientObjectInfo::ACTOR)
	{
		// false hit
		return false;
	}
	bool bFound = false;

	if (m_property[0] == 0)
	{
		bFound = true;
	}
	else
	{
		if (m_option & KX_ACT_CONSTRAINT_MATERIAL)
		{
			if (client->m_auxilary_info)
			{
				bFound = !strcmp(m_property, ((char*)client->m_auxilary_info));
			}
		}
		else
		{
			bFound = hitKXObj->GetProperty(m_property) != NULL;
		}
	}

	return bFound;
}

bool KX_ConstraintActuator::Update(double curtime, bool frame)
{

	bool result = false;	
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (!bNegativeEvent) {
		/* Constraint clamps the values to the specified range, with a sort of    */
		/* low-pass filtered time response, if the damp time is unequal to 0.     */

		/* Having to retrieve location/rotation and setting it afterwards may not */
		/* be efficient enough... Somthing to look at later.                      */
		KX_GameObject  *obj = (KX_GameObject*) GetParent();
		MT_Point3    position = obj->NodeGetWorldPosition();
		MT_Point3    newposition;
		MT_Vector3   direction;
		MT_Matrix3x3 rotation = obj->NodeGetWorldOrientation();
		MT_Scalar    filter, newdistance;
		int axis, sign;

		if (m_posDampTime) {
			filter = m_posDampTime/(1.0+m_posDampTime);
		}
		switch (m_locrot) {
		case KX_ACT_CONSTRAINT_ORIX:
		case KX_ACT_CONSTRAINT_ORIY:
		case KX_ACT_CONSTRAINT_ORIZ:
			switch (m_locrot) {
			case KX_ACT_CONSTRAINT_ORIX:
				direction[0] = rotation[0][0];
				direction[1] = rotation[1][0];
				direction[2] = rotation[2][0];
				axis = 0;
				break;
			case KX_ACT_CONSTRAINT_ORIY:
				direction[0] = rotation[0][1];
				direction[1] = rotation[1][1];
				direction[2] = rotation[2][1];
				axis = 1;
				break;
			case KX_ACT_CONSTRAINT_ORIZ:
				direction[0] = rotation[0][2];
				direction[1] = rotation[1][2];
				direction[2] = rotation[2][2];
				axis = 2;
				break;
			}
			// apply damping on the direction
			if (m_posDampTime) {
				direction = filter*direction + (1.0-filter)*m_refDirection;
			}
			obj->AlignAxisToVect(direction, axis);
			result = true;
			goto CHECK_TIME;
		case KX_ACT_CONSTRAINT_DIRPX:
		case KX_ACT_CONSTRAINT_DIRPY:
		case KX_ACT_CONSTRAINT_DIRPZ:
		case KX_ACT_CONSTRAINT_DIRMX:
		case KX_ACT_CONSTRAINT_DIRMY:
		case KX_ACT_CONSTRAINT_DIRMZ:
			switch (m_locrot) {
			case KX_ACT_CONSTRAINT_DIRPX:
				direction[0] = rotation[0][0];
				direction[1] = rotation[1][0];
				direction[2] = rotation[2][0];
				axis = 0;		// axis according to KX_GameObject::AlignAxisToVect()
				sign = 1;		// X axis will be anti parrallel to normal
				break;
			case KX_ACT_CONSTRAINT_DIRPY:
				direction[0] = rotation[0][1];
				direction[1] = rotation[1][1];
				direction[2] = rotation[2][1];
				axis = 1;
				sign = 1;
				break;
			case KX_ACT_CONSTRAINT_DIRPZ:
				direction[0] = rotation[0][2];
				direction[1] = rotation[1][2];
				direction[2] = rotation[2][2];
				axis = 2;
				sign = 1;
				break;
			case KX_ACT_CONSTRAINT_DIRMX:
				direction[0] = -rotation[0][0];
				direction[1] = -rotation[1][0];
				direction[2] = -rotation[2][0];
				axis = 0;
				sign = 0;
				break;
			case KX_ACT_CONSTRAINT_DIRMY:
				direction[0] = -rotation[0][1];
				direction[1] = -rotation[1][1];
				direction[2] = -rotation[2][1];
				axis = 1;
				sign = 0;
				break;
			case KX_ACT_CONSTRAINT_DIRMZ:
				direction[0] = -rotation[0][2];
				direction[1] = -rotation[1][2];
				direction[2] = -rotation[2][2];
				axis = 2;
				sign = 0;
				break;
			}
			direction.normalize();
			{
				MT_Point3 topoint = position + (m_maximumBound) * direction;
				MT_Point3 resultpoint;
				MT_Vector3 resultnormal;
				PHY_IPhysicsEnvironment* pe = obj->GetPhysicsEnvironment();
				KX_IPhysicsController *spc = obj->GetPhysicsController();

				if (!pe) {
					std::cout << "WARNING: Constraint actuator " << GetName() << ":  There is no physics environment!" << std::endl;
					goto CHECK_TIME;
				}	 
				if (!spc) {
					// the object is not physical, we probably want to avoid hitting its own parent
					KX_GameObject *parent = obj->GetParent();
					if (parent) {
						spc = parent->GetPhysicsController();
						parent->Release();
					}
				}
				result = KX_RayCast::RayTest(spc, pe, position, topoint, resultpoint, resultnormal, KX_RayCast::Callback<KX_ConstraintActuator>(this));

				if (result)	{
					// compute new position & orientation
					if ((m_option & (KX_ACT_CONSTRAINT_NORMAL|KX_ACT_CONSTRAINT_DISTANCE)) == 0) {
						// if none option is set, the actuator does nothing but detect ray 
						// (works like a sensor)
						goto CHECK_TIME;
					}
					if (m_option & KX_ACT_CONSTRAINT_NORMAL) {
						// the new orientation must be so that the axis is parallel to normal
						if (sign)
							resultnormal = -resultnormal;
						// apply damping on the direction
						if (m_rotDampTime) {
							MT_Scalar rotFilter = 1.0/(1.0+m_rotDampTime);
							resultnormal = (-m_rotDampTime*rotFilter)*direction + rotFilter*resultnormal;
						} else if (m_posDampTime) {
							resultnormal = -filter*direction + (1.0-filter)*resultnormal;
						}
						obj->AlignAxisToVect(resultnormal, axis);
						direction = -resultnormal;
					}
					if (m_option & KX_ACT_CONSTRAINT_DISTANCE) {
						if (m_posDampTime) {
							newdistance = filter*(position-resultpoint).length()+(1.0-filter)*m_minimumBound;
						} else {
							newdistance = m_minimumBound;
						}
					} else {
						newdistance = (position-resultpoint).length();
					}
					newposition = resultpoint-newdistance*direction;
				} else if (m_option & KX_ACT_CONSTRAINT_PERMANENT) {
					// no contact but still keep running
					result = true;
					goto CHECK_TIME;
				}
			}
			break; 
		case KX_ACT_CONSTRAINT_LOCX:
		case KX_ACT_CONSTRAINT_LOCY:
		case KX_ACT_CONSTRAINT_LOCZ:
			newposition = position;
			switch (m_locrot) {
			case KX_ACT_CONSTRAINT_LOCX:
				Clamp(newposition[0], m_minimumBound, m_maximumBound);
				break;
			case KX_ACT_CONSTRAINT_LOCY:
				Clamp(newposition[1], m_minimumBound, m_maximumBound);
				break;
			case KX_ACT_CONSTRAINT_LOCZ:
				Clamp(newposition[2], m_minimumBound, m_maximumBound);
				break;
			}
			result = true;
			if (m_posDampTime) {
				newposition = filter*position + (1.0-filter)*newposition;
			}
			break;
		}
		if (result) {
			// set the new position but take into account parent if any
			obj->NodeSetWorldPosition(newposition);
		}
	CHECK_TIME:
		if (result && m_activeTime > 0 ) {
			if (++m_currentTime >= m_activeTime)
				result = false;
		}
	}
	if (!result) {
		m_currentTime = 0;
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
	{"setRotDamp", (PyCFunction) KX_ConstraintActuator::sPySetRotDamp, METH_VARARGS, SetRotDamp_doc},
	{"getRotDamp", (PyCFunction) KX_ConstraintActuator::sPyGetRotDamp, METH_VARARGS, GetRotDamp_doc},
	{"setDirection", (PyCFunction) KX_ConstraintActuator::sPySetDirection, METH_VARARGS, SetDirection_doc},
	{"getDirection", (PyCFunction) KX_ConstraintActuator::sPyGetDirection, METH_VARARGS, GetDirection_doc},
	{"setOption", (PyCFunction) KX_ConstraintActuator::sPySetOption, METH_VARARGS, SetOption_doc},
	{"getOption", (PyCFunction) KX_ConstraintActuator::sPyGetOption, METH_VARARGS, GetOption_doc},
	{"setTime", (PyCFunction) KX_ConstraintActuator::sPySetTime, METH_VARARGS, SetTime_doc},
	{"getTime", (PyCFunction) KX_ConstraintActuator::sPyGetTime, METH_VARARGS, GetTime_doc},
	{"setProperty", (PyCFunction) KX_ConstraintActuator::sPySetProperty, METH_VARARGS, SetProperty_doc},
	{"getProperty", (PyCFunction) KX_ConstraintActuator::sPyGetProperty, METH_VARARGS, GetProperty_doc},
	{"setMin", (PyCFunction) KX_ConstraintActuator::sPySetMin, METH_VARARGS, SetMin_doc},
	{"getMin", (PyCFunction) KX_ConstraintActuator::sPyGetMin, METH_VARARGS, GetMin_doc},
	{"setDistance", (PyCFunction) KX_ConstraintActuator::sPySetMin, METH_VARARGS, SetDistance_doc},
	{"getDistance", (PyCFunction) KX_ConstraintActuator::sPyGetMin, METH_VARARGS, GetDistance_doc},
	{"setMax", (PyCFunction) KX_ConstraintActuator::sPySetMax, METH_VARARGS, SetMax_doc},
	{"getMax", (PyCFunction) KX_ConstraintActuator::sPyGetMax, METH_VARARGS, GetMax_doc},
	{"setRayLength", (PyCFunction) KX_ConstraintActuator::sPySetMax, METH_VARARGS, SetRayLength_doc},
	{"getRayLength", (PyCFunction) KX_ConstraintActuator::sPyGetMax, METH_VARARGS, GetRayLength_doc},
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
"\tSets the time constant of the orientation and distance constraint.\n"
"\tIf the duration is negative, it is set to 0.\n";
PyObject* KX_ConstraintActuator::PySetDamp(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds) {
	int dampArg;
	if(!PyArg_ParseTuple(args, "i", &dampArg)) {
		return NULL;		
	}
	
	m_posDampTime = dampArg;
	if (m_posDampTime < 0) m_posDampTime = 0;

	Py_Return;
}
/* 3. getDamp                                                                */
char KX_ConstraintActuator::GetDamp_doc[] = 
"getDamp()\n"
"\tReturns the damping parameter.\n";
PyObject* KX_ConstraintActuator::PyGetDamp(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds){
	return PyInt_FromLong(m_posDampTime);
}

/* 2. setRotDamp                                                                */
char KX_ConstraintActuator::SetRotDamp_doc[] = 
"setRotDamp(duration)\n"
"\t- duration: integer\n"
"\tSets the time constant of the orientation constraint.\n"
"\tIf the duration is negative, it is set to 0.\n";
PyObject* KX_ConstraintActuator::PySetRotDamp(PyObject* self, 
										      PyObject* args, 
										      PyObject* kwds) {
	int dampArg;
	if(!PyArg_ParseTuple(args, "i", &dampArg)) {
		return NULL;		
	}
	
	m_rotDampTime = dampArg;
	if (m_rotDampTime < 0) m_rotDampTime = 0;

	Py_Return;
}
/* 3. getRotDamp                                                                */
char KX_ConstraintActuator::GetRotDamp_doc[] = 
"getRotDamp()\n"
"\tReturns the damping time for application of the constraint.\n";
PyObject* KX_ConstraintActuator::PyGetRotDamp(PyObject* self, 
										      PyObject* args, 
										      PyObject* kwds){
	return PyInt_FromLong(m_rotDampTime);
}

/* 2. setDirection                                                                */
char KX_ConstraintActuator::SetDirection_doc[] = 
"setDirection(vector)\n"
"\t- vector: 3-tuple\n"
"\tSets the reference direction in world coordinate for the orientation constraint.\n";
PyObject* KX_ConstraintActuator::PySetDirection(PyObject* self, 
										        PyObject* args, 
										        PyObject* kwds) {
	float x, y, z;
	MT_Scalar len;
	MT_Vector3 dir;

	if(!PyArg_ParseTuple(args, "(fff)", &x, &y, &z)) {
		return NULL;		
	}
	dir[0] = x;
	dir[1] = y;
	dir[2] = z;
	len = dir.length();
	if (MT_fuzzyZero(len)) {
		std::cout << "Invalid direction" << std::endl;
		return NULL;
	}
	m_refDirection = dir/len;

	Py_Return;
}
/* 3. getDirection                                                                */
char KX_ConstraintActuator::GetDirection_doc[] = 
"getDirection()\n"
"\tReturns the reference direction of the orientation constraint as a 3-tuple.\n";
PyObject* KX_ConstraintActuator::PyGetDirection(PyObject* self, 
										        PyObject* args, 
										        PyObject* kwds){
	PyObject *retVal = PyList_New(3);

	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_refDirection[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_refDirection[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_refDirection[2]));
	return retVal;
}

/* 2. setOption                                                                */
char KX_ConstraintActuator::SetOption_doc[] = 
"setOption(option)\n"
"\t- option: integer\n"
"\tSets several options of the distance  constraint.\n"
"\tBinary combination of the following values:\n"
"\t\t 64 : Activate alignment to surface\n"
"\t\t128 : Detect material rather than property\n"
"\t\t256 : No deactivation if ray does not hit target\n"
"\t\t512 : Activate distance control\n";
PyObject* KX_ConstraintActuator::PySetOption(PyObject* self, 
										     PyObject* args, 
										     PyObject* kwds) {
	int option;
	if(!PyArg_ParseTuple(args, "i", &option)) {
		return NULL;		
	}
	
	m_option = option;

	Py_Return;
}
/* 3. getOption                                                              */
char KX_ConstraintActuator::GetOption_doc[] = 
"getOption()\n"
"\tReturns the option parameter.\n";
PyObject* KX_ConstraintActuator::PyGetOption(PyObject* self, 
										     PyObject* args, 
										     PyObject* kwds){
	return PyInt_FromLong(m_option);
}

/* 2. setTime                                                                */
char KX_ConstraintActuator::SetTime_doc[] = 
"setTime(duration)\n"
"\t- duration: integer\n"
"\tSets the activation time of the actuator.\n"
"\tThe actuator disables itself after this many frame.\n"
"\tIf set to 0 or negative, the actuator is not limited in time.\n";
PyObject* KX_ConstraintActuator::PySetTime(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds) {
	int t;
	if(!PyArg_ParseTuple(args, "i", &t)) {
		return NULL;		
	}
	
	if (t < 0)
		t = 0;
	m_activeTime = t;

	Py_Return;
}
/* 3. getTime                                                                */
char KX_ConstraintActuator::GetTime_doc[] = 
"getTime()\n"
"\tReturns the time parameter.\n";
PyObject* KX_ConstraintActuator::PyGetTime(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds){
	return PyInt_FromLong(m_activeTime);
}

/* 2. setProperty                                                                */
char KX_ConstraintActuator::SetProperty_doc[] = 
"setProperty(property)\n"
"\t- property: string\n"
"\tSets the name of the property or material for the ray detection of the distance constraint.\n"
"\tIf empty, the ray will detect any collisioning object.\n";
PyObject* KX_ConstraintActuator::PySetProperty(PyObject* self, 
										       PyObject* args, 
										       PyObject* kwds) {
	char *property;
	if (!PyArg_ParseTuple(args, "s", &property)) {
		return NULL;
	}
	if (property == NULL) {
		m_property[0] = 0;
	} else {
		strncpy(m_property, property, sizeof(m_property));
		m_property[sizeof(m_property)-1] = 0;
	}

	Py_Return;
}
/* 3. getProperty                                                                */
char KX_ConstraintActuator::GetProperty_doc[] = 
"getProperty()\n"
"\tReturns the property parameter.\n";
PyObject* KX_ConstraintActuator::PyGetProperty(PyObject* self, 
										       PyObject* args, 
										       PyObject* kwds){
	return PyString_FromString(m_property);
}

/* 4. setDistance                                                                 */
char KX_ConstraintActuator::SetDistance_doc[] = 
"setDistance(distance)\n"
"\t- distance: float\n"
"\tSets the target distance in distance constraint\n";
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
	default:
		m_minimumBound = minArg;
		break;
	case KX_ACT_CONSTRAINT_ROTX:
	case KX_ACT_CONSTRAINT_ROTY:
	case KX_ACT_CONSTRAINT_ROTZ:
		m_minimumBound = MT_radians(minArg);
		break;
	}

	Py_Return;
}
/* 5. getDistance                                                                 */
char KX_ConstraintActuator::GetDistance_doc[] = 
"getDistance()\n"
"\tReturns the distance parameter \n";
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

/* 6. setRayLength                                                                 */
char KX_ConstraintActuator::SetRayLength_doc[] = 
"setRayLength(length)\n"
"\t- length: float\n"
"\tSets the maximum ray length of the distance constraint\n";
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
	default:
		m_maximumBound = maxArg;
		break;
	case KX_ACT_CONSTRAINT_ROTX:
	case KX_ACT_CONSTRAINT_ROTY:
	case KX_ACT_CONSTRAINT_ROTZ:
		m_maximumBound = MT_radians(maxArg);
		break;
	}

	Py_Return;
}
/* 7. getRayLength                                                                 */
char KX_ConstraintActuator::GetRayLength_doc[] = 
"getRayLength()\n"
"\tReturns the length of the ray\n";
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
"\t- type: integer\n"
"\t  1  : LocX\n"
"\t  2  : LocY\n"
"\t  3  : LocZ\n"
"\t  7  : Distance along +X axis\n"
"\t  8  : Distance along +Y axis\n"
"\t  9  : Distance along +Z axis\n"
"\t  10 : Distance along -X axis\n"
"\t  11 : Distance along -Y axis\n"
"\t  12 : Distance along -Z axis\n"
"\t  13 : Align X axis\n"
"\t  14 : Align Y axis\n"
"\t  15 : Align Z axis\n"
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
"getLimit()\n"
"\tReturns the type of constraint.\n";
PyObject* KX_ConstraintActuator::PyGetLimit(PyObject* self, 
											PyObject* args, 
											PyObject* kwds) {
	return PyInt_FromLong(m_locrot);
}

/* eof */
