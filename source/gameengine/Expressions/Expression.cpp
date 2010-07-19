// Expression.cpp: implementation of the CExpression class.
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

#include "Expression.h"
#include "ErrorValue.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
#ifdef _DEBUG
//int gRefCountExpr;
#endif
CExpression::CExpression()// : m_cached_calculate(NULL)
{
	m_refcount = 1;
#ifdef _DEBUG
	//gRefCountExpr++;
#endif
}

CExpression::~CExpression()
{
	assert (m_refcount == 0);
}



// destuctor for CBrokenLinkInfo
CBrokenLinkInfo::~CBrokenLinkInfo()
{
	if (m_pExpr && !m_bRestored)
		m_pExpr->Release();
}


void CBrokenLinkInfo::RestoreLink()
{

	
	assertd(m_pExpr);

	if (m_pExpr)
	{
		if (!m_bRestored){
			m_bRestored=true;
			
		}
		if (*m_pmemExpr)
		{
			(*m_pmemExpr)->Release();
		}
		*m_pmemExpr = m_pExpr;
		
//		m_pExpr=NULL;
	}
}

void CBrokenLinkInfo::BreakLink()
{
	m_bRestored=false;
	m_pExpr->AddRef();
}

