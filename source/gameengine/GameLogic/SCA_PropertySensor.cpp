/**
 * Property sensor
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

#include <iostream>
#include "SCA_PropertySensor.h"
#include "Operator2Expr.h"
#include "ConstExpr.h"
#include "InputParser.h"
#include "StringValue.h"
#include "SCA_EventManager.h"
#include "SCA_LogicManager.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

SCA_PropertySensor::SCA_PropertySensor(SCA_EventManager* eventmgr,
									 SCA_IObject* gameobj,
									 const STR_String& propname,
									 const STR_String& propval,
									 const STR_String& propmaxval,
									 KX_PROPSENSOR_TYPE checktype,
									 PyTypeObject* T )
	: SCA_ISensor(gameobj,eventmgr,T),
	  m_checktype(checktype),
	  m_checkpropval(propval),
	  m_checkpropmaxval(propmaxval),
	  m_checkpropname(propname),
	  m_range_expr(NULL)
{
	//CParser pars;
	//pars.SetContext(this->AddRef());
	//CValue* resultval = m_rightexpr->Calculate();

	CValue* orgprop = GetParent()->FindIdentifier(m_checkpropname);
	if (!orgprop->IsError())
	{
		m_previoustext = orgprop->GetText();
	}
	orgprop->Release();

	if (m_checktype==KX_PROPSENSOR_INTERVAL)
	{
		PrecalculateRangeExpression();
	}
	Init();
}

void SCA_PropertySensor::Init()
{
	m_recentresult = false;
	m_lastresult = m_invert?true:false;
}

void SCA_PropertySensor::PrecalculateRangeExpression()
{
		CParser pars;
		//The context is needed to retrieve the property at runtime but it creates
		//loop of references
		pars.SetContext(this->AddRef());
		STR_String checkstr = "(" + m_checkpropval + " <= " 
							+ m_checkpropname + ") && ( " 
							+ m_checkpropname + " <= " 
							+ m_checkpropmaxval + ")";

		m_range_expr = pars.ProcessText(checkstr);
}

// Forced deletion of precalculated range expression to break reference loop
// Use this function when you know that you won't use the sensor anymore
void SCA_PropertySensor::Delete()
{
	if (m_range_expr)
	{
		m_range_expr->Release();
		m_range_expr = NULL;
	}
	Release();
}

CValue* SCA_PropertySensor::GetReplica()
{
	SCA_PropertySensor* replica = new SCA_PropertySensor(*this);
	// m_range_expr must be recalculated on replica!
	CValue::AddDataToReplica(replica);

	replica->m_range_expr = NULL;
	if (replica->m_checktype==KX_PROPSENSOR_INTERVAL)
	{
		replica->PrecalculateRangeExpression();
	}
	
	
	return replica;
}



bool SCA_PropertySensor::IsPositiveTrigger()
{
	bool result = m_recentresult;//CheckPropertyCondition();
	if (m_invert)
		result = !result;

	return result;
}



SCA_PropertySensor::~SCA_PropertySensor()
{
	//if (m_rightexpr)
	//	m_rightexpr->Release();

	if (m_range_expr)
	{
		m_range_expr->Release();
		m_range_expr=NULL;
	}

}



bool SCA_PropertySensor::Evaluate(CValue* event)
{
	bool result = CheckPropertyCondition();
	
	if (m_lastresult!=result)
	{
		m_lastresult = result;
		return true;
	}

	return false;
}


bool	SCA_PropertySensor::CheckPropertyCondition()
{

	m_recentresult=false;
	bool result=false;
	bool reverse = false;
	switch (m_checktype)
	{
	case KX_PROPSENSOR_NOTEQUAL:
		reverse = true;
	case KX_PROPSENSOR_EQUAL:
		{
			CValue* orgprop = GetParent()->FindIdentifier(m_checkpropname);
			if (!orgprop->IsError())
			{
				STR_String testprop = orgprop->GetText();
				// Force strings to upper case, to avoid confusion in
				// bool tests. It's stupid the prop's identity is lost
				// on the way here...
				if ((testprop == "TRUE") || (testprop == "FALSE")) {
					STR_String checkprop = m_checkpropval;
					checkprop.Upper();
					result = (testprop == checkprop);
				} else {
					result = (orgprop->GetText() == m_checkpropval);
				}
			}
			orgprop->Release();

			if (reverse)
				result = !result;
			break;

		}

	case KX_PROPSENSOR_EXPRESSION:
		{
			/*
			if (m_rightexpr)
			{
				CValue* resultval = m_rightexpr->Calculate();
				if (resultval->IsError())
				{
					int i=0;
					STR_String errortest = resultval->GetText();
					printf(errortest);

				} else
				{
					result = resultval->GetNumber() != 0;
				}
			}
			*/
			break;
		}
	case KX_PROPSENSOR_INTERVAL:
		{
			//CValue* orgprop = GetParent()->FindIdentifier(m_checkpropname);
			//if (orgprop)
			//{
				if (m_range_expr)
				{
					CValue* vallie = m_range_expr->Calculate();
					if (vallie)
					{
						STR_String errtext = vallie->GetText();
						if (errtext == "TRUE")
						{
							result = true;
						} else
						{
							if (vallie->IsError())
							{
								//printf (errtext.ReadPtr());
							} 
						}
						
						vallie->Release();
					}
				}

				
			//}
			
		//cout << " \nSens:Prop:interval!"; /* need implementation here!!! */

		break;
		}
	case KX_PROPSENSOR_CHANGED:
		{
			CValue* orgprop = GetParent()->FindIdentifier(m_checkpropname);
				
			if (!orgprop->IsError())
			{
				if (m_previoustext != orgprop->GetText())
				{
					m_previoustext = orgprop->GetText();
					result = true;
				}
			}
			orgprop->Release();

			//cout << " \nSens:Prop:changed!"; /* need implementation here!!! */
			break;
		}
	default:
		; /* error */
	}

	//the concept of Edge and Level triggering has unwanted effect for KX_PROPSENSOR_CHANGED
	//see Game Engine bugtracker [ #3809 ]
	if (m_checktype != KX_PROPSENSOR_CHANGED)
	{
		m_recentresult=result;
	} else
	{
		m_recentresult=result;//true;
	}
	return result;
}

