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
#include "KX_SumoPhysicsController.h"
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
	m_bTriggered = false;
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
	return result;
}

KX_TouchSensor::KX_TouchSensor(SCA_EventManager* eventmgr,KX_GameObject* gameobj,bool bFindMaterial,const STR_String& touchedpropname,PyTypeObject* T)
:SCA_ISensor(gameobj,eventmgr,T),
m_touchedpropname(touchedpropname),
m_bFindMaterial(bFindMaterial),
m_eventmgr(eventmgr)
/*m_sumoObj(sumoObj),*/
{
//	KX_TouchEventManager* touchmgr = (KX_TouchEventManager*) eventmgr;
//	m_resptable = touchmgr->GetResponseTable();
	
//	m_solidHandle = m_sumoObj->getObjectHandle();

	m_colliders = new CListValue();
	
	KX_ClientObjectInfo *client_info = gameobj->getClientInfo();
	client_info->m_gameobject = gameobj;
	client_info->m_auxilary_info = NULL;
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
	client_info->m_gameobject = gameobj;
	client_info->m_auxilary_info = NULL;
	
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
	
	if (gameobj && (gameobj != parent) && client_info->isActor())
	{
		if (!m_colliders->SearchValue(gameobj))
			m_colliders->Add(gameobj->AddRef());
		
		bool found = m_touchedpropname.IsEmpty();
		if (!found)
		{
			if (m_bFindMaterial)
			{
				if (client_info->m_auxilary_info)
				{
					found = (m_touchedpropname == STR_String((char*)client_info->m_auxilary_info));
				}
			} else
			{
				found = (gameobj->GetProperty(m_touchedpropname) != NULL);
			}
		}
		if (found)
		{
			m_bTriggered = true;
			m_hitObject = gameobj;
			//printf("KX_TouchSensor::HandleCollision\n");
		}
		
	} 
	return DT_CONTINUE;
}


/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */
/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_TouchSensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_TouchSensor",
	sizeof(KX_TouchSensor),
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

