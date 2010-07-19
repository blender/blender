// Operator1Expr.cpp: implementation of the COperator1Expr class.
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

#include "Operator1Expr.h"
#include "EmptyValue.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

COperator1Expr::COperator1Expr()
/*
pre:
effect: constucts an empty COperator1Expr
*/
{
	m_lhs = NULL;
}

COperator1Expr::COperator1Expr(VALUE_OPERATOR op, CExpression * lhs)
/*
pre:
effect: constucts a COperator1Expr with op and lhs in it
*/
{
	m_lhs = lhs;
	m_op = op;
}

COperator1Expr::~COperator1Expr()
/*
pre:
effect: deletes the object
*/
{
	if (m_lhs) m_lhs->Release();
}

CValue * COperator1Expr::Calculate()
/*
pre:
ret: a new object containing the result of applying the operator m_op to the
	 value of m_lhs
*/
{
	CValue *ret;
	CValue *temp = m_lhs->Calculate();
	CValue* empty = new CEmptyValue();
	ret = empty->Calc(m_op, temp);
	empty->Release();
	temp->Release();
	
	return ret;
}

/*
bool COperator1Expr::IsInside(float x, float y, float z,bool bBorderInclude)
{

	bool result = true;
	switch (m_op)
	{
		
	case VALUE_ADD_OPERATOR:
		{
			
			if (m_lhs)
			{
				result = result || m_lhs->IsInside(x,y,z,bBorderInclude);
			}
			break;
		}
	case VALUE_SUB_OPERATOR:
		{
			result = true;
			if (m_lhs)
			{
				result = result && (!m_lhs->IsInside(x,y,z,bBorderInclude));
			}
			break;
		}
	}
	return result;
}

*/
bool COperator1Expr::NeedsRecalculated() {
	
	return m_lhs->NeedsRecalculated();

}

CExpression* COperator1Expr::CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks) {

	CExpression* newlhs = m_lhs->CheckLink(brokenlinks);

	if (newlhs)
	{
		if (newlhs==m_lhs) {
			// not changed
		} else {
			// changed
			//numchanges++;
			newlhs->AddRef();
			
			//m_lhs->Release();
			brokenlinks.push_back(new CBrokenLinkInfo(&m_lhs,m_lhs));

			m_lhs = newlhs;
		}
		return this;
	} else {
		//numchanges++;
		AddRef();

		return Release();
	}
	
}

void COperator1Expr::BroadcastOperators(VALUE_OPERATOR op)
{
	if (m_lhs)
		m_lhs->BroadcastOperators(m_op);
}




bool COperator1Expr::MergeExpression(CExpression *otherexpr)
{
	if (m_lhs)
		return m_lhs->MergeExpression(otherexpr);
	
	assertd(false); // should not get here, expression is not compatible for merge
	return false;
}
