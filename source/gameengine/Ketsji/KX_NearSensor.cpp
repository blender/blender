/**
 * Sense if other objects are near
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

#include "KX_NearSensor.h"
#include "SCA_LogicManager.h"
#include "KX_GameObject.h"
#include "KX_TouchEventManager.h"
#include "KX_Scene.h" // needed to create a replica
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IPhysicsController.h"


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
KX_NearSensor::KX_NearSensor(SCA_EventManager* eventmgr,
							 KX_GameObject* gameobj,
							 double margin,
							 double resetmargin,
							 bool bFindMaterial,
							 const STR_String& touchedpropname,
							 class KX_Scene* scene,
 							 PHY_IPhysicsController*	ctrl,
							 PyTypeObject* T)
			 :KX_TouchSensor(eventmgr,
							 gameobj,
							 bFindMaterial,
							 touchedpropname,
							 /* scene, */
							 T),
			 m_Margin(margin),
			 m_ResetMargin(resetmargin)

{

	gameobj->getClientInfo()->m_sensors.remove(this);
	m_client_info = new KX_ClientObjectInfo(gameobj, KX_ClientObjectInfo::NEAR);
	m_client_info->m_sensors.push_back(this);
	
	//DT_ShapeHandle shape = (DT_ShapeHandle) vshape;
	m_physCtrl = ctrl;
	if (m_physCtrl)
	{
		m_physCtrl->SetMargin(m_Margin);
		m_physCtrl->setNewClientInfo(m_client_info);
	}
	SynchronizeTransform();
}

void KX_NearSensor::SynchronizeTransform()
{
	// The near and radar sensors are using a different physical object which is 
	// not linked to the parent object, must synchronize it.
	if (m_physCtrl)
	{
		KX_GameObject* parent = ((KX_GameObject*)GetParent());
		MT_Vector3 pos = parent->NodeGetWorldPosition();
		MT_Quaternion orn = parent->NodeGetWorldOrientation().getRotation();
		m_physCtrl->setPosition(pos.x(),pos.y(),pos.z());
		m_physCtrl->setOrientation(orn.x(),orn.y(),orn.z(),orn.w());
		m_physCtrl->calcXform();
	}
}

void KX_NearSensor::RegisterSumo(KX_TouchEventManager *touchman)
{
	if (m_physCtrl)
	{
		touchman->GetPhysicsEnvironment()->addSensor(m_physCtrl);
	}
}

CValue* KX_NearSensor::GetReplica()
{
	KX_NearSensor* replica = new KX_NearSensor(*this);
	replica->m_colliders = new CListValue();
	replica->Init();
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	
	replica->m_client_info = new KX_ClientObjectInfo(m_client_info->m_gameobject, KX_ClientObjectInfo::NEAR);
	
	if (replica->m_physCtrl)
	{
		replica->m_physCtrl = replica->m_physCtrl->GetReplica();
		if (replica->m_physCtrl)
		{
			//static_cast<KX_TouchEventManager*>(m_eventmgr)->GetPhysicsEnvironment()->addSensor(replica->m_physCtrl);
			replica->m_physCtrl->SetMargin(m_Margin);
			replica->m_physCtrl->setNewClientInfo(replica->m_client_info);
		}
		
	}
	//static_cast<KX_TouchEventManager*>(m_eventmgr)->RegisterSensor(this);
	//todo: make sure replication works fine
	//>m_sumoObj = new SM_Object(DT_NewSphere(0.0),NULL,NULL,NULL);
	//replica->m_sumoObj->setMargin(m_Margin);
	//replica->m_sumoObj->setClientObject(replica->m_client_info);
	
	((KX_GameObject*)replica->GetParent())->GetSGNode()->ComputeWorldTransforms(NULL);
	replica->SynchronizeTransform();
	
	return replica;
}



void KX_NearSensor::ReParent(SCA_IObject* parent)
{

	SCA_ISensor::ReParent(parent);
	
	m_client_info->m_gameobject = static_cast<KX_GameObject*>(parent); 
	m_client_info->m_sensors.push_back(this);
	

/*	KX_ClientObjectInfo *client_info = gameobj->getClientInfo();
	client_info->m_gameobject = gameobj;
	client_info->m_auxilary_info = NULL;
	
	client_info->m_sensors.push_back(this);
	SCA_ISensor::ReParent(parent);
*/
	((KX_GameObject*)GetParent())->GetSGNode()->ComputeWorldTransforms(NULL);
	SynchronizeTransform();
}



