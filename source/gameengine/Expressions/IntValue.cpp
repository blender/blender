/** \file gameengine/Expressions/IntValue.cpp
 *  \ingroup expressions
 */
// IntValue.cpp: implementation of the CIntValue class.
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

#include "IntValue.h"
#include "ErrorValue.h"
#include "FloatValue.h"
#include "BoolValue.h"
#include "StringValue.h"
#include "VoidValue.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIntValue::CIntValue()
/*
pre: false
effect: constructs a new CIntValue
*/
{
	
#ifdef _DEBUG_
	m_textval = "Int illegal constructor";
#endif
	m_pstrRep=NULL;
}



CIntValue::CIntValue(cInt innie)
/*
pre:
effect: constructs a new CIntValue containing cInt innie
*/
{
	m_int = innie;
	m_pstrRep=NULL;
}



CIntValue::CIntValue(cInt innie,const char *name,AllocationTYPE alloctype)
{
	m_int = innie;
	SetName(name);
	
	if (alloctype==CValue::STACKVALUE)
	{
		CValue::DisableRefCount();
	}
	m_pstrRep=NULL;
	
}



CIntValue::~CIntValue()
/*
pre:
effect: deletes the object
*/
{
	if (m_pstrRep)
		delete m_pstrRep;
}



CValue* CIntValue::Calc(VALUE_OPERATOR op, CValue *val)
/*
pre:
ret: a new object containing the result of applying operator op to this
object and val
*/
{
	//return val->CalcInt(op, this);
	switch (op) {
	case VALUE_POS_OPERATOR:
		return new CIntValue (m_int);
		break;
	case VALUE_NEG_OPERATOR:
		return new CIntValue (-m_int);
		break;
	case VALUE_NOT_OPERATOR:
		return new CErrorValue (op2str(op) + "only allowed on booleans");
		break;
	case VALUE_AND_OPERATOR:
	case VALUE_OR_OPERATOR:
		return new CErrorValue(val->GetText() + op2str(op) + "only allowed on booleans");
		break;
	default:
		return val->CalcFinal(VALUE_INT_TYPE, op, this);
		break;
	}
}



