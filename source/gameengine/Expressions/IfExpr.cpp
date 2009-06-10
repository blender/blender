// IfExpr.cpp: implementation of the CIfExpr class.
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

#include "IfExpr.h"
#include "EmptyValue.h"
#include "ErrorValue.h"
#include "BoolValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


CIfExpr::CIfExpr()
{
}



CIfExpr::CIfExpr(CExpression *guard, CExpression *e1, CExpression *e2)
/*
pre:
effect: constructs an CifExpr-object corresponding to IF(guard, e1, e2)
*/
{
	m_guard = guard;
	m_e1 = e1;
	m_e2 = e2;
}



CIfExpr::~CIfExpr()
/*
pre:
effect: dereferences the object
*/
{
	if (m_guard)
		m_guard->Release();

	if (m_e1)
		m_e1->Release();

	if (m_e2)
		m_e2->Release();
}



CValue* CIfExpr::Calculate()
/*
pre:
ret: a new object containing the value of m_e1 if m_guard is a boolean TRUE
	 a new object containing the value of m_e2 if m_guard is a boolean FALSE
	 an new errorvalue if m_guard is not a boolean
*/
{
	CValue *guardval;
	guardval = m_guard->Calculate();
	const STR_String& text = guardval->GetText();
	guardval->Release();

	if (&text == &CBoolValue::sTrueString)
	{
		return m_e1->Calculate();
	}
	else if (&text == &CBoolValue::sFalseString)
	{
		return m_e2->Calculate();
	}
	else
	{
		return new CErrorValue("Guard should be of boolean type");
	}
}



bool CIfExpr::MergeExpression(CExpression *otherexpr)
{
	assertd(false);
	return false;
}



bool CIfExpr::IsInside(float x,float y,float z,bool bBorderInclude)
{
	assertd(false);
	return false;
}
	


bool CIfExpr::NeedsRecalculated() 
{
	return  (m_guard->NeedsRecalculated() ||
		m_e1->NeedsRecalculated() ||
		m_e2->NeedsRecalculated());
}



CExpression* CIfExpr::CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks)
{
	assertd(false);
	return NULL;
}



void CIfExpr::ClearModified()
{
	assertd(false);
}



void CIfExpr::BroadcastOperators(VALUE_OPERATOR op)
{
	assertd(false);
}



unsigned char CIfExpr::GetExpressionID()
{
	return CIFEXPRESSIONID;
}
