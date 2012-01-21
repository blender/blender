/*
 * Cast a ray and feel for objects
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

/** \file gameengine/Ketsji/KX_RaySensor.cpp
 *  \ingroup ketsji
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
#include "KX_PyMath.h"
#include "PHY_IPhysicsEnvironment.h"
#include "KX_IPhysicsController.h"
#include "PHY_IPhysicsController.h"
#include "DNA_sensor_types.h"

#include <stdio.h>


KX_RaySensor::KX_RaySensor(class SCA_EventManager* eventmgr,
					SCA_IObject* gameobj,
					const STR_String& propname,
					bool bFindMaterial,
					bool bXRay,
					double distance,
					int axis,
					KX_Scene* ketsjiScene)
			: SCA_ISensor(gameobj,eventmgr),
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
	replica->ProcessReplica();
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
		m_hitPosition[0] = result->m_hitPoint[0];
		m_hitPosition[1] = result->m_hitPoint[1];
		m_hitPosition[2] = result->m_hitPoint[2];

		m_hitNormal[0] = result->m_hitNormal[0];
		m_hitNormal[1] = result->m_hitNormal[1];
		m_hitNormal[2] = result->m_hitNormal[2];
			
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

bool KX_RaySensor::Evaluate()
{
	bool result = false;
	bool reset = m_reset && m_level;
	m_rayHit = false; 
	m_hitObject = NULL;
	m_hitPosition[0] = 0;
	m_hitPosition[1] = 0;
	m_hitPosition[2] = 0;

	m_hitNormal[0] = 1;
	m_hitNormal[1] = 0;
	m_hitNormal[2] = 0;
	
	KX_GameObject* obj = (KX_GameObject*)GetParent();
	MT_Point3 frompoint = obj->NodeGetWorldPosition();
	MT_Matrix3x3 matje = obj->NodeGetWorldOrientation();
	MT_Matrix3x3 invmat = matje.inverse();
	
	MT_Vector3 todir;
	m_reset = false;
	switch (m_axis)
	{
	case SENS_RAY_X_AXIS: // X
		{
			todir[0] = invmat[0][0];
			todir[1] = invmat[0][1];
			todir[2] = invmat[0][2];
			break;
		}
	case SENS_RAY_Y_AXIS: // Y
		{
			todir[0] = invmat[1][0];
			todir[1] = invmat[1][1];
			todir[2] = invmat[1][2];
			break;
		}
	case SENS_RAY_Z_AXIS: // Z
		{
			todir[0] = invmat[2][0];
			todir[1] = invmat[2][1];
			todir[2] = invmat[2][2];
			break;
		}
	case SENS_RAY_NEG_X_AXIS: // -X
		{
			todir[0] = -invmat[0][0];
			todir[1] = -invmat[0][1];
			todir[2] = -invmat[0][2];
			break;
		}
	case SENS_RAY_NEG_Y_AXIS: // -Y
		{
			todir[0] = -invmat[1][0];
			todir[1] = -invmat[1][1];
			todir[2] = -invmat[1][2];
			break;
		}
	case SENS_RAY_NEG_Z_AXIS: // -Z
		{
			todir[0] = -invmat[2][0];
			todir[1] = -invmat[2][1];
			todir[2] = -invmat[2][2];
			break;
		}
	}
	todir.normalize();
	m_rayDirection[0] = todir[0];
	m_rayDirection[1] = todir[1];
	m_rayDirection[2] = todir[2];

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

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_RaySensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_RaySensor",
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
	&SCA_ISensor::Type,
	0,0,0,0,0,0,
	py_base_new

};

PyMethodDef KX_RaySensor::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_RaySensor::Attributes[] = {
	KX_PYATTRIBUTE_BOOL_RW("useMaterial", KX_RaySensor, m_bFindMaterial),
	KX_PYATTRIBUTE_BOOL_RW("useXRay", KX_RaySensor, m_bXRay),
	KX_PYATTRIBUTE_FLOAT_RW("range", 0, 10000, KX_RaySensor, m_distance),
	KX_PYATTRIBUTE_STRING_RW("propName", 0, MAX_PROP_NAME, false, KX_RaySensor, m_propertyname),
	KX_PYATTRIBUTE_INT_RW("axis", 0, 5, true, KX_RaySensor, m_axis),
	KX_PYATTRIBUTE_FLOAT_ARRAY_RO("hitPosition", KX_RaySensor, m_hitPosition, 3),
	KX_PYATTRIBUTE_FLOAT_ARRAY_RO("rayDirection", KX_RaySensor, m_rayDirection, 3),
	KX_PYATTRIBUTE_FLOAT_ARRAY_RO("hitNormal", KX_RaySensor, m_hitNormal, 3),
	KX_PYATTRIBUTE_RO_FUNCTION("hitObject", KX_RaySensor, pyattr_get_hitobject),
	{ NULL }	//Sentinel
};

PyObject* KX_RaySensor::pyattr_get_hitobject(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_RaySensor* self = static_cast<KX_RaySensor*>(self_v);
	if (self->m_hitObject)
		return self->m_hitObject->GetProxy();

	Py_RETURN_NONE;
}

#endif // WITH_PYTHON