CValue* CIntValue::CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val)
/*
pre: the type of val is dtype
ret: a new object containing the result of applying operator op to val and
this object
*/
{
	CValue *ret;
	
	switch(dtype) {
	case VALUE_EMPTY_TYPE:
	case VALUE_INT_TYPE:
		{
			switch (op) {
			case VALUE_MOD_OPERATOR:
				ret = new CIntValue (((CIntValue *) val)->GetInt() % m_int);
				break;
			case VALUE_ADD_OPERATOR:
				ret = new CIntValue (((CIntValue *) val)->GetInt() + m_int);
				break;
			case VALUE_SUB_OPERATOR:
				ret = new CIntValue (((CIntValue *) val)->GetInt() - m_int);
				break;
			case VALUE_MUL_OPERATOR:
				ret = new CIntValue (((CIntValue *) val)->GetInt() * m_int);
				break;
			case VALUE_DIV_OPERATOR:
				if (m_int == 0)
				{
					if (val->GetNumber() == 0)
					{
						ret = new CErrorValue("Not a Number");
					} else
					{
						ret = new CErrorValue("Division by zero");
					}
				}
				else
					ret = new CIntValue (((CIntValue *) val)->GetInt() / m_int);
				break;
			case VALUE_EQL_OPERATOR:
				ret = new CBoolValue(((CIntValue *) val)->GetInt() == m_int);
				break;
			case VALUE_NEQ_OPERATOR:
				ret = new CBoolValue(((CIntValue *) val)->GetInt() != m_int);
				break;
			case VALUE_GRE_OPERATOR:
				ret = new CBoolValue(((CIntValue *) val)->GetInt() > m_int);
				break;
			case VALUE_LES_OPERATOR:
				ret = new CBoolValue(((CIntValue *) val)->GetInt() < m_int);
				break;
			case VALUE_GEQ_OPERATOR:
				ret = new CBoolValue(((CIntValue *) val)->GetInt() >= m_int);
				break;
			case VALUE_LEQ_OPERATOR:
				ret = new CBoolValue(((CIntValue *) val)->GetInt() <= m_int);
				break;
			case VALUE_NEG_OPERATOR:
				ret = new CIntValue (-m_int);
				break;
			case VALUE_POS_OPERATOR:
				ret = new CIntValue (m_int);
				break;
			default:
				ret = new CErrorValue("illegal operator. please send a bug report.");
				break;
			}
			break;
		}
	case VALUE_FLOAT_TYPE:
		{
			switch (op) {
			case VALUE_MOD_OPERATOR:
				ret = new CFloatValue(fmod(((CFloatValue *) val)->GetFloat(), m_int));
				break;
			case VALUE_ADD_OPERATOR:
				ret = new CFloatValue (((CFloatValue *) val)->GetFloat() + m_int);
				break;
			case VALUE_SUB_OPERATOR:
				ret = new CFloatValue (((CFloatValue *) val)->GetFloat() - m_int);
				break;
			case VALUE_MUL_OPERATOR:
				ret = new CFloatValue (((CFloatValue *) val)->GetFloat() * m_int);
				break;
			case VALUE_DIV_OPERATOR:
				if (m_int == 0)
					ret = new CErrorValue("Division by zero");
				else
					ret = new CFloatValue (((CFloatValue *) val)->GetFloat() / m_int);
				break;
			case VALUE_EQL_OPERATOR:
				ret = new CBoolValue(((CFloatValue *) val)->GetFloat() == m_int);
				break;
			case VALUE_NEQ_OPERATOR:
				ret = new CBoolValue(((CFloatValue *) val)->GetFloat() != m_int);
				break;
			case VALUE_GRE_OPERATOR:
				ret = new CBoolValue(((CFloatValue *) val)->GetFloat() > m_int);
				break;
			case VALUE_LES_OPERATOR:
				ret = new CBoolValue(((CFloatValue *) val)->GetFloat() < m_int);
				break;
			case VALUE_GEQ_OPERATOR:
				ret = new CBoolValue(((CFloatValue *) val)->GetFloat() >= m_int);
				break;
			case VALUE_LEQ_OPERATOR:
				ret = new CBoolValue(((CFloatValue *) val)->GetFloat() <= m_int);
				break;
			default:
				ret = new CErrorValue("illegal operator. please send a bug report.");
				break;
			}
			break;
		}
	case VALUE_STRING_TYPE:
		{
			switch(op) {
			case VALUE_ADD_OPERATOR:
				ret = new CStringValue(val->GetText() + GetText(),"");
				break;
			case VALUE_EQL_OPERATOR:
			case VALUE_NEQ_OPERATOR:
			case VALUE_GRE_OPERATOR:
			case VALUE_LES_OPERATOR:
			case VALUE_GEQ_OPERATOR:
			case VALUE_LEQ_OPERATOR:
				ret = new CErrorValue("[Cannot compare string with integer]" + op2str(op) + GetText());
				break;
			default:
				ret =  new CErrorValue("[operator not allowed on strings]" + op2str(op) + GetText());
				break;
			}
			break;
		}
	case VALUE_BOOL_TYPE:
		ret =  new CErrorValue("[operator not valid on boolean and integer]" + op2str(op) + GetText());
		break;
		/*
		case VALUE_EMPTY_TYPE:
		{
		switch(op) {
		
		  case VALUE_ADD_OPERATOR:
		  ret = new CIntValue (m_int);
		  break;
		  case VALUE_SUB_OPERATOR:
		  ret = new CIntValue (-m_int);
		  break;
		  default:
		  {
		  ret = new CErrorValue(op2str(op) +	GetText());
		  }
		  }
		  break;
		  }
		*/
	case VALUE_ERROR_TYPE:
		ret = new CErrorValue(val->GetText() + op2str(op) +	GetText());
		break;
	default:
		ret = new CErrorValue("illegal type. contact your dealer (if any)");
		break;
	}
	return ret;
}



cInt CIntValue::GetInt()
/*
pre:
ret: the cInt stored in the object
*/
{
	return m_int;
}



double CIntValue::GetNumber()
{
	return (float) m_int;
}



const STR_String & CIntValue::GetText()
{
	if (!m_pstrRep)
		m_pstrRep=new STR_String();
	m_pstrRep->Format("%lld",m_int);
	
	return *m_pstrRep;
}



CValue* CIntValue::GetReplica()
{
	CIntValue* replica = new CIntValue(*this);
	replica->ProcessReplica();
	replica->m_pstrRep = NULL;
	
	return replica;
}



void CIntValue::SetValue(CValue* newval)
{ 	
	m_int = (cInt)newval->GetNumber(); 
	SetModified(true);
}


#ifdef WITH_PYTHON
PyObject* CIntValue::ConvertValueToPython()
{
	if((m_int > INT_MIN) && (m_int < INT_MAX))
		return PyLong_FromSsize_t(m_int);
	else
		return PyLong_FromLongLong(m_int);
}
#endif // WITH_PYTHON
