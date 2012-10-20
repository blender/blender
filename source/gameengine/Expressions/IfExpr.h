/*
 * IfExpr.h: interface for the CIfExpr class.
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

/** \file IfExpr.h
 *  \ingroup expressions
 */

#ifndef __IFEXPR_H__
#define __IFEXPR_H__

#include "Expression.h"

class CIfExpr : public CExpression  
{
	//PLUGIN_DECLARE_SERIAL_EXPRESSION (CIfExpr,CExpression)

private:
	CExpression *m_guard, *m_e1, *m_e2;

public:
	virtual bool MergeExpression(CExpression* otherexpr);
	CIfExpr(CExpression *guard, CExpression *e1, CExpression *e2);
	CIfExpr();
	
	virtual unsigned char GetExpressionID();
	virtual ~CIfExpr();
	virtual CValue* Calculate();
	
	virtual bool		IsInside(float x,float y,float z,bool bBorderInclude=true);
	virtual bool		NeedsRecalculated();


	virtual CExpression*	CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks);
	virtual void			ClearModified();
	virtual void			BroadcastOperators(VALUE_OPERATOR op);


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CIfExpr")
#endif
};

#endif  /* __IFEXPR_H__ */
