/*
 * Property sensor
 *
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

/** \file gameengine/GameLogic/SCA_PropertySensor.cpp
 *  \ingroup gamelogic
 */


#include <stddef.h>

#include <iostream>
#include "SCA_PropertySensor.h"
#include "Operator2Expr.h"
#include "ConstExpr.h"
#include "InputParser.h"
#include "StringValue.h"
#include "SCA_EventManager.h"
#include "SCA_LogicManager.h"
#include "BoolValue.h"
#include "FloatValue.h"
#include <stdio.h>

SCA_PropertySensor::SCA_PropertySensor(SCA_EventManager* eventmgr,
									 SCA_IObject* gameobj,
									 const STR_String& propname,
									 const STR_String& propval,
									 const STR_String& propmaxval,
									 KX_PROPSENSOR_TYPE checktype)
	: SCA_ISensor(gameobj,eventmgr),
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
	m_reset = true;
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
	replica->ProcessReplica();
	replica->Init();

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



bool SCA_PropertySensor::Evaluate()
{
	bool result = CheckPropertyCondition();
	bool reset = m_reset && m_level;
	
	m_reset = false;
	if (m_lastresult!=result)
	{
		m_lastresult = result;
		return true;
	}
	return (reset) ? true : false;
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
				const STR_String& testprop = orgprop->GetText();
				// Force strings to upper case, to avoid confusion in
				// bool tests. It's stupid the prop's identity is lost
				// on the way here...
				if ((&testprop == &CBoolValue::sTrueString) || (&testprop == &CBoolValue::sFalseString)) {
					m_checkpropval.Upper();
				}
				result = (testprop == m_checkpropval);
				
				/* Patch: floating point values cant use strings usefully since you can have "0.0" == "0.0000"
				 * this could be made into a generic Value class function for comparing values with a string.
				 */
				if (result==false && dynamic_cast<CFloatValue *>(orgprop) != NULL) {
					float f;
					
					if (EOF == sscanf(m_checkpropval.ReadPtr(), "%f", &f))
					{
						//error
					} 
					else {
						result = (f == ((CFloatValue *)orgprop)->GetFloat());
					}
				}
				/* end patch */
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
						const STR_String& errtext = vallie->GetText();
						if (&errtext == &CBoolValue::sTrueString)
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

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

int SCA_PropertySensor::validValueForProperty(void *self, const PyAttributeDef*)
{
	/*  If someone actually do type checking please make sure the 'max' and 'min'
		are checked as well (currently they are calling the PrecalculateRangeExpression
		function directly	*/

	/*  There is no type checking at this moment, unfortunately...           */
	return 0;
}

int SCA_PropertySensor::validValueForIntervalProperty(void *self, const PyAttributeDef*)
{
	SCA_PropertySensor*	sensor = reinterpret_cast<SCA_PropertySensor*>(self);

	if (sensor->m_checktype==KX_PROPSENSOR_INTERVAL)
	{
		sensor->PrecalculateRangeExpression();
	}
	return 0;
}

int SCA_PropertySensor::modeChange(void *self, const PyAttributeDef* attrdef)
{
	SCA_PropertySensor*	sensor = reinterpret_cast<SCA_PropertySensor*>(self);

	if (sensor->m_checktype==KX_PROPSENSOR_INTERVAL)
	{
		sensor->PrecalculateRangeExpression();
	}
	return 0;
}

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_PropertySensor::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_PropertySensor",
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

PyMethodDef SCA_PropertySensor::Methods[] = {
	{NULL,NULL} //Sentinel
};

PyAttributeDef SCA_PropertySensor::Attributes[] = {
	KX_PYATTRIBUTE_INT_RW_CHECK("mode",KX_PROPSENSOR_NODEF,KX_PROPSENSOR_MAX-1,false,SCA_PropertySensor,m_checktype,modeChange),
	KX_PYATTRIBUTE_STRING_RW_CHECK("propName",0,MAX_PROP_NAME,false,SCA_PropertySensor,m_checkpropname,CheckProperty),
	KX_PYATTRIBUTE_STRING_RW_CHECK("value",0,100,false,SCA_PropertySensor,m_checkpropval,validValueForProperty),
	KX_PYATTRIBUTE_STRING_RW_CHECK("min",0,100,false,SCA_PropertySensor,m_checkpropval,validValueForIntervalProperty),
	KX_PYATTRIBUTE_STRING_RW_CHECK("max",0,100,false,SCA_PropertySensor,m_checkpropmaxval,validValueForIntervalProperty),
	{ NULL }	//Sentinel
};

#endif // WITH_PYTHON

/* eof */
