/*
 * Expression.h: interface for the CExpression class.
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

/** \file Expression.h
 *  \ingroup expressions
 */

#ifndef __EXPRESSION_H__
#define __EXPRESSION_H__

#include "Value.h"

//extern int gRefCountExpr; // only for debugging purposes (detect mem.leaks)


#define PLUGIN_DECLARE_SERIAL_EXPRESSION(class_name, base_class_name)          \
public:                                                                        \
	virtual base_class_name * Copy() {                                         \
		return new class_name;                                                 \
	}                                                                          \
	virtual bool EdSerialize(CompressorArchive& arch,                          \
	                         class CFactoryManager* facmgr,                    \
	                         bool bIsStoring);                                 \
	virtual bool EdIdSerialize(CompressorArchive& arch,                        \
	                           class CFactoryManager* facmgr,                  \
	                           bool bIsStoring)                                \
	{                                                                          \
		if (bIsStoring)                                                        \
		{                                                                      \
			unsigned char exprID = GetExpressionID();                          \
			arch << exprID;                                                    \
		}                                                                      \
		return true;                                                           \
	}                                                                          \



class CExpression;


// for undo/redo system the deletion in the expressiontree can be restored by replacing broken links 'inplace'
class CBrokenLinkInfo
{
	public:
		CBrokenLinkInfo(CExpression** pmemexpr,CExpression* expr)
			:m_pmemExpr(pmemexpr),
			m_pExpr(expr)
		 { 
			assertd(pmemexpr);
			m_bRestored=false;
		};

		virtual ~CBrokenLinkInfo();
		void RestoreLink();
		void BreakLink();
		
		
	// members vars
	private:
	CExpression** m_pmemExpr;
	CExpression* m_pExpr;
	bool		m_bRestored;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CBrokenLinkInfo")
#endif
};








class CExpression  
{
public:
	enum {
			COPERATOR1EXPRESSIONID = 1,
			COPERATOR2EXPRESSIONID = 2,
			CCONSTEXPRESSIONID = 3,
			CIFEXPRESSIONID = 4,
			COPERATORVAREXPRESSIONID = 5,
			CIDENTIFIEREXPRESSIONID = 6
	};


protected:
	virtual				~CExpression() = 0;			//pure virtual
public:
	virtual bool MergeExpression(CExpression* otherexpr) = 0;
	CExpression();

	
	virtual				CValue* Calculate() = 0;	//pure virtual
	virtual	unsigned char GetExpressionID() = 0;
	//virtual bool		IsInside(float x,float y,float z,bool bBorderInclude=true) = 0;		//pure virtual
	virtual bool		NeedsRecalculated() = 0; // another pure one
	virtual CExpression * CheckLink(std::vector<CBrokenLinkInfo*>& brokenlinks) =0; // another pure one
	virtual void				ClearModified() = 0; // another pure one
	//virtual CExpression * Copy() =0;
	virtual void		BroadcastOperators(VALUE_OPERATOR op) =0;

	virtual CExpression * AddRef() { // please leave multiline, for debugger !!!

#ifdef _DEBUG
		//gRefCountExpr++;
		assertd(m_refcount < 255);
#endif
		m_refcount++; 
		return this;
	};
	virtual CExpression* Release(CExpression* complicatedtrick=NULL) { 
#ifdef _DEBUG
		//gRefCountExpr--;
#endif
		if (--m_refcount < 1) 
		{
			delete this;
		} //else
		//	return this;
		return complicatedtrick;
	};
	

protected:

	int m_refcount;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CExpression")
#endif
};

#endif // !defined __EXPRESSION_H__

