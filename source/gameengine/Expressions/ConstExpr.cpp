/** \file gameengine/Expressions/ConstExpr.cpp
 *  \ingroup expressions
 */
// ConstExpr.cpp: implementation of the CConstExpr class.

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

#include "Value.h" // for precompiled header
#include "ConstExpr.h"
#include "VectorValue.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CConstExpr::CConstExpr()
{
}



CConstExpr::CConstExpr(CValue* constval) 
/*
pre:
effect: constructs a CConstExpr cointing the value constval
*/
{
	m_value = constval;
//	m_bModified=true;
}



CConstExpr::~CConstExpr()
/*
pre:
effect: deletes the object
*/
{
	if (m_value)
		m_value->Release();
}



unsigned char CConstExpr::GetExpressionID()
{
	return CCONSTEXPRESSIONID;
}



CValue* CConstExpr::Calculate()
/*
pre:
ret: a new object containing the value of the stored CValue
*/
{
	return m_value->AddRef();
}



void CConstExpr::ClearModified()
{ 
	if (m_value)
	{
		m_value->SetModified(false);
		m_value->SetAffected(false);
	}
}



double CConstExpr::GetNumber()
{
	return -1;
}



bool CConstExpr::NeedsRecalculated()
{
	return m_value->IsAffected(); // IsAffected is m_bModified OR m_bAffected !!!
}



CExpression* CConstExpr::CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks)
{
// parent checks if child is still useful.
// When for example it's value it's deleted flag set
// then release Value, and return NULL in case of constexpression
// else return this...

	assertd(m_value);
	if (m_value->IsReleaseRequested())
	{
		AddRef(); //numchanges++;
		return Release();
	}
	else
		return this;
}



void CConstExpr::BroadcastOperators(VALUE_OPERATOR op)
{
	assertd(m_value);
	m_value->SetColorOperator(op);
}



bool CConstExpr::MergeExpression(CExpression *otherexpr)
{
	assertd(false);
	return false;
}
