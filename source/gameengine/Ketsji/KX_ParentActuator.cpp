/**
 * Set or remove an objects parent
 *
 * $Id: SCA_ParentActuator.cpp 13932 2008-03-01 19:05:41Z ben2610 $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * ***** END GPL LICENSE BLOCK *****
 */

#include "KX_ParentActuator.h"
#include "KX_GameObject.h"
#include "KX_PythonInit.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* ------------------------------------------------------------------------- */
/* Native functions                                                          */
/* ------------------------------------------------------------------------- */

KX_ParentActuator::KX_ParentActuator(SCA_IObject *gameobj, 
									 int mode,
									 SCA_IObject *ob,
									 PyTypeObject* T)
	: SCA_IActuator(gameobj, T),
	  m_mode(mode),
	  m_ob(ob)
{
	if (m_ob)
		m_ob->RegisterActuator(this);
} 



KX_ParentActuator::~KX_ParentActuator()
{
	if (m_ob)
		m_ob->UnregisterActuator(this);
} 



CValue* KX_ParentActuator::GetReplica()
{
	KX_ParentActuator* replica = new KX_ParentActuator(*this);
	// replication just copy the m_base pointer => common random generator
	replica->ProcessReplica();
	CValue::AddDataToReplica(replica);

	return replica;
}

void KX_ParentActuator::ProcessReplica()
{
	if (m_ob)
		m_ob->RegisterActuator(this);
	SCA_IActuator::ProcessReplica();
}


bool KX_ParentActuator::UnlinkObject(SCA_IObject* clientobj)
{
	if (clientobj == m_ob)
	{
		// this object is being deleted, we cannot continue to track it.
		m_ob = NULL;
		return true;
	}
	return false;
}

void KX_ParentActuator::Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map)
{
	void **h_obj = (*obj_map)[m_ob];
	if (h_obj) {
		if (m_ob)
			m_ob->UnregisterActuator(this);
		m_ob = (SCA_IObject*)(*h_obj);
		m_ob->RegisterActuator(this);
	}
}



bool KX_ParentActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();

	if (bNegativeEvent)
		return false; // do nothing on negative events

	KX_GameObject *obj = (KX_GameObject*) GetParent();
	KX_Scene *scene = PHY_GetActiveScene();
	switch (m_mode) {
		case KX_PARENT_SET:
			if (m_ob)
				obj->SetParent(scene, (KX_GameObject*)m_ob);
			break;
		case KX_PARENT_REMOVE:
			obj->RemoveParent(scene);
			break;
	};
	
	return false;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject KX_ParentActuator::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"KX_ParentActuator",
	sizeof(KX_ParentActuator),
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

PyParentObject KX_ParentActuator::Parents[] = {
	&KX_ParentActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_ParentActuator::Methods[] = {
	{"setObject",         (PyCFunction) KX_ParentActuator::sPySetObject, METH_VARARGS, SetObject_doc},
	{"getObject",         (PyCFunction) KX_ParentActuator::sPyGetObject, METH_VARARGS, GetObject_doc},
	{NULL,NULL} //Sentinel
};

PyObject* KX_ParentActuator::_getattr(const STR_String& attr) {
	_getattr_up(SCA_IActuator);
}

/* 1. setObject                                                            */
char KX_ParentActuator::SetObject_doc[] = 
"setObject(object)\n"
"\tSet the object to set as parent.\n"
"\tCan be an object name or an object\n";
PyObject* KX_ParentActuator::PySetObject(PyObject* self, PyObject* args, PyObject* kwds) {
	PyObject* gameobj;
	if (PyArg_ParseTuple(args, "O!", &KX_GameObject::Type, &gameobj))
	{
		if (m_ob != NULL)
			m_ob->UnregisterActuator(this);
		m_ob = (SCA_IObject*)gameobj;
		if (m_ob)
			m_ob->RegisterActuator(this);
		Py_Return;
	}
	PyErr_Clear();
	
	char* objectname;
	if (PyArg_ParseTuple(args, "s", &objectname))
	{
		SCA_IObject *object = (SCA_IObject*)SCA_ILogicBrick::m_sCurrentLogicManager->GetGameObjectByName(STR_String(objectname));
		if(object)
		{
			if (m_ob != NULL)
				m_ob->UnregisterActuator(this);
			m_ob = object;
			m_ob->RegisterActuator(this);
			Py_Return;
		}
	}
	
	return NULL;
}

/* 2. getObject                                                            */
char KX_ParentActuator::GetObject_doc[] =
"getObject()\n"
"\tReturns the object that is set to.\n";
PyObject* KX_ParentActuator::PyGetObject(PyObject* self, PyObject* args, PyObject* kwds) {
	return PyString_FromString(m_ob->GetName());
}
	
/* eof */
