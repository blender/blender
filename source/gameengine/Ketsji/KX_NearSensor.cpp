/**
 * Sense if other objects are near
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

#include "KX_NearSensor.h"
#include "SCA_LogicManager.h"
#include "KX_GameObject.h"
#include "KX_TouchEventManager.h"
#include "KX_Scene.h" // needed to create a replica

#include "SM_Object.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
KX_NearSensor::KX_NearSensor(SCA_EventManager* eventmgr,
							 KX_GameObject* gameobj,
							 void *vshape,
							 double margin,
							 double resetmargin,
							 bool bFindMaterial,
							 const STR_String& touchedpropname,
							 class KX_Scene* scene,
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
	m_client_info = new KX_ClientObjectInfo(gameobj);
	m_client_info->m_type = KX_ClientObjectInfo::NEAR;
	
	DT_ShapeHandle shape = (DT_ShapeHandle) vshape;
	m_sumoObj = new SM_Object(shape,NULL,NULL,NULL);
	m_sumoObj->setMargin(m_Margin);
	m_sumoObj->setClientObject(m_client_info);
	
	SynchronizeTransform();
}

KX_NearSensor::KX_NearSensor(SCA_EventManager* eventmgr,
							 KX_GameObject* gameobj,
							 double margin,
							 double resetmargin,
							 bool bFindMaterial,
							 const STR_String& touchedpropname,
							 class KX_Scene* scene,
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
	m_client_info = new KX_ClientObjectInfo(gameobj);
	m_client_info->m_type = KX_ClientObjectInfo::NEAR;
	m_client_info->m_auxilary_info = NULL;
	
	m_sumoObj = new SM_Object(DT_NewSphere(0.0),NULL,NULL,NULL);
	m_sumoObj->setMargin(m_Margin);
	m_sumoObj->setClientObject(m_client_info);
	
	SynchronizeTransform();
}

void KX_NearSensor::RegisterSumo(KX_TouchEventManager *touchman)
{
	touchman->GetSumoScene()->addSensor(*m_sumoObj);
}

CValue* KX_NearSensor::GetReplica()
{
	KX_NearSensor* replica = new KX_NearSensor(*this);
	replica->m_colliders = new CListValue();
	replica->m_bCollision = false;
	replica->m_bTriggered= false;
	replica->m_hitObject = NULL;
	replica->m_bLastTriggered = false;
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	
	replica->m_client_info = new KX_ClientObjectInfo(m_client_info->m_clientobject);
	replica->m_client_info->m_type = KX_ClientObjectInfo::NEAR;
	replica->m_client_info->m_auxilary_info = NULL;
	
	replica->m_sumoObj = new SM_Object(DT_NewSphere(0.0),NULL,NULL,NULL);
	replica->m_sumoObj->setMargin(m_Margin);
	replica->m_sumoObj->setClientObject(replica->m_client_info);
	
	replica->SynchronizeTransform();
	
	return replica;
}



void KX_NearSensor::ReParent(SCA_IObject* parent)
{
	SCA_ISensor::ReParent(parent);
	
	m_client_info->m_clientobject = static_cast<KX_GameObject*>(parent); 
	
	SynchronizeTransform();
}



KX_NearSensor::~KX_NearSensor()
{
	// for nearsensor, the sensor is the 'owner' of sumoobj
	// for touchsensor, it's the parent
	static_cast<KX_TouchEventManager*>(m_eventmgr)->GetSumoScene()->remove(*m_sumoObj);

	if (m_sumoObj)
		delete m_sumoObj;
		
	if (m_client_info)
		delete m_client_info;
}


bool KX_NearSensor::Evaluate(CValue* event)
{
	bool result = false;
	KX_GameObject* parent = static_cast<KX_GameObject*>(GetParent());

	if (m_bTriggered != m_bLastTriggered)
	{
		m_bLastTriggered = m_bTriggered;
		if (m_bTriggered)
		{
			if (m_sumoObj)
			{
				m_sumoObj->setMargin(m_ResetMargin);
			}
		} else
		{
			if (m_sumoObj)
			{
				m_sumoObj->setMargin(m_Margin);
			}

		}
		result = true;
	}

	return result;
}



DT_Bool KX_NearSensor::HandleCollision(void* obj1,void* obj2,const DT_CollData * coll_data)
{
	KX_TouchEventManager* toucheventmgr = static_cast<KX_TouchEventManager*>(m_eventmgr);
	KX_GameObject* parent = static_cast<KX_GameObject*>(GetParent());
	
	// need the mapping from SM_Objects to gameobjects now
	
	KX_ClientObjectInfo* client_info =static_cast<KX_ClientObjectInfo*> (obj1 == m_sumoObj? 
					((SM_Object*)obj2)->getClientObject() : 
					((SM_Object*)obj1)->getClientObject());

	KX_GameObject* gameobj = ( client_info ? 
			static_cast<KX_GameObject*>(client_info->m_clientobject) : 
			NULL);
	
	if (gameobj && (gameobj != parent))
	{
		if (!m_colliders->SearchValue(gameobj))
			m_colliders->Add(gameobj->AddRef());
		// only take valid colliders
		if (client_info->m_type == KX_ClientObjectInfo::ACTOR)
		{
			if ((m_touchedpropname.Length() == 0) || 
				(gameobj->GetProperty(m_touchedpropname)))
			{
				m_bTriggered = true;
				m_hitObject = gameobj;
			}
		}
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
KX_NearSensor::_getattr(char* attr)
{
  _getattr_up(KX_TouchSensor);
}

