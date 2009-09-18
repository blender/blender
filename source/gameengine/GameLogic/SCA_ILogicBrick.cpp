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

SCA_ILogicBrick::SCA_ILogicBrick(SCA_IObject* gameobj)
	:
	CValue(),
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



STR_String& SCA_ILogicBrick::GetName()
{
	return m_name;
}



void SCA_ILogicBrick::SetName(const char *name)
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
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_ILogicBrick",
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
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_ILogicBrick::Methods[] = {
  {NULL,NULL} //Sentinel
};

PyAttributeDef SCA_ILogicBrick::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("owner",	SCA_ILogicBrick, pyattr_get_owner),
	KX_PYATTRIBUTE_INT_RW("executePriority",0,100000,false,SCA_ILogicBrick,m_Execute_Priority),
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
	return PyLong_FromSsize_t(boolarg? KX_TRUE: KX_FALSE);	
}