PyParentObject KX_TouchSensor::Parents[] = {
	&KX_TouchSensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_TouchSensor::Methods[] = {
	{"setProperty", 
	 (PyCFunction) KX_TouchSensor::sPySetProperty,      METH_VARARGS, SetProperty_doc},
	{"getProperty", 
	 (PyCFunction) KX_TouchSensor::sPyGetProperty,      METH_VARARGS, GetProperty_doc},
	{"getHitObject", 
	 (PyCFunction) KX_TouchSensor::sPyGetHitObject,     METH_VARARGS, GetHitObject_doc},
	{"getHitObjectList", 
	 (PyCFunction) KX_TouchSensor::sPyGetHitObjectList, METH_VARARGS, GetHitObjectList_doc},
	{NULL,NULL} //Sentinel
};

PyObject* KX_TouchSensor::_getattr(const STR_String& attr) {
	_getattr_up(SCA_ISensor);
}

/* Python API */

/* 1. setProperty */
char KX_TouchSensor::SetProperty_doc[] = 
"setProperty(name)\n"
"\t- name: string\n"
"\tSet the property or material to collide with. Use\n"
"\tsetTouchMaterial() to switch between properties and\n"
"\tmaterials.";
PyObject* KX_TouchSensor::PySetProperty(PyObject* self, 
										PyObject* args, 
										PyObject* kwds) {
	char *nameArg;
	if (!PyArg_ParseTuple(args, "s", &nameArg)) {
		return NULL;
	}

	CValue* prop = GetParent()->FindIdentifier(nameArg);

	if (!prop->IsError()) {
		m_touchedpropname = nameArg;
	} else {
		; /* not found ... */
	}
	prop->Release();
	
	Py_Return;
}
/* 2. getProperty */
char KX_TouchSensor::GetProperty_doc[] = 
"getProperty(name)\n"
"\tReturns the property or material to collide with. Use\n"
"\tgetTouchMaterial() to find out whether this sensor\n"
"\tlooks for properties or materials.";
PyObject*  KX_TouchSensor::PyGetProperty(PyObject* self, 
										 PyObject* args, 
										 PyObject* kwds) {
	return PyString_FromString(m_touchedpropname);
}

char KX_TouchSensor::GetHitObject_doc[] = 
"getHitObject()\n"
;
PyObject* KX_TouchSensor::PyGetHitObject(PyObject* self, 
										 PyObject* args, 
										 PyObject* kwds)
{
	/* to do: do Py_IncRef if the object is already known in Python */
	/* otherwise, this leaks memory */
	if (m_hitObject)
	{
		return m_hitObject->AddRef();
	}
	Py_Return;
}

char KX_TouchSensor::GetHitObjectList_doc[] = 
"getHitObjectList()\n"
"\tReturn a list of the objects this object collided with,\n"
"\tbut only those matching the property/material condition.\n";
PyObject* KX_TouchSensor::PyGetHitObjectList(PyObject* self, 
										 PyObject* args, 
										 PyObject* kwds)
{

	/* to do: do Py_IncRef if the object is already known in Python */
	/* otherwise, this leaks memory */

	if ( m_touchedpropname.IsEmpty() ) {
		return m_colliders->AddRef();
	} else {
		CListValue* newList = new CListValue();
		int i = 0;
		while (i < m_colliders->GetCount()) {
			if (m_bFindMaterial) {
				/* need to associate the CValues from the list to material
				 * names. The collider list _should_ contains only
				 * KX_GameObjects. I am loathe to cast them, though... The
				 * material name must be retrieved from Sumo. To a Sumo
				 * object, a client-info block is attached. This block
				 * contains the material name. 
				 * - this also doesn't work (obviously) for multi-materials... 
				 */
				KX_GameObject* gameob = (KX_GameObject*) m_colliders->GetValue(i);
				PHY_IPhysicsController* spc = dynamic_cast<PHY_IPhysicsController*>(gameob->GetPhysicsController());
				
				if (spc) {
					KX_ClientObjectInfo* cl_inf = static_cast<KX_ClientObjectInfo*>(spc->getNewClientInfo());
					
					if (m_touchedpropname == ((char*)cl_inf->m_auxilary_info)) {
						newList->Add(m_colliders->GetValue(i)->AddRef());
					} 
				}
				
			} else {
				CValue* val = m_colliders->GetValue(i)->FindIdentifier(m_touchedpropname);
				if (!val->IsError()) {
					newList->Add(m_colliders->GetValue(i)->AddRef());
				}
				val->Release();
			}
			
			i++;
		}
		return newList->AddRef();
	}

}

/* 5. getTouchMaterial */
char KX_TouchSensor::GetTouchMaterial_doc[] = 
"getTouchMaterial()\n"
"\tReturns KX_TRUE if this sensor looks for a specific material,\n"
"\tKX_FALSE if it looks for a specific property.\n" ;
PyObject* KX_TouchSensor::PyGetTouchMaterial(PyObject* self, 
											 PyObject* args, 
											 PyObject* kwds)
{
	return PyInt_FromLong(m_bFindMaterial);
}

/* 6. setTouchMaterial */
char KX_TouchSensor::SetTouchMaterial_doc[] = 
"setTouchMaterial(flag)\n"
"\t- flag: KX_TRUE or KX_FALSE.\n"
"\tSet flag to KX_TRUE to switch on positive pulse mode,\n"
"\tKX_FALSE to switch off positive pulse mode.\n" ;
PyObject* KX_TouchSensor::PySetTouchMaterial(PyObject* self, PyObject* args, PyObject* kwds)
{
	int pulseArg = 0;

	if(!PyArg_ParseTuple(args, "i", &pulseArg)) {
		return NULL;
	}
	
	m_bFindMaterial = pulseArg != 0;

	Py_Return;
}


/* eof */
