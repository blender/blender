/*
 * Operator1Expr.h: interface for the COperator1Expr class.
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

/** \file Operator1Expr.h
 *  \ingroup expressions
 */

#ifndef __OPERATOR1EXPR_H__
#define __OPERATOR1EXPR_H__

#include "Expression.h"

class COperator1Expr : public CExpression  
{
	//PLUGIN_DECLARE_SERIAL_EXPRESSION (COperator1Expr,CExpression)



public:
	virtual bool MergeExpression(CExpression* otherexpr);
	virtual void BroadcastOperators(VALUE_OPERATOR op);

	virtual unsigned char GetExpressionID() { return COPERATOR1EXPRESSIONID; }
	CExpression* CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks);
	//virtual bool IsInside(float x,float y,float z,bool bBorderInclude = true);
	virtual	bool NeedsRecalculated();
	void ClearModified() {
		if (m_lhs)
			m_lhs->ClearModified();
	}
	virtual CValue* Calculate();
	COperator1Expr(VALUE_OPERATOR op, CExpression *lhs);
	COperator1Expr();
	virtual ~COperator1Expr();
	
	
	
private:
	VALUE_OPERATOR m_op;
	CExpression * m_lhs;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:")
#endif
};

#endif  /* __OPERATOR1EXPR_H__ */
