/*
 * IfExpr.h: interface for the CIfExpr class.
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
#if !defined(AFX_IFEXPR_H__1F691841_C5C7_11D1_A863_0000B4542BD8__INCLUDED_)
#define AFX_IFEXPR_H__1F691841_C5C7_11D1_A863_0000B4542BD8__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

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
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:CIfExpr"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // !defined(AFX_IFEXPR_H__1F691841_C5C7_11D1_A863_0000B4542BD8__INCLUDED_)

