// Operator2Expr.cpp: implementation of the COperator2Expr class.
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
// 31 dec 1998 - big update: try to use the cached data for updating, instead of
// rebuilding completely it from left and right node. Modified flags and bounding boxes
// have to do the trick
// when expression is cached, there will be a call to UpdateCalc() instead of Calc()

#include "Operator2Expr.h"
#include "StringValue.h"
#include "VoidValue.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

COperator2Expr::COperator2Expr(VALUE_OPERATOR op, CExpression *lhs, CExpression *rhs)
: 	
m_rhs(rhs),
m_lhs(lhs),
m_cached_calculate(NULL),
m_op(op)
/*
pre:
effect: constucts a COperator2Expr with op, lhs and rhs in it
*/
{

}

COperator2Expr::COperator2Expr():
m_rhs(NULL),
m_lhs(NULL),
m_cached_calculate(NULL)

/*
pre:
effect: constucts an empty COperator2Expr
*/
{
	
}

COperator2Expr::~COperator2Expr()
/*
pre:
effect: deletes the object
*/
{
	if (m_lhs)
		m_lhs->Release();
	if (m_rhs)
		m_rhs->Release();
	if (m_cached_calculate)
		m_cached_calculate->Release();
	
}
CValue* COperator2Expr::Calculate()
/*
pre:
ret: a new object containing the result of applying operator m_op to m_lhs
and m_rhs
*/
{
	
	bool leftmodified,rightmodified;
	leftmodified = m_lhs->NeedsRecalculated();
	rightmodified = m_rhs->NeedsRecalculated();
	
	// if no modifications on both left and right subtree, and result is already calculated
	// then just return cached result...
	if (!leftmodified && !rightmodified && (m_cached_calculate))
	{
		// not modified, just return m_cached_calculate
	} else {
		// if not yet calculated, or modified...
		
		
		if (m_cached_calculate) {
			m_cached_calculate->Release();
			m_cached_calculate=NULL;
		}
		
		CValue* ffleft = m_lhs->Calculate();
		CValue* ffright = m_rhs->Calculate();
		
		ffleft->SetOwnerExpression(this);//->m_pOwnerExpression=this;
		ffright->SetOwnerExpression(this);//->m_pOwnerExpression=this;
		
		m_cached_calculate = ffleft->Calc(m_op,ffright);
		
		//if (m_cached_calculate)				
		//	m_cached_calculate->Action(CValue::SETOWNEREXPR,&CVoidValue(this,false,CValue::STACKVALUE));

		ffleft->Release();
		ffright->Release();
	}
	
	return m_cached_calculate->AddRef();
	
}

/*
bool COperator2Expr::IsInside(float x, float y, float z,bool bBorderInclude)
{
	bool inside;
	inside = false;
	
	switch (m_op) 
	{
	case VALUE_ADD_OPERATOR: {
	//	inside = first || second; // optimized with early out if first is inside
		// todo: calculate smallest leaf first ! is much faster...
			
		bool second;//first ;//,second;
		
		//first = m_lhs->IsInside(x,y,z) ;
		second = m_rhs->IsInside(x,y,z,bBorderInclude) ;
		if (second)
			return true; //early out
	
	//	second = m_rhs->IsInside(x,y,z) ;

		return m_lhs->IsInside(x,y,z,bBorderInclude) ;
			
		break;
							 }
		
	case VALUE_SUB_OPERATOR: {
		//inside = first && !second; // optimized with early out
		// todo: same as with add_operator: calc smallest leaf first

		bool second;//first ;//,second;
		//first = m_lhs->IsInside(x,y,z) ;
		second = m_rhs->IsInside(x,y,z,bBorderInclude);
		if (second)
			return false;

		// second space get subtracted -> negate!
		//second = m_rhs->IsInside(x,y,z);

		return (m_lhs->IsInside(x,y,z,bBorderInclude));

		
		break;
							 }
	default:{
		assert(false);
		// not yet implemented, only add or sub csg operations
			}
	}
	
	return inside;	
}



bool COperator2Expr::IsRightInside(float x, float y, float z,bool bBorderInclude) {
	
	return m_rhs->IsInside(x,y,z,bBorderInclude) ;
	
}

bool COperator2Expr::IsLeftInside(float x, float y, float z,bool bBorderInclude) {
	return m_lhs->IsInside(x,y,z,bBorderInclude);
}
*/
bool COperator2Expr::NeedsRecalculated() {
	// added some lines, just for debugging purposes, it could be a one-liner :)
	//bool modleft
	//bool modright;
	assertd(m_lhs);
	assertd(m_rhs);

	//modright = m_rhs->NeedsRecalculated();
	if (m_rhs->NeedsRecalculated()) // early out
		return true;
	return m_lhs->NeedsRecalculated();
	//modleft = m_lhs->NeedsRecalculated();
	//return (modleft || modright);
	
}



CExpression* COperator2Expr::CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks) {
// if both children are 'dead', return NULL
// if only one child is alive, return that child
// if both childresn are alive, return this


//  	bool leftalive = true,rightalive=true;
	/* Does this mean the function will always bomb? */
	assertd(false);
	assert(m_lhs);
	assert(m_rhs);
/*
	if (m_cached_calculate)
		m_cached_calculate->Action(CValue::REFRESH_CACHE);
	
	CExpression* newlhs = m_lhs->CheckLink(brokenlinks);
	CExpression* newrhs = m_rhs->CheckLink(brokenlinks);

	if (m_lhs != newlhs)
	{
		brokenlinks.push_back(new CBrokenLinkInfo(&m_lhs,m_lhs));
	}

	if (m_rhs != newrhs)
	{
		brokenlinks.push_back(new CBrokenLinkInfo(&m_rhs,m_rhs));
	}



	m_lhs = newlhs;
	m_rhs = newrhs;

	if (m_lhs && m_rhs) {
		return this;
	}
	
	AddRef();
	if (m_lhs) 
		return Release(m_lhs->AddRef());
	
	if (m_rhs)
		return Release(m_rhs->AddRef());
/		

  */
  return Release();

  
	
}


bool COperator2Expr::MergeExpression(CExpression *otherexpr)
{
	if (m_lhs)
	{
		if (m_lhs->GetExpressionID() == CExpression::CCONSTEXPRESSIONID)
		{
			// cross fingers ;) replace constexpr by new tree...
			m_lhs->Release();
			m_lhs = otherexpr->AddRef();
			return true;
		}
	}

	assertd(false);
	return false;
}


void COperator2Expr::BroadcastOperators(VALUE_OPERATOR op)
{
	if (m_lhs)
		m_lhs->BroadcastOperators(m_op);
	if (m_rhs)
		m_rhs->BroadcastOperators(m_op);
}
