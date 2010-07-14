/*
 * Operator1Expr.h: interface for the COperator1Expr class.
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

#if !defined(AFX_OPERATOR1EXPR_H__A1653901_BF41_11D1_A51C_00A02472FC58__INCLUDED_)
#define AFX_OPERATOR1EXPR_H__A1653901_BF41_11D1_A51C_00A02472FC58__INCLUDED_

#include "Expression.h"

class COperator1Expr : public CExpression  
{
	//PLUGIN_DECLARE_SERIAL_EXPRESSION (COperator1Expr,CExpression)



public:
	virtual bool MergeExpression(CExpression* otherexpr);
	virtual void BroadcastOperators(VALUE_OPERATOR op);

	virtual unsigned char GetExpressionID() { return COPERATOR1EXPRESSIONID;};
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
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:COperator1Expr"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // !defined(AFX_OPERATOR1EXPR_H__A1653901_BF41_11D1_A51C_00A02472FC58__INCLUDED_)

