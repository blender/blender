/**
 * $Id$
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

#include "SCA_ILogicBrick.h"
#include "PyObjectPlus.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SCA_LogicManager* SCA_ILogicBrick::m_sCurrentLogicManager = NULL;

SCA_ILogicBrick::SCA_ILogicBrick(SCA_IObject* gameobj,
								 PyTypeObject* T)
	:
	CValue(T),
	m_gameobj(gameobj),
	m_Execute_Priority(0),
	m_Execute_Ueber_Priority(0),
	m_bActive(false),
	m_eventval(0)
{
	m_text = "KX_LogicBrick";
}



SCA_ILogicBrick::~SCA_ILogicBrick()
{
	RemoveEvent();
}



void SCA_ILogicBrick::SetExecutePriority(int execute_Priority)
{
	m_Execute_Priority = execute_Priority;
}



void SCA_ILogicBrick::SetUeberExecutePriority(int execute_Priority)
{
	m_Execute_Ueber_Priority = execute_Priority;
}



void SCA_ILogicBrick::ReParent(SCA_IObject* parent)
{
	m_gameobj = parent;
}

void SCA_ILogicBrick::Relink(GEN_Map<GEN_HashedPtr, void*> *obj_map)
{
	// nothing to do
}

CValue* SCA_ILogicBrick::Calc(VALUE_OPERATOR op, CValue *val)
{
	CValue* temp = new CBoolValue(false,"");
	CValue* result = temp->Calc(op,val);
	temp->Release();

	return result;
} 



CValue* SCA_ILogicBrick::CalcFinal(VALUE_DATA_TYPE dtype,
								   VALUE_OPERATOR op,
								   CValue *val)
{
	// same as bool implementation, so...
	CValue* temp = new CBoolValue(false,"");
	CValue* result = temp->CalcFinal(dtype,op,val);
	temp->Release();

	return result;
}



const STR_String& SCA_ILogicBrick::GetText()
{ 
	if (m_name.Length())
		return m_name;

	return m_text;
}



double SCA_ILogicBrick::GetNumber()
{
	return -1;
}



STR_String SCA_ILogicBrick::GetName()
{
	return m_name;
}



void SCA_ILogicBrick::SetName(STR_String name)
{
	m_name = name;
}

bool SCA_ILogicBrick::LessComparedTo(SCA_ILogicBrick* other)
{
	return (this->m_Execute_Ueber_Priority < other->m_Execute_Ueber_Priority) 
		|| ((this->m_Execute_Ueber_Priority == other->m_Execute_Ueber_Priority) && 
		(this->m_Execute_Priority < other->m_Execute_Priority));
}

void SCA_ILogicBrick::RegisterEvent(CValue* eventval)
{
	if (m_eventval)
		m_eventval->Release();

	m_eventval = eventval->AddRef();
}


void SCA_ILogicBrick::RemoveEvent()
{
	if (m_eventval)
	{
		m_eventval->Release();
		m_eventval = NULL;
	}
}



CValue* SCA_ILogicBrick::GetEvent()
{
	if (m_eventval)
	{
		return m_eventval->AddRef();
	}
	
	return NULL;
}




/* python stuff */

PyTypeObject SCA_ILogicBrick::Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif
	"SCA_ILogicBrick",
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



PyParentObject SCA_ILogicBrick::Parents[] = {
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};



PyMethodDef SCA_ILogicBrick::Methods[] = {
	// --> Deprecated
  {"getOwner", (PyCFunction) SCA_ILogicBrick::sPyGetOwner, METH_NOARGS},
  {"getExecutePriority", (PyCFunction) SCA_ILogicBrick::sPyGetExecutePriority, METH_NOARGS},
  {"setExecutePriority", (PyCFunction) SCA_ILogicBrick::sPySetExecutePriority, METH_VARARGS},
  // <-- Deprecated
  {NULL,NULL} //Sentinel
};

PyAttributeDef SCA_ILogicBrick::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("owner",	SCA_ILogicBrick, pyattr_get_owner),
	KX_PYATTRIBUTE_INT_RW("executePriority",0,100000,false,SCA_ILogicBrick,m_Execute_Ueber_Priority),
	KX_PYATTRIBUTE_STRING_RO("name", SCA_ILogicBrick, m_name),
	{NULL} //Sentinel
};

int SCA_ILogicBrick::CheckProperty(void *self, const PyAttributeDef *attrdef)
{
	if (attrdef->m_type != KX_PYATTRIBUTE_TYPE_STRING || attrdef->m_length != 1) {
		PyErr_SetString(PyExc_AttributeError, "inconsistent check function for attribute type, report to blender.org");
		return 1;
	}
	SCA_ILogicBrick* brick = reinterpret_cast<SCA_ILogicBrick*>(self);
	STR_String* var = reinterpret_cast<STR_String*>((char*)self+attrdef->m_offset);
	CValue* prop = brick->GetParent()->FindIdentifier(*var);
	bool error = prop->IsError();
	prop->Release();
	if (error) {
		PyErr_SetString(PyExc_ValueError, "string does not correspond to a property");
		return 1;
	}
	return 0;
}

PyObject* SCA_ILogicBrick::py_getattro(PyObject *attr)
{
  py_getattro_up(CValue);
}

PyObject* SCA_ILogicBrick::py_getattro_dict() {
	py_getattro_dict_up(CValue);
}

int SCA_ILogicBrick::py_setattro(PyObject *attr, PyObject *value)
{
	py_setattro_up(CValue);
}


PyObject* SCA_ILogicBrick::PyGetOwner()
{
	ShowDeprecationWarning("getOwner()", "the owner property");
	
	CValue* parent = GetParent();
	if (parent)
	{
		return parent->GetProxy();
	}

	printf("ERROR: Python scriptblock without owner\n");
	Py_RETURN_NONE; //Int_FromLong(IsPositiveTrigger());
}



PyObject* SCA_ILogicBrick::PySetExecutePriority(PyObject* args)
{
	ShowDeprecationWarning("setExecutePriority()", "the executePriority property");

	int priority=0;

    if (!PyArg_ParseTuple(args, "i:setExecutePriority", &priority)) {
		return NULL;
    }
	
	m_Execute_Ueber_Priority = priority;

	Py_RETURN_NONE;
}



PyObject* SCA_ILogicBrick::PyGetExecutePriority()
{
	ShowDeprecationWarning("getExecutePriority()", "the executePriority property");
	return PyInt_FromLong(m_Execute_Ueber_Priority);
}


/*Attribute functions */
PyObject* SCA_ILogicBrick::pyattr_get_owner(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	SCA_ILogicBrick* self= static_cast<SCA_ILogicBrick*>(self_v);
	CValue* parent = self->GetParent();
	
	if (parent)
		return parent->GetProxy();
	
	Py_RETURN_NONE;
}



/* Conversions for making life better. */
bool SCA_ILogicBrick::PyArgToBool(int boolArg)
{
	if (boolArg) {
		return true;
	} else {
		return false;
	}
}

PyObject* SCA_ILogicBrick::BoolToPyArg(bool boolarg)
{
	return PyInt_FromLong(boolarg? KX_TRUE: KX_FALSE);	
}
