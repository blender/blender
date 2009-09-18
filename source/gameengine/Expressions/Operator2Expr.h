/*
 * Operator2Expr.h: interface for the COperator2Expr class.
 * $Id$
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

#if !defined _OPERATOR2EXPR_H
#define _OPERATOR2EXPR_H


#include "Expression.h"
#include "Value.h"	// Added by ClassView

class COperator2Expr : public CExpression  
{
	//PLUGIN_DECLARE_SERIAL_EXPRESSION (COperator2Expr,CExpression)

public:
	virtual bool MergeExpression(CExpression* otherexpr);
	virtual unsigned char GetExpressionID() { return COPERATOR2EXPRESSIONID;};
	virtual void BroadcastOperators(VALUE_OPERATOR op);
	CExpression* CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks);
	//virtual bool IsInside(float x,float y,float z,bool bBorderInclude=true);
	//virtual bool IsLeftInside(float x,float y,float z,bool bBorderInclude);
	//virtual bool IsRightInside(float x,float y,float z,bool bBorderInclude);
	bool NeedsRecalculated();
	void	ClearModified() { 
		if (m_lhs)
			m_lhs->ClearModified();
		if (m_rhs)
			m_rhs->ClearModified();
	}
	virtual CValue* Calculate();
	COperator2Expr(VALUE_OPERATOR op, CExpression *lhs, CExpression *rhs);
	COperator2Expr();
	virtual ~COperator2Expr();

	
protected:
	CExpression * m_rhs;
	CExpression * m_lhs;
	CValue* m_cached_calculate; // cached result
	
private:
	VALUE_OPERATOR m_op;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:COperator2Expr"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // !defined _OPERATOR2EXPR_H

