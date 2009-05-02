/**
 * Set or remove an objects parent
 *
 * $Id$
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

#include "PyObjectPlus.h" 

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
	KX_Scene *scene = KX_GetActiveScene();
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
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif
	"KX_ParentActuator",
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

PyParentObject KX_ParentActuator::Parents[] = {
	&KX_ParentActuator::Type,
	&SCA_IActuator::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef KX_ParentActuator::Methods[] = {
	// Deprecated ----->
	{"setObject", (PyCFunction) KX_ParentActuator::sPySetObject, METH_O, (PY_METHODCHAR)SetObject_doc},
	{"getObject", (PyCFunction) KX_ParentActuator::sPyGetObject, METH_VARARGS, (PY_METHODCHAR)GetObject_doc},
	// <-----
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_ParentActuator::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("object", KX_ParentActuator, pyattr_get_object, pyattr_set_object),
	KX_PYATTRIBUTE_INT_RW("mode", KX_PARENT_NODEF+1, KX_PARENT_MAX-1, true, KX_ParentActuator, m_mode),
	{ NULL }	//Sentinel
};

PyObject* KX_ParentActuator::pyattr_get_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_ParentActuator* actuator = static_cast<KX_ParentActuator*>(self);
	if (!actuator->m_ob)	
		Py_RETURN_NONE;
	else
		return actuator->m_ob->GetProxy();
}

int KX_ParentActuator::pyattr_set_object(void *self, const struct KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_ParentActuator* actuator = static_cast<KX_ParentActuator*>(self);
	KX_GameObject *gameobj;
		
	if (!ConvertPythonToGameObject(value, &gameobj, true, "actuator.object = value: KX_ParentActuator"))
		return 1; // ConvertPythonToGameObject sets the error
		
	if (actuator->m_ob != NULL)
		actuator->m_ob->UnregisterActuator(actuator);	

	actuator->m_ob = (SCA_IObject*) gameobj;
		
	if (actuator->m_ob)
		actuator->m_ob->RegisterActuator(actuator);
		
	return 0;
}


PyObject* KX_ParentActuator::py_getattro(PyObject *attr) {	
	py_getattro_up(SCA_IActuator);
}

PyObject* KX_ParentActuator::py_getattro_dict() {
	py_getattro_dict_up(SCA_IActuator);
}

int KX_ParentActuator::py_setattro(PyObject *attr, PyObject* value) {
	py_setattro_up(SCA_IActuator);
}

/* Deprecated -----> */
/* 1. setObject                                                            */
const char KX_ParentActuator::SetObject_doc[] = 
"setObject(object)\n"
"\t- object: KX_GameObject, string or None\n"
"\tSet the object to set as parent.\n";
PyObject* KX_ParentActuator::PySetObject(PyObject* value) {
	KX_GameObject *gameobj;
	
	ShowDeprecationWarning("setObject()", "the object property");
	
	if (!ConvertPythonToGameObject(value, &gameobj, true, "actuator.setObject(value): KX_ParentActuator"))
		return NULL; // ConvertPythonToGameObject sets the error
	
	if (m_ob != NULL)
		m_ob->UnregisterActuator(this);	

	m_ob = (SCA_IObject*)gameobj;
	if (m_ob)
		m_ob->RegisterActuator(this);
	
	Py_RETURN_NONE;
}

/* 2. getObject                                                            */

/* get obj  ---------------------------------------------------------- */
const char KX_ParentActuator::GetObject_doc[] = 
"getObject(name_only = 1)\n"
"name_only - optional arg, when true will return the KX_GameObject rather then its name\n"
"\tReturns the object that is set to.\n";
PyObject* KX_ParentActuator::PyGetObject(PyObject* args)
{
	int ret_name_only = 1;
	
	ShowDeprecationWarning("getObject()", "the object property");
	
	if (!PyArg_ParseTuple(args, "|i:getObject", &ret_name_only))
		return NULL;
	
	if (!m_ob)
		Py_RETURN_NONE;
	
	if (ret_name_only)
		return PyString_FromString(m_ob->GetName());
	else
		return m_ob->GetProxy();
}
/* <----- */

/* eof */
