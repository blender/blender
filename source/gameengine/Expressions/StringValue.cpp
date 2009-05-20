// StringValue.cpp: implementation of the CStringValue class.
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

#include "StringValue.h"
#include "BoolValue.h"
#include "ErrorValue.h"
#include "VoidValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CStringValue::CStringValue()
/*
pre: false
effect: constructs a new CStringValue
*/
{
	m_strString = "[Illegal String constructor call]";
}

CStringValue::CStringValue(const char *txt,const char *name,AllocationTYPE alloctype)
/*
pre:
effect: constructs a new CStringValue containing text txt
*/
{
	m_strString = txt;
	SetName(name);
	
	if (alloctype==CValue::STACKVALUE)
	{
		CValue::DisableRefCount();
		
	}
	
	
}


CValue* CStringValue::Calc(VALUE_OPERATOR op, CValue *val)
/*
pre:
ret: a new object containing the result of applying operator op to this
object and val
*/
{
	//return val->CalrcString(op, this);
	return val->CalcFinal(VALUE_STRING_TYPE, op, this);
}

CValue* CStringValue::CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val)
/*
pre: the type of val is dtype
ret: a new object containing the result of applying operator op to val and
this object
*/
{
	CValue *ret;
	
	if (op == VALUE_ADD_OPERATOR) {
		if (dtype == VALUE_ERROR_TYPE)
			ret = new CErrorValue(val->GetText() + op2str(op) +	GetText());
		else
			ret = new CStringValue(val->GetText() + GetText(),"");
	} else {
		
		if (dtype == VALUE_STRING_TYPE || dtype == VALUE_EMPTY_TYPE) {
			switch(op) {
			case VALUE_EQL_OPERATOR:
				ret = new CBoolValue(val->GetText() == GetText());
				break;
			case VALUE_NEQ_OPERATOR:
				ret = new CBoolValue(val->GetText() != GetText());
				break;
			case VALUE_GRE_OPERATOR:
				ret = new CBoolValue(val->GetText() > GetText());
				break;
			case VALUE_LES_OPERATOR:
				ret = new CBoolValue(val->GetText() < GetText());
				break;
			case VALUE_GEQ_OPERATOR:
				ret = new CBoolValue(val->GetText() >= GetText());
				break;
			case VALUE_LEQ_OPERATOR:
				ret = new CBoolValue(val->GetText() <= GetText());
				break;
			default:
				ret =  new CErrorValue(val->GetText() + op2str(op) + "[operator not allowed on strings]");
				break;
			}
		} else {
			ret =  new CErrorValue(val->GetText() + op2str(op) + "[operator not allowed on strings]");
		}
	}
	return ret;
}



double CStringValue::GetNumber()
{
	return -1;
}



const STR_String & CStringValue::GetText()
{
	return m_strString;
}

bool CStringValue::IsEqual(const STR_String & other)
{
	return (m_strString == other);
}

CValue* CStringValue::GetReplica()
{ 
	CStringValue* replica = new CStringValue(*this);
	replica->ProcessReplica();
	return replica;
};