CValue* SCA_PropertySensor::FindIdentifier(const STR_String& identifiername)
{
	return  GetParent()->FindIdentifier(identifiername);
}

bool SCA_PropertySensor::validValueForProperty(char *val, STR_String &prop)
{
	bool result = true;
	/*  There is no type checking at this moment, unfortunately...           */
	return result;
}

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_PropertySensor::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,
	"SCA_PropertySensor",
	sizeof(SCA_PropertySensor),
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

PyParentObject SCA_PropertySensor::Parents[] = {
	&SCA_PropertySensor::Type,
	&SCA_ISensor::Type,
	&SCA_ILogicBrick::Type,
	&CValue::Type,
	NULL
};

PyMethodDef SCA_PropertySensor::Methods[] = {
	{"getType", (PyCFunction) SCA_PropertySensor::sPyGetType, METH_VARARGS, GetType_doc},
	{"setType", (PyCFunction) SCA_PropertySensor::sPySetType, METH_VARARGS, SetType_doc},
	{"getProperty", (PyCFunction) SCA_PropertySensor::sPyGetProperty, METH_VARARGS, GetProperty_doc},
	{"setProperty", (PyCFunction) SCA_PropertySensor::sPySetProperty, METH_VARARGS, SetProperty_doc},
	{"getValue", (PyCFunction) SCA_PropertySensor::sPyGetValue, METH_VARARGS, GetValue_doc},
	{"setValue", (PyCFunction) SCA_PropertySensor::sPySetValue, METH_VARARGS, SetValue_doc},
	{NULL,NULL} //Sentinel
};