KX_NearSensor::~KX_NearSensor()
{
	// for nearsensor, the sensor is the 'owner' of sumoobj
	// for touchsensor, it's the parent
	if (m_physCtrl)
	{
		//static_cast<KX_TouchEventManager*>(m_eventmgr)->GetPhysicsEnvironment()->removeSensor(m_physCtrl);
		delete m_physCtrl;
		m_physCtrl = NULL;
	}
	
		
	if (m_client_info)
		delete m_client_info;
}


bool KX_NearSensor::Evaluate(CValue* event)
{
	bool result = false;
//	KX_GameObject* parent = static_cast<KX_GameObject*>(GetParent());

	if (m_bTriggered != m_bLastTriggered)
	{
		m_bLastTriggered = m_bTriggered;
		if (m_bTriggered)
		{
			if (m_physCtrl)
			{
				m_physCtrl->SetMargin(m_ResetMargin);
			}
		} else
		{
			if (m_physCtrl)
			{
				m_physCtrl->SetMargin(m_Margin);
			}

		}
		result = true;
	}

	return result;
}

// this function is called at broad phase stage to check if the two controller
// need to interact at all. It is used for Near/Radar sensor that don't need to
// check collision with object not included in filter
bool	KX_NearSensor::BroadPhaseFilterCollision(void*obj1,void*obj2)
{
	KX_GameObject* parent = static_cast<KX_GameObject*>(GetParent());
	
	// need the mapping from PHY_IPhysicsController to gameobjects now
	assert(obj1==m_physCtrl && obj2);
	KX_ClientObjectInfo* client_info = static_cast<KX_ClientObjectInfo*>((static_cast<PHY_IPhysicsController*>(obj2))->getNewClientInfo());

	KX_GameObject* gameobj = ( client_info ? 
			client_info->m_gameobject :
			NULL);
	
	if (gameobj && (gameobj != parent))
	{
		// only take valid colliders
		if (client_info->m_type == KX_ClientObjectInfo::ACTOR)
		{
			if ((m_touchedpropname.Length() == 0) || 
				(gameobj->GetProperty(m_touchedpropname)))
			{
				return true;
			}
		}
	}

	return false;
}

bool	KX_NearSensor::NewHandleCollision(void* obj1,void* obj2,const PHY_CollData * coll_data)
{
//	KX_TouchEventManager* toucheventmgr = static_cast<KX_TouchEventManager*>(m_eventmgr);
	KX_GameObject* parent = static_cast<KX_GameObject*>(GetParent());
	
	// need the mapping from PHY_IPhysicsController to gameobjects now
	
	KX_ClientObjectInfo* client_info =static_cast<KX_ClientObjectInfo*> (obj1 == m_physCtrl? 
					((PHY_IPhysicsController*)obj2)->getNewClientInfo() : 
					((PHY_IPhysicsController*)obj1)->getNewClientInfo());

	KX_GameObject* gameobj = ( client_info ? 
			client_info->m_gameobject :
			NULL);
	
	// these checks are done already in BroadPhaseFilterCollision()
	if (gameobj /*&& (gameobj != parent)*/)
	{
		if (!m_colliders->SearchValue(gameobj))
			m_colliders->Add(gameobj->AddRef());
		// only take valid colliders
		// These checks are done already in BroadPhaseFilterCollision()
		//if (client_info->m_type == KX_ClientObjectInfo::ACTOR)
		//{
		//	if ((m_touchedpropname.Length() == 0) || 
		//		(gameobj->GetProperty(m_touchedpropname)))
		//	{
				m_bTriggered = true;
				m_hitObject = gameobj;
		//	}
		//}
	}
	
	return DT_CONTINUE;
}



// python embedding
PyTypeObject KX_NearSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_NearSensor",
	sizeof(KX_NearSensor),
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



PyParentObject KX_NearSensor::Parents[] = {
	&KX_NearSensor::Type,
	&KX_TouchSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};



PyMethodDef KX_NearSensor::Methods[] = {
	{"setProperty", 
	 (PyCFunction) KX_NearSensor::sPySetProperty,      METH_VARARGS, SetProperty_doc},
	{"getProperty", 
	 (PyCFunction) KX_NearSensor::sPyGetProperty,      METH_VARARGS, GetProperty_doc},
	{"getHitObject", 
	 (PyCFunction) KX_NearSensor::sPyGetHitObject,     METH_VARARGS, GetHitObject_doc},
	{"getHitObjectList", 
	 (PyCFunction) KX_NearSensor::sPyGetHitObjectList, METH_VARARGS, GetHitObjectList_doc},
	{NULL,NULL} //Sentinel
};


PyObject*
KX_NearSensor::_getattr(const STR_String& attr)
{
  _getattr_up(KX_TouchSensor);
}

