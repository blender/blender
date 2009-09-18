// ErrorValue.cpp: implementation of the CErrorValue class.
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

#include "ErrorValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CErrorValue::CErrorValue()
/*
pre:
effect: constructs a new CErrorValue containing errormessage "Error"
*/
{
	m_strErrorText = "Error";
	SetError(true);
}



CErrorValue::CErrorValue(const char *errmsg)
/*
pre:
effect: constructs a new CErrorValue containing errormessage errmsg
*/
{
  m_strErrorText = "[";
  m_strErrorText += errmsg;
  m_strErrorText += "]";
  SetError(true);
}



CErrorValue::~CErrorValue()
/*
pre:
effect: deletes the object
*/
{

}



CValue* CErrorValue::Calc(VALUE_OPERATOR op, CValue *val)
/*
pre:
ret: a new object containing the result of applying operator op to this
	 object and val
*/
{
	CValue* errorval;

	switch (op)
	{
	case VALUE_POS_OPERATOR:
	case VALUE_NEG_OPERATOR:
	case VALUE_NOT_OPERATOR:
		{
			errorval = new CErrorValue (op2str(op) + GetText());
			break;
		}
	default:
		{
			errorval = val->CalcFinal(VALUE_ERROR_TYPE, op, this);
			break;
		}
	}
	
	return errorval;
}



CValue* CErrorValue::CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val)
/*
pre: the type of val is dtype
ret: a new object containing the result of applying operator op to val and
	 this object
*/
{
	return new CErrorValue (val->GetText() + op2str(op) + GetText());
}



double CErrorValue::GetNumber()
{
	return -1;
}



const STR_String & CErrorValue::GetText()
{
	return m_strErrorText;
}



CValue* CErrorValue::GetReplica()
{ 
	// who would want a copy of an error ?
	trace ("Error: ErrorValue::GetReplica() not implemented yet");
	assertd(false);

	return NULL;
}
