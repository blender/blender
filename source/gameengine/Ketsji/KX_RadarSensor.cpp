/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include "KX_RadarSensor.h"

#include "KX_GameObject.h"
/**
 * 	RadarSensor constructor. Creates a near-sensor derived class, with a cone collision shape.
 */

#ifdef PHYSICS_NOT_YET

KX_RadarSensor::KX_RadarSensor(class SCA_EventManager* eventmgr,
		class KX_GameObject* gameobj,
			double coneradius,
			double coneheight,
			int	axis,
			double margin,
			double resetmargin,
			bool bFindMaterial,
			const STR_String& touchedpropname,
			class KX_Scene* kxscene,
			PyTypeObject* T)

			: KX_NearSensor(
				eventmgr,
				gameobj,
				margin,
				resetmargin,
				bFindMaterial,
				touchedpropname,
				kxscene,
				T),
				m_coneheight(coneheight),
				m_coneradius(coneradius),
				m_axis(axis)
{
	m_client_info.m_type = 3;
	m_client_info.m_clientobject = gameobj;
	m_client_info.m_auxilary_info = NULL;
	sumoObj->setClientObject(&m_client_info);
}
			

KX_RadarSensor::~KX_RadarSensor()
{
	
}

/**
 *	Transforms the collision object. A cone is not correctly centered
 *	for usage.  */
void KX_RadarSensor::SynchronizeTransform()
{
	// Getting the parent location was commented out. Why?
	MT_Transform trans;
	trans.setOrigin(((KX_GameObject*)GetParent())->NodeGetWorldPosition());
	trans.setBasis(((KX_GameObject*)GetParent())->NodeGetWorldOrientation());
	// What is the default orientation? pointing in the -y direction?
	// is the geometry correctly converted?

	// a collision cone is oriented
	// center the cone correctly 
	// depends on the radar 'axis'
	switch (m_axis)
	{
	case 0: // X Axis
		{
			MT_Quaternion rotquatje(MT_Vector3(0,0,1),MT_radians(90));
			trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	case 1: // Y Axis
		{
			MT_Quaternion rotquatje(MT_Vector3(1,0,0),MT_radians(-180));
			trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	case 2: // Z Axis
		{
			MT_Quaternion rotquatje(MT_Vector3(1,0,0),MT_radians(-90));
			trans.rotate(rotquatje);
			trans.translate(MT_Vector3 (0, -m_coneheight/2.0 ,0));
			break;
		};
	default:
		{
		}
	}
	m_cone_origin = trans.getOrigin();
	m_cone_target = trans(MT_Point3(0, -m_coneheight/2.0 ,0));

	m_sumoObj->setPosition(trans.getOrigin());
	m_sumoObj->setOrientation(trans.getRotation());
	m_sumoObj->calcXform();
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_RadarSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_RadarSensor",
	sizeof(KX_RadarSensor),
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

PyParentObject KX_RadarSensor::Parents[] = {
	&KX_RadarSensor::Type,
	&KX_NearSensor::Type,
	&KX_TouchSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_RadarSensor::Methods[] = {
	{"getConeOrigin", (PyCFunction) KX_RadarSensor::sPyGetConeOrigin, 
	 METH_VARARGS, GetConeOrigin_doc},
	{"getConeTarget", (PyCFunction) KX_RadarSensor::sPyGetConeTarget, 
	 METH_VARARGS, GetConeTarget_doc},
	{"getConeHeight", (PyCFunction) KX_RadarSensor::sPyGetConeHeight, 
	 METH_VARARGS, GetConeHeight_doc},
	{NULL,NULL,NULL,NULL} //Sentinel
};

PyObject* KX_RadarSensor::_getattr(char* attr) {
	_getattr_up(KX_TouchSensor);
}

/* getConeOrigin */
char KX_RadarSensor::GetConeOrigin_doc[] = 
"getConeOrigin()\n"
"\tReturns the origin of the cone with which to test. The origin\n"
"\tis in the middle of the cone.";
PyObject* KX_RadarSensor::PyGetConeOrigin(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) {
	PyObject *retVal = PyList_New(3);
	
	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_cone_origin[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_cone_origin[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_cone_origin[2]));
	
	return retVal;
}

/* getConeOrigin */
char KX_RadarSensor::GetConeTarget_doc[] = 
"getConeTarget()\n"
"\tReturns the center of the bottom face of the cone with which to test.\n";
PyObject* KX_RadarSensor::PyGetConeTarget(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) {
	PyObject *retVal = PyList_New(3);
	
	PyList_SetItem(retVal, 0, PyFloat_FromDouble(m_cone_target[0]));
	PyList_SetItem(retVal, 1, PyFloat_FromDouble(m_cone_target[1]));
	PyList_SetItem(retVal, 2, PyFloat_FromDouble(m_cone_target[2]));
	
	return retVal;
}

/* getConeOrigin */
char KX_RadarSensor::GetConeHeight_doc[] = 
"getConeHeight()\n"
"\tReturns the height of the cone with which to test.\n";
PyObject* KX_RadarSensor::PyGetConeHeight(PyObject* self, 
										  PyObject* args, 
										  PyObject* kwds) {
	return PyFloat_FromDouble(m_coneheight);
}


#endif //PHYSICS_NOT_YET
