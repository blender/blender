/**
 * Cast a ray and feel for objects
 *
 * $Id$
 *
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

#include "KX_RaySensor.h"
#include "SCA_EventManager.h"
#include "SCA_RandomEventManager.h"
#include "SCA_LogicManager.h"
#include "SCA_IObject.h"
#include "KX_ClientObjectInfo.h"
#include "KX_GameObject.h"
#include "KX_Scene.h"


KX_RaySensor::KX_RaySensor(class SCA_EventManager* eventmgr,
					SCA_IObject* gameobj,
					const STR_String& propname,
					bool bFindMaterial,
					double distance,
					int axis,
					KX_Scene* ketsjiScene,
					PyTypeObject* T)
			: SCA_ISensor(gameobj,eventmgr, T),
					m_propertyname(propname),
					m_bFindMaterial(bFindMaterial),
					m_distance(distance),
					m_axis(axis),
					m_ketsjiScene(ketsjiScene),
					m_rayHit(false),
					m_bTriggered(false),
					m_hitObject(NULL)

				
{

}



KX_RaySensor::~KX_RaySensor() 
{
    /* Nothing to be done here. */
}



CValue* KX_RaySensor::GetReplica()
{
	CValue* replica = new KX_RaySensor(*this);
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);

	return replica;
}



bool KX_RaySensor::IsPositiveTrigger()
{
	bool result = m_rayHit;

	if (m_invert)
		result = !result;
	
	return result;
}



bool KX_RaySensor::Evaluate(CValue* event)
{
	bool result = false;
	m_rayHit = false; 
	m_hitObject = NULL;
	m_hitPosition = MT_Vector3(0,0,0);
	m_hitNormal = MT_Vector3(1,0,0);
	
	KX_GameObject* obj = (KX_GameObject*)GetParent();
	MT_Point3 frompoint = obj->NodeGetWorldPosition();
	MT_Matrix3x3 matje = obj->NodeGetWorldOrientation();
	MT_Matrix3x3 invmat = matje.inverse();
	
	MT_Vector3 todir;
	switch (m_axis)
	{
	case 1: // X
		{
			todir[0] = invmat[0][0];
			todir[1] = invmat[0][1];
			todir[2] = invmat[0][2];
			break;
		}
	case 0: // Y
		{
			todir[0] = invmat[1][0];
			todir[1] = invmat[1][1];
			todir[2] = invmat[1][2];
			break;
		}
	case 2: // Z
		{
			todir[0] = invmat[2][0];
			todir[1] = invmat[2][1];
			todir[2] = invmat[2][2];
			break;
		}
	case 3: // -X
		{
			todir[0] = invmat[0][0] * -1;
			todir[1] = invmat[0][1] * -1;
			todir[2] = invmat[0][2] * -1;
			break;
		}
	case 4: // -Y
		{
			todir[0] = invmat[1][0] * -1;
			todir[1] = invmat[1][1] * -1;
			todir[2] = invmat[1][2] * -1;
			break;
		}
	case 5: // -Z
		{
			todir[0] = invmat[2][0] * -1;
			todir[1] = invmat[2][1] * -1;
			todir[2] = invmat[2][2] * -1;
			break;
		}
	}
	todir.normalize();
	m_rayDirection = todir;
	


	MT_Point3 topoint = frompoint + (m_distance) * todir;
	MT_Point3 resultpoint;
	MT_Vector3 resultnormal;
	bool ready = false;
	/*
	do {
	
		
		
		SM_Object* hitObj = m_sumoScene->rayTest(obj->GetSumoObject(),
												 frompoint,
												 topoint,
												 resultpoint,
												 resultnormal);
		if (hitObj)
		{
			KX_ClientObjectInfo* info = (SM_ClientObjectInfo*)hitObj->getClientObject();
			SCA_IObject* hitgameobj = (SCA_IObject*)info->m_clientobject;
			bool bFound = false;

			if (hitgameobj == obj)
			{
				// false hit
				MT_Scalar marg = obj->GetSumoObject()->getMargin() ;
				frompoint = resultpoint + marg * todir;
			}
			else
			{
				ready = true;
				if (m_propertyname.Length() == 0)
				{
					bFound = true;
				}
				else
				{
					if (m_bFindMaterial)
					{
						if (info->m_auxilary_info)
						{
							bFound = (m_propertyname== ((char*)info->m_auxilary_info));
						}
					}
					else
					{
						if (hitgameobj->GetProperty(m_propertyname) != NULL)
						{
							bFound = true;
						}
					}
				}

				if (bFound)
				{
					m_rayHit = true;
					m_hitObject = hitgameobj;
					m_hitPosition = resultpoint;
					m_hitNormal = resultnormal;
						
				}
			}
		}
		else
		{
			ready = true;
		}
	}
	while (!ready);
	*/
	
	
	/* now pass this result to some controller */
    if (m_rayHit) 
	{
		if (!m_bTriggered)
		{
			// notify logicsystem that ray is now hitting
			result = true;
			m_bTriggered = true;
		}
		else
		{
			
		}
	}
	else
	{
		if (m_bTriggered)
		{
			m_bTriggered = false;
			// notify logicsystem that ray is not hitting anymore
			result = true;
		}
	}

	return result;
}



