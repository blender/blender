/** \file gameengine/Expressions/BoolValue.cpp
 *  \ingroup expressions
 */

// BoolValue.cpp: implementation of the CBoolValue class.
/*
 * Copyright (c) 1996-2000 Erwin Coumans <coockie@acm.org>
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Erwin Coumans makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 */

#include "BoolValue.h"
#include "StringValue.h"
#include "ErrorValue.h"
#include "VoidValue.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

const STR_String CBoolValue::sTrueString  = "TRUE";
const STR_String CBoolValue::sFalseString = "FALSE";

CBoolValue::CBoolValue()
/*
 * pre: false
 * effect: constructs a new CBoolValue
 */
{
	trace("Bool constructor error");
}



CBoolValue::CBoolValue(bool inBool)
: m_bool(inBool)
{
} // Constructs a new CBoolValue containing <inBool>



CBoolValue::CBoolValue(bool innie,const char *name,AllocationTYPE alloctype)
{
	m_bool = innie;
	SetName(name);

	if (alloctype == CValue::STACKVALUE)
		CValue::DisableRefCount();
}



void CBoolValue::SetValue(CValue* newval)
{
	m_bool = (newval->GetNumber() != 0);
	SetModified(true);
}



CValue* CBoolValue::Calc(VALUE_OPERATOR op, CValue *val)
/*
pre:
ret: a new object containing the result of applying operator op to this
object and val
*/
{
	switch (op)
	{
	case VALUE_POS_OPERATOR:
	case VALUE_NEG_OPERATOR:
		{
			return new CErrorValue (op2str(op) + GetText());
			break;
		}
	case VALUE_NOT_OPERATOR:
		{
			return new CBoolValue (!m_bool);
			break;
		}
	default:
		{
			return val->CalcFinal(VALUE_BOOL_TYPE, op, this);
			break;
		}
	}
}



CValue* CBoolValue::CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val)
/*
pre: the type of val is dtype
ret: a new object containing the result of applying operator op to val and
this object
*/
{
	CValue *ret;
	
	switch (dtype) {
		case VALUE_EMPTY_TYPE:
		case VALUE_BOOL_TYPE:
		{
			switch (op) {
				case VALUE_AND_OPERATOR:
				{
					ret = new CBoolValue (((CBoolValue *) val)->GetBool() && m_bool);
					break;
				}
				case VALUE_OR_OPERATOR:
				{
					ret = new CBoolValue (((CBoolValue *) val)->GetBool() || m_bool);
					break;
				}
				case VALUE_EQL_OPERATOR:
				{
					ret = new CBoolValue (((CBoolValue *) val)->GetBool() == m_bool);
					break;
				}
				case VALUE_NEQ_OPERATOR:
				{
					ret = new CBoolValue (((CBoolValue *) val)->GetBool() != m_bool);
					break;
				}
				case VALUE_NOT_OPERATOR:
				{
					return new CBoolValue (!m_bool);
					break;
				}
				default:
				{
					ret =  new CErrorValue(val->GetText() + op2str(op) +
					                       "[operator not allowed on booleans]");
					break;
				}
			}
			break;
		}
		case VALUE_STRING_TYPE:
		{
			switch (op) {
				case VALUE_ADD_OPERATOR:
				{
					ret = new CStringValue(val->GetText() + GetText(),"");
					break;
				}
				default:
				{
					ret =  new CErrorValue(val->GetText() + op2str(op) + "[Only + allowed on boolean and string]");
					break;
				}
			}
			break;
		}
		default:
			ret =  new CErrorValue("[type mismatch]" + op2str(op) + GetText());
	}

	return ret;
}



bool CBoolValue::GetBool()
/*
pre:
ret: the bool stored in the object
*/
{
	return m_bool;
}



double CBoolValue::GetNumber()
{
	return (double)m_bool;
}



int CBoolValue::GetValueType()
{
	return VALUE_BOOL_TYPE;
}



const STR_String& CBoolValue::GetText()
{
	return m_bool ? sTrueString : sFalseString;
}



CValue* CBoolValue::GetReplica()
{
	CBoolValue* replica = new CBoolValue(*this);
	replica->ProcessReplica();
	
	return replica;
}

#ifdef WITH_PYTHON
PyObject *CBoolValue::ConvertValueToPython()
{
	return PyBool_FromLong(m_bool != 0);
}
#endif // WITH_PYTHON
