/*
 * ConstExpr.h: interface for the CConstExpr class.
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

#ifndef __CONSTEXPR_H__
#define __CONSTEXPR_H__

#include "Expression.h"
#include "Value.h"	// Added by ClassView

class CConstExpr : public CExpression  
{
	//PLUGIN_DECLARE_SERIAL_EXPRESSION (CConstExpr,CExpression)
public:
	virtual bool MergeExpression(CExpression* otherexpr);
	
	void BroadcastOperators(VALUE_OPERATOR op);

	virtual unsigned char GetExpressionID();
	CExpression*	CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks);
	//bool IsInside(float x,float y,float z,bool bBorderInclude=true);
	bool NeedsRecalculated();
	void ClearModified();
	virtual double GetNumber();
	virtual CValue* Calculate();
	CConstExpr(CValue* constval);
	CConstExpr();
	virtual ~CConstExpr();
			

private:
	CValue* m_value;


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:CConstExpr"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // !defined(AFX_CONSTEXPR_H__061ECFC3_BE87_11D1_A51C_00A02472FC58__INCLUDED_)

