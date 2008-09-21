/**
 * Cast a ray and feel for objects
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

#include "KX_RaySensor.h"
#include "SCA_EventManager.h"
#include "SCA_RandomEventManager.h"
#include "SCA_LogicManager.h"
#include "SCA_IObject.h"
#include "KX_ClientObjectInfo.h"
#include "KX_GameObject.h"
#include "KX_Scene.h"
#include "KX_RayCast.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_IPhysicsController.h"
#include "PHY_IPhysicsController.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


KX_RaySensor::KX_RaySensor(class SCA_EventManager* eventmgr,
					SCA_IObject* gameobj,
					const STR_String& propname,
					bool bFindMaterial,
					bool bXRay,
					double distance,
					int axis,
					KX_Scene* ketsjiScene,
					PyTypeObject* T)
			: SCA_ISensor(gameobj,eventmgr, T),
					m_propertyname(propname),
					m_bFindMaterial(bFindMaterial),
					m_bXRay(bXRay),
					m_distance(distance),
					m_scene(ketsjiScene),
					m_axis(axis)

				
{
	Init();
}

void KX_RaySensor::Init()
{
	m_bTriggered = (m_invert)?true:false;
	m_rayHit = false;
	m_hitObject = NULL;
	m_reset = true;
}

KX_RaySensor::~KX_RaySensor() 
{
    /* Nothing to be done here. */
}



CValue* KX_RaySensor::GetReplica()
{
	KX_RaySensor* replica = new KX_RaySensor(*this);
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	replica->Init();

	return replica;
}



bool KX_RaySensor::IsPositiveTrigger()
{
	bool result = m_rayHit;

	if (m_invert)
		result = !result;
	
	return result;
}

bool KX_RaySensor::RayHit(KX_ClientObjectInfo* client, KX_RayCast* result, void * const data)
{

	KX_GameObject* hitKXObj = client->m_gameobject;
	bool bFound = false;

	if (m_propertyname.Length() == 0)
	{
		bFound = true;
	}
	else
	{
		if (m_bFindMaterial)
		{
			if (client->m_auxilary_info)
			{
				bFound = (m_propertyname== ((char*)client->m_auxilary_info));
			}
		}
		else
		{
			bFound = hitKXObj->GetProperty(m_propertyname) != NULL;
		}
	}

	if (bFound)
	{
		m_rayHit = true;
		m_hitObject = hitKXObj;
		m_hitPosition = result->m_hitPoint;
		m_hitNormal = result->m_hitNormal;
			
	}
	// no multi-hit search yet
	return true;
}

/* this function is used to pre-filter the object before casting the ray on them.
   This is useful for "X-Ray" option when we want to see "through" unwanted object.
 */
bool KX_RaySensor::NeedRayCast(KX_ClientObjectInfo* client)
{
	if (client->m_type > KX_ClientObjectInfo::ACTOR)
	{
		// Unknown type of object, skip it.
		// Should not occur as the sensor objects are filtered in RayTest()
		printf("Invalid client type %d found ray casting\n", client->m_type);
		return false;
	}
	if (m_bXRay && m_propertyname.Length() != 0)
	{
		if (m_bFindMaterial)
		{
			// not quite correct: an object may have multiple material
			// should check all the material and not only the first one
			if (!client->m_auxilary_info || (m_propertyname != ((char*)client->m_auxilary_info)))
				return false;
		}
		else
		{
			if (client->m_gameobject->GetProperty(m_propertyname) == NULL)
				return false;
		}
	}
	return true;
}

bool KX_RaySensor::Evaluate(CValue* event)
{
	bool result = false;
	bool reset = m_reset && m_level;
	m_rayHit = false; 
	m_hitObject = NULL;
	m_hitPosition = MT_Vector3(0,0,0);
	m_hitNormal = MT_Vector3(1,0,0);
	
	KX_GameObject* obj = (KX_GameObject*)GetParent();
	MT_Point3 frompoint = obj->NodeGetWorldPosition();
	MT_Matrix3x3 matje = obj->NodeGetWorldOrientation();
	MT_Matrix3x3 invmat = matje.inverse();
	
	MT_Vector3 todir;
	m_reset = false;
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
			todir[0] = -invmat[0][0];
			todir[1] = -invmat[0][1];
			todir[2] = -invmat[0][2];
			break;
		}
	case 4: // -Y
		{
			todir[0] = -invmat[1][0];
			todir[1] = -invmat[1][1];
			todir[2] = -invmat[1][2];
			break;
		}
	case 5: // -Z
		{
			todir[0] = -invmat[2][0];
			todir[1] = -invmat[2][1];
			todir[2] = -invmat[2][2];
			break;
		}
	}
	todir.normalize();
	m_rayDirection = todir;

	MT_Point3 topoint = frompoint + (m_distance) * todir;
	PHY_IPhysicsEnvironment* pe = m_scene->GetPhysicsEnvironment();

	if (!pe)
	{
		std::cout << "WARNING: Ray sensor " << GetName() << ":  There is no physics environment!" << std::endl;
		std::cout << "         Check universe for malfunction." << std::endl;
		return false;
	} 

	KX_IPhysicsController *spc = obj->GetPhysicsController();
	KX_GameObject *parent = obj->GetParent();
	if (!spc && parent)
		spc = parent->GetPhysicsController();
	
	if (parent)
		parent->Release();
	

	PHY_IPhysicsEnvironment* physics_environment = this->m_scene->GetPhysicsEnvironment();
	

	KX_RayCast::Callback<KX_RaySensor> callback(this, spc);
	KX_RayCast::RayTest(physics_environment, frompoint, topoint, callback);

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
			// notify logicsystem that ray is STILL hitting ...
			result = false;
		    
		  }
	}
    else
      {
		if (m_bTriggered)
		{
			m_bTriggered = false;
			// notify logicsystem that ray JUST left the Object
			result = true;
		}
		else
		{
			result = false;
		}
	
      }
    if (reset)
		// force an event
		result = true;

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

const char KX_RaySensor::GetHitObject_doc[] = 
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


const char KX_RaySensor::GetHitPosition_doc[] = 
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

const char KX_RaySensor::GetRayDirection_doc[] = 
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

const char KX_RaySensor::GetHitNormal_doc[] = 
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



PyObject* KX_RaySensor::_getattr(const STR_String& attr) {
	_getattr_up(SCA_ISensor);
}
