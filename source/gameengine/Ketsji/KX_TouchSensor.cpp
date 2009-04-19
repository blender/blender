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

#include "KX_TouchSensor.h"
#include "SCA_EventManager.h"
#include "SCA_LogicManager.h"
#include "KX_GameObject.h"
#include "KX_TouchEventManager.h"

#include "PHY_IPhysicsController.h"

#include <iostream>
#include "PHY_IPhysicsEnvironment.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
	m_eventmgr->RemoveSensor(this);
}

bool KX_TouchSensor::Evaluate(CValue* event)
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

KX_TouchSensor::KX_TouchSensor(SCA_EventManager* eventmgr,KX_GameObject* gameobj,bool bFindMaterial,bool bTouchPulse,const STR_String& touchedpropname,PyTypeObject* T)
:SCA_ISensor(gameobj,eventmgr,T),
m_touchedpropname(touchedpropname),
m_bFindMaterial(bFindMaterial),
m_bTouchPulse(bTouchPulse),
m_eventmgr(eventmgr)
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
	replica->m_colliders = new CListValue();
	replica->Init();
	// this will copy properties and so on...
	CValue::AddDataToReplica(replica);
	return replica;
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
		touchman->GetPhysicsEnvironment()->requestCollisionCallback(m_physCtrl);
		// collision
		// Deprecated	

	}
}

void KX_TouchSensor::UnregisterSumo(KX_TouchEventManager* touchman)
{
	if (m_physCtrl)
	{
		touchman->GetPhysicsEnvironment()->removeCollisionCallback(m_physCtrl);
	}
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


/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */
/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_TouchSensor::Type = {
	PyObject_HEAD_INIT(NULL)
	0,
	"KX_TouchSensor",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,
	py_base_getattro,
	py_base_setattro,
	0,0,0,0,0,0,0,0,0,
	Methods
};

PyParentObject KX_TouchSensor::Parents[] = {
	&KX_TouchSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_TouchSensor::Methods[] = {
	//Deprecated ----->
	{"setProperty", 
	 (PyCFunction) KX_TouchSensor::sPySetProperty,      METH_O, (PY_METHODCHAR)SetProperty_doc},
	{"getProperty", 
	 (PyCFunction) KX_TouchSensor::sPyGetProperty,      METH_NOARGS, (PY_METHODCHAR)GetProperty_doc},
	{"getHitObject", 
	 (PyCFunction) KX_TouchSensor::sPyGetHitObject,     METH_NOARGS, (PY_METHODCHAR)GetHitObject_doc},
	{"getHitObjectList", 
	 (PyCFunction) KX_TouchSensor::sPyGetHitObjectList, METH_NOARGS, (PY_METHODCHAR)GetHitObjectList_doc},
	 //<-----
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_TouchSensor::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW("property",0,100,false,KX_TouchSensor,m_touchedpropname),
	KX_PYATTRIBUTE_BOOL_RW("useMaterial",KX_TouchSensor,m_bFindMaterial),
	KX_PYATTRIBUTE_BOOL_RW("pulseCollisions",KX_TouchSensor,m_bTouchPulse),
	KX_PYATTRIBUTE_RO_FUNCTION("objectHit", KX_TouchSensor, pyattr_get_object_hit),
	KX_PYATTRIBUTE_RO_FUNCTION("objectHitList", KX_TouchSensor, pyattr_get_object_hit_list),
	{ NULL }	//Sentinel
};

PyObject* KX_TouchSensor::py_getattro(PyObject *attr)
{
	py_getattro_up(SCA_ISensor);
}

int KX_TouchSensor::py_setattro(PyObject *attr, PyObject *value)
{
	py_setattro_up(SCA_ISensor);
}

/* Python API */

/* 1. setProperty */
const char KX_TouchSensor::SetProperty_doc[] = 
"setProperty(name)\n"
"\t- name: string\n"
"\tSet the property or material to collide with. Use\n"
"\tsetTouchMaterial() to switch between properties and\n"
"\tmaterials.";
PyObject* KX_TouchSensor::PySetProperty(PyObject* value)
{
	ShowDeprecationWarning("setProperty()", "the propertyName property");
	char *nameArg= PyString_AsString(value);
	if (nameArg==NULL) {
		PyErr_SetString(PyExc_ValueError, "expected a ");
		return NULL;
	}
	
	m_touchedpropname = nameArg;
	Py_RETURN_NONE;
}
/* 2. getProperty */
const char KX_TouchSensor::GetProperty_doc[] = 
"getProperty(name)\n"
"\tReturns the property or material to collide with. Use\n"
"\tgetTouchMaterial() to find out whether this sensor\n"
"\tlooks for properties or materials.";
PyObject*  KX_TouchSensor::PyGetProperty() {
	return PyString_FromString(m_touchedpropname);
}

const char KX_TouchSensor::GetHitObject_doc[] = 
"getHitObject()\n"
;
PyObject* KX_TouchSensor::PyGetHitObject()
{
	ShowDeprecationWarning("getHitObject()", "the objectHit property");
	/* to do: do Py_IncRef if the object is already known in Python */
	/* otherwise, this leaks memory */
	if (m_hitObject)
	{
		return m_hitObject->GetProxy();
	}
	Py_RETURN_NONE;
}

const char KX_TouchSensor::GetHitObjectList_doc[] = 
"getHitObjectList()\n"
"\tReturn a list of the objects this object collided with,\n"
"\tbut only those matching the property/material condition.\n";
PyObject* KX_TouchSensor::PyGetHitObjectList()
{
	ShowDeprecationWarning("getHitObjectList()", "the objectHitList property");
	/* to do: do Py_IncRef if the object is already known in Python */
	/* otherwise, this leaks memory */ /* Edit, this seems ok and not to leak memory - Campbell */
	return m_colliders->GetProxy();
}

/*getTouchMaterial and setTouchMaterial were never added to the api,
they can probably be removed with out anyone noticing*/

/* 5. getTouchMaterial */
const char KX_TouchSensor::GetTouchMaterial_doc[] = 
"getTouchMaterial()\n"
"\tReturns KX_TRUE if this sensor looks for a specific material,\n"
"\tKX_FALSE if it looks for a specific property.\n" ;
PyObject* KX_TouchSensor::PyGetTouchMaterial()
{
	ShowDeprecationWarning("getTouchMaterial()", "the materialCheck property");
	return PyInt_FromLong(m_bFindMaterial);
}

/* 6. setTouchMaterial */
#if 0
const char KX_TouchSensor::SetTouchMaterial_doc[] = 
"setTouchMaterial(flag)\n"
"\t- flag: KX_TRUE or KX_FALSE.\n"
"\tSet flag to KX_TRUE to switch on positive pulse mode,\n"
"\tKX_FALSE to switch off positive pulse mode.\n" ;
PyObject* KX_TouchSensor::PySetTouchMaterial(PyObject *value)
{
	int pulseArg = PyInt_AsLong(value);

	if(pulseArg ==-1 && PyErr_Occurred()) {
		PyErr_SetString(PyExc_ValueError, "expected a bool");
		return NULL;
	}
	
	m_bFindMaterial = pulseArg != 0;

	Py_RETURN_NONE;
}
#endif

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


/* eof */