PyObject* SCA_PropertySensor::_getattr(const STR_String& attr) {
	_getattr_up(SCA_ISensor); /* implicit return! */
}

/* 1. getType */
char SCA_PropertySensor::GetType_doc[] = 
"getType()\n"
"\tReturns the type of check this sensor performs.\n";
PyObject* SCA_PropertySensor::PyGetType(PyObject* self, PyObject* args, PyObject* kwds)
{
	return PyInt_FromLong(m_checktype);
}

/* 2. setType */
char SCA_PropertySensor::SetType_doc[] = 
"setType(type)\n"
"\t- type: KX_PROPSENSOR_EQUAL, KX_PROPSENSOR_NOTEQUAL,\n"
"\t        KX_PROPSENSOR_INTERVAL, KX_PROPSENSOR_CHANGED,\n"
"\t        or KX_PROPSENSOR_EXPRESSION.\n"
"\tSet the type of check to perform.\n";
PyObject* SCA_PropertySensor::PySetType(PyObject* self, PyObject* args, PyObject* kwds) 
{
	int typeArg;
	
	if (!PyArg_ParseTuple(args, "i", &typeArg)) {
		return NULL;
	}
	
	if ( (typeArg > KX_PROPSENSOR_NODEF) 
		 && (typeArg < KX_PROPSENSOR_MAX) ) {
		m_checktype =  typeArg;
	}
	
	Py_Return;
}

/* 3. getProperty */
char SCA_PropertySensor::GetProperty_doc[] = 
"getProperty()\n"
"\tReturn the property with which the sensor operates.\n";
PyObject* SCA_PropertySensor::PyGetProperty(PyObject* self, PyObject* args, PyObject* kwds) 
{
	return PyString_FromString(m_checkpropname);
}

/* 4. setProperty */
char SCA_PropertySensor::SetProperty_doc[] = 
"setProperty(name)\n"
"\t- name: string\n"
"\tSets the property with which to operate. If there is no property\n"
"\tof this name, the call is ignored.\n";
PyObject* SCA_PropertySensor::PySetProperty(PyObject* self, PyObject* args, PyObject* kwds) 
{
	/* We should query whether the name exists. Or should we create a prop   */
	/* on the fly?                                                           */
	char *propNameArg = NULL;

	if (!PyArg_ParseTuple(args, "s", &propNameArg)) {
		return NULL;
	}

	CValue *prop = FindIdentifier(STR_String(propNameArg));
	if (!prop->IsError()) {
		m_checkpropname = propNameArg;
	} else {
		; /* error: bad property name */
	}
	prop->Release();
	Py_Return;
}

/* 5. getValue */
char SCA_PropertySensor::GetValue_doc[] = 
"getValue()\n"
"\tReturns the value with which the sensor operates.\n";
PyObject* SCA_PropertySensor::PyGetValue(PyObject* self, PyObject* args, PyObject* kwds) 
{
	return PyString_FromString(m_checkpropval);
}

/* 6. setValue */
char SCA_PropertySensor::SetValue_doc[] = 
"setValue(value)\n"
"\t- value: string\n"
"\tSet the value with which the sensor operates. If the value\n"
"\tis not compatible with the type of the property, the subsequent\n"
"\t action is ignored.\n";
PyObject* SCA_PropertySensor::PySetValue(PyObject* self, PyObject* args, PyObject* kwds) 
{
	/* Here, we need to check whether the value is 'valid' for this property.*/
	/* We know that the property exists, or is NULL.                         */
	char *propValArg = NULL;

	if(!PyArg_ParseTuple(args, "s", &propValArg)) {
		return NULL;
	}

	if (validValueForProperty(propValArg, m_checkpropname)) {
		m_checkpropval = propValArg;
	}	

	Py_Return;
}

/* eof */
