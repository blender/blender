/**
 * Senses touch and collision events
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

#include "KX_TouchSensor.h"
#include "SCA_EventManager.h"
#include "SCA_LogicManager.h"
#include "KX_GameObject.h"
#include "KX_TouchEventManager.h"

#include "PHY_IPhysicsController.h"

#include <iostream>
#include "PHY_IPhysicsEnvironment.h"

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

void KX_TouchSensor::SynchronizeTransform()
{
	// the touch sensor does not require any synchronization: it uses
	// the same physical object which is already synchronized by Blender
}


void KX_TouchSensor::EndFrame() {
	m_colliders->ReleaseAndRemoveAll();
	m_hitObject = NULL;
	m_bTriggered = false;
	m_bColliderHash = 0;
}

void KX_TouchSensor::UnregisterToManager()
{
	// before unregistering the sensor, make sure we release all references
	EndFrame();
	SCA_ISensor::UnregisterToManager();
}

bool KX_TouchSensor::Evaluate()
{
	bool result = false;
	bool reset = m_reset && m_level;
	m_reset = false;
	if (m_bTriggered != m_bLastTriggered)
	{
		m_bLastTriggered = m_bTriggered;
		if (!m_bTriggered)
			m_hitObject = NULL;
		result = true;
	}
	if (reset)
		// force an event
		result = true;
	
	if (m_bTouchPulse) { /* pulse on changes to the colliders */
		int count = m_colliders->GetCount();
		
		if (m_bLastCount!=count || m_bColliderHash!=m_bLastColliderHash) {
			m_bLastCount = count;
			m_bLastColliderHash= m_bColliderHash;
			result = true;
		}
	}
	return result;
}

KX_TouchSensor::KX_TouchSensor(SCA_EventManager* eventmgr,KX_GameObject* gameobj,bool bFindMaterial,bool bTouchPulse,const STR_String& touchedpropname)
:SCA_ISensor(gameobj,eventmgr),
m_touchedpropname(touchedpropname),
m_bFindMaterial(bFindMaterial),
m_bTouchPulse(bTouchPulse)
/*m_sumoObj(sumoObj),*/
{
//	KX_TouchEventManager* touchmgr = (KX_TouchEventManager*) eventmgr;
//	m_resptable = touchmgr->GetResponseTable();
	
//	m_solidHandle = m_sumoObj->getObjectHandle();

	m_colliders = new CListValue();
	
	KX_ClientObjectInfo *client_info = gameobj->getClientInfo();
	//client_info->m_gameobject = gameobj;
	//client_info->m_auxilary_info = NULL;
	client_info->m_sensors.push_back(this);
	
	m_physCtrl = dynamic_cast<PHY_IPhysicsController*>(gameobj->GetPhysicsController());
	MT_assert( !gameobj->GetPhysicsController() || m_physCtrl );
	Init();
}

void KX_TouchSensor::Init()
{
	m_bCollision = false;
	m_bTriggered = false;
	m_bLastTriggered = (m_invert)?true:false;
	m_bLastCount = 0;
	m_bColliderHash = m_bLastColliderHash = 0;
	m_hitObject =  NULL;
	m_reset = true;
}

KX_TouchSensor::~KX_TouchSensor()
{
	//DT_ClearObjectResponse(m_resptable,m_solidHandle);
	m_colliders->Release();
}

CValue* KX_TouchSensor::GetReplica() 
{
	KX_TouchSensor* replica = new KX_TouchSensor(*this);
	replica->ProcessReplica();
	return replica;
}

void KX_TouchSensor::ProcessReplica()
{
	SCA_ISensor::ProcessReplica();
	m_colliders = new CListValue();
	Init();
}

void	KX_TouchSensor::ReParent(SCA_IObject* parent)
{
	KX_GameObject *gameobj = static_cast<KX_GameObject *>(parent);
	PHY_IPhysicsController *sphy = dynamic_cast<PHY_IPhysicsController*>(((KX_GameObject*)parent)->GetPhysicsController());
	if (sphy)
		m_physCtrl = sphy;
	
//	m_solidHandle = m_sumoObj->getObjectHandle();
	KX_ClientObjectInfo *client_info = gameobj->getClientInfo();
	//client_info->m_gameobject = gameobj;
	//client_info->m_auxilary_info = NULL;
	
	client_info->m_sensors.push_back(this);
	SCA_ISensor::ReParent(parent);
}

void KX_TouchSensor::RegisterSumo(KX_TouchEventManager *touchman)
{
	if (m_physCtrl)
	{
		if (touchman->GetPhysicsEnvironment()->requestCollisionCallback(m_physCtrl))
		{
			KX_ClientObjectInfo* client_info = static_cast<KX_ClientObjectInfo*>(m_physCtrl->getNewClientInfo());
			if (client_info->isSensor())
				touchman->GetPhysicsEnvironment()->addSensor(m_physCtrl);
		}
	}
}
void KX_TouchSensor::UnregisterSumo(KX_TouchEventManager* touchman)
{
	if (m_physCtrl)
	{
		if (touchman->GetPhysicsEnvironment()->removeCollisionCallback(m_physCtrl))
		{
			// no more sensor on the controller, can remove it if it is a sensor object
			KX_ClientObjectInfo* client_info = static_cast<KX_ClientObjectInfo*>(m_physCtrl->getNewClientInfo());
			if (client_info->isSensor())
				touchman->GetPhysicsEnvironment()->removeSensor(m_physCtrl);
		}
	}
}

