/** \file gameengine/Expressions/EmptyValue.cpp
 *  \ingroup expressions
 */

// EmptyValue.cpp: implementation of the CEmptyValue class.
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

#include "EmptyValue.h"
#include "IntValue.h"
#include "FloatValue.h"
#include "StringValue.h"
#include "ErrorValue.h"
#include "ListValue.h"
#include "VoidValue.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEmptyValue::CEmptyValue()
/*
 * pre:
 * effect: constructs a new CEmptyValue
 */
{
	SetModified(false);
}



CEmptyValue::~CEmptyValue()
/*
 * pre:
 * effect: deletes the object
 */
{

}



CValue *CEmptyValue::Calc(VALUE_OPERATOR op, CValue *val)
/*
 * pre:
 * ret: a new object containing the result of applying operator op to this
 * object and val
 */
{
	return val->CalcFinal(VALUE_EMPTY_TYPE, op, this);
	
}



CValue * CEmptyValue::CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val)
/*
 * pre: the type of val is dtype
 * ret: a new object containing the result of applying operator op to val and
 * this object
 */
{
	return val->AddRef();
}



double CEmptyValue::GetNumber()
{
	return 0;
}



CListValue* CEmptyValue::GetPolySoup()
{
	CListValue* soup = new CListValue();
	//don't add any poly, while it's an empty value
	return soup;
}



bool CEmptyValue::IsInside(CValue* testpoint,bool bBorderInclude)
{
	// empty space is solid, so always inside
	return true;
}



double*	CEmptyValue::GetVector3(bool bGetTransformedVec)
{ 
	assertd(false); // don't get vector from me
	return ZeroVector();
}



static STR_String emptyString = STR_String("");


const STR_String & CEmptyValue::GetText()
{
	return emptyString;
}



CValue* CEmptyValue::GetReplica()
{ 
	CEmptyValue* replica = new CEmptyValue(*this);
	replica->ProcessReplica();
	return replica;
}