/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_RaySensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_RaySensor",
	sizeof(KX_RaySensor),
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

PyParentObject KX_RaySensor::Parents[] = {
	&KX_RaySensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_RaySensor::Methods[] = {
	{"getHitObject",(PyCFunction) KX_RaySensor::sPyGetHitObject,METH_VARARGS, GetHitObject_doc},
	{"getHitPosition",(PyCFunction) KX_RaySensor::sPyGetHitPosition,METH_VARARGS, GetHitPosition_doc},
	{"getHitNormal",(PyCFunction) KX_RaySensor::sPyGetHitNormal,METH_VARARGS, GetHitNormal_doc},
	{"getRayDirection",(PyCFunction) KX_RaySensor::sPyGetRayDirection,METH_VARARGS, GetRayDirection_doc},
	{NULL,NULL} //Sentinel
};

char KX_RaySensor::GetHitObject_doc[] = 
"getHitObject()\n"
"\tReturns the name of the object that was hit by this ray.\n";
PyObject* KX_RaySensor::PyGetHitObject(PyObject* self, 
										   PyObject* args, 
										   PyObject* kwds)
{
	if (m_hitObject)
	{
		return m_hitObject->AddRef();
	}
	Py_Return;
}


char KX_RaySensor::GetHitPosition_doc[] = 
"getHitPosition()\n"
"\tReturns the position (in worldcoordinates) where the object was hit by this ray.\n";
PyObject* KX_RaySensor::PyGetHitPosition(PyObject* self, 
			       PyObject* args, 
			       PyObject* kwds)
{

	MT_Point3 pos = m_hitPosition;

	PyObject* resultlist = PyList_New(3);
	int index;
	for (index=0;index<3;index++)
	{
		PyList_SetItem(resultlist,index,PyFloat_FromDouble(pos[index]));
	}
	return resultlist;

}

char KX_RaySensor::GetRayDirection_doc[] = 
"getRayDirection()\n"
"\tReturns the direction from the ray (in worldcoordinates) .\n";
PyObject* KX_RaySensor::PyGetRayDirection(PyObject* self, 
			       PyObject* args, 
			       PyObject* kwds)
{

	MT_Vector3 dir = m_rayDirection;

	PyObject* resultlist = PyList_New(3);
	int index;
	for (index=0;index<3;index++)
	{
		PyList_SetItem(resultlist,index,PyFloat_FromDouble(dir[index]));
	}
	return resultlist;

}

char KX_RaySensor::GetHitNormal_doc[] = 
"getHitNormal()\n"
"\tReturns the normal (in worldcoordinates) of the object at the location where the object was hit by this ray.\n";
PyObject* KX_RaySensor::PyGetHitNormal(PyObject* self, 
			       PyObject* args, 
			       PyObject* kwds)
{
	MT_Vector3 pos = m_hitNormal;

	PyObject* resultlist = PyList_New(3);
	int index;
	for (index=0;index<3;index++)
	{
		PyList_SetItem(resultlist,index,PyFloat_FromDouble(pos[index]));
	}
	return resultlist;

}



PyObject* KX_RaySensor::_getattr(char* attr) {
	_getattr_up(SCA_ISensor);
}