// this function is called only for sensor objects
// return true if the controller can collide with the object
bool	KX_TouchSensor::BroadPhaseSensorFilterCollision(void*obj1,void*obj2)
{
	assert(obj1==m_physCtrl && obj2);

	KX_GameObject* myobj = (KX_GameObject*)GetParent();
	KX_GameObject* myparent = myobj->GetParent();
	KX_ClientObjectInfo* client_info = static_cast<KX_ClientObjectInfo*>(((PHY_IPhysicsController*)obj2)->getNewClientInfo());
	KX_ClientObjectInfo* my_client_info = static_cast<KX_ClientObjectInfo*>(m_physCtrl->getNewClientInfo());
	KX_GameObject* otherobj = ( client_info ? client_info->m_gameobject : NULL);

	// first, decrement refcount as GetParent() increases it
	if (myparent)
		myparent->Release();

	// we can only check on persistent characteristic: m_link and m_suspended are not
	// good candidate because they are transient. That must be handled at another level
	if (!otherobj ||
		otherobj == myparent ||		// don't interact with our parent
		(my_client_info->m_type == KX_ClientObjectInfo::OBACTORSENSOR &&
		 client_info->m_type != KX_ClientObjectInfo::ACTOR))	// only with actor objects
		return false;
		
	bool found = m_touchedpropname.IsEmpty();
	if (!found)
	{
		if (m_bFindMaterial)
		{
			if (client_info->m_auxilary_info)
			{
				found = (!strcmp(m_touchedpropname.Ptr(), (char*)client_info->m_auxilary_info));
			}
		} else
		{
			found = (otherobj->GetProperty(m_touchedpropname) != NULL);
		}
	}
	return found;
}

bool	KX_TouchSensor::NewHandleCollision(void*object1,void*object2,const PHY_CollData* colldata)
{
//	KX_TouchEventManager* toucheventmgr = (KX_TouchEventManager*)m_eventmgr;
	KX_GameObject* parent = (KX_GameObject*)GetParent();

	// need the mapping from PHY_IPhysicsController to gameobjects now
	
	KX_ClientObjectInfo* client_info = static_cast<KX_ClientObjectInfo*> (object1 == m_physCtrl? 
					((PHY_IPhysicsController*)object2)->getNewClientInfo(): 
					((PHY_IPhysicsController*)object1)->getNewClientInfo());

	KX_GameObject* gameobj = ( client_info ? 
			client_info->m_gameobject : 
			NULL);
	
	// add the same check as in SCA_ISensor::Activate(), 
	// we don't want to record collision when the sensor is not active.
	if (m_links && !m_suspended &&
		gameobj && (gameobj != parent) && client_info->isActor())
	{
		
		bool found = m_touchedpropname.IsEmpty();
		if (!found)
		{
			if (m_bFindMaterial)
			{
				if (client_info->m_auxilary_info)
				{
					found = (!strcmp(m_touchedpropname.Ptr(), (char*)client_info->m_auxilary_info));
				}
			} else
			{
				found = (gameobj->GetProperty(m_touchedpropname) != NULL);
			}
		}
		if (found)
		{
			if (!m_colliders->SearchValue(gameobj)) {
				m_colliders->Add(gameobj->AddRef());
				
				if (m_bTouchPulse)
					m_bColliderHash += (uint_ptr)(static_cast<void *>(&gameobj));
			}
			m_bTriggered = true;
			m_hitObject = gameobj;
			//printf("KX_TouchSensor::HandleCollision\n");
		}
		
	} 
	return false; // was DT_CONTINUE but this was defined in sumo as false.
}

#ifndef DISABLE_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */
/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_TouchSensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_TouchSensor",
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

PyMethodDef KX_TouchSensor::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_TouchSensor::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW("propName",0,100,false,KX_TouchSensor,m_touchedpropname),
	KX_PYATTRIBUTE_BOOL_RW("useMaterial",KX_TouchSensor,m_bFindMaterial),
	KX_PYATTRIBUTE_BOOL_RW("usePulseCollision",KX_TouchSensor,m_bTouchPulse),
	KX_PYATTRIBUTE_RO_FUNCTION("hitObject", KX_TouchSensor, pyattr_get_object_hit),
	KX_PYATTRIBUTE_RO_FUNCTION("hitObjectList", KX_TouchSensor, pyattr_get_object_hit_list),
	{ NULL }	//Sentinel
};

/* Python API */

PyObject* KX_TouchSensor::pyattr_get_object_hit(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_TouchSensor* self= static_cast<KX_TouchSensor*>(self_v);
	
	if (self->m_hitObject)
		return self->m_hitObject->GetProxy();
	else
		Py_RETURN_NONE;
}

PyObject* KX_TouchSensor::pyattr_get_object_hit_list(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_TouchSensor* self= static_cast<KX_TouchSensor*>(self_v);
	return self->m_colliders->GetProxy();
}

#endif

/* eof */
