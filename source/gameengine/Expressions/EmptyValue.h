/*
 * EmptyValue.h: interface for the CEmptyValue class.
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

/** \file EmptyValue.h
 *  \ingroup expressions
 */

#ifndef _EMPTYVALUE_H
#define _EMPTYVALUE_H

#include "Value.h"

class CListValue;

class CEmptyValue : public CPropValue  
{
	//PLUGIN_DECLARE_SERIAL (CEmptyValue,CValue)
public:
	CEmptyValue();
	virtual					~CEmptyValue();

	virtual const STR_String &	GetText();
	virtual double			GetNumber();
	CListValue*				GetPolySoup();
	virtual double*			GetVector3(bool bGetTransformedVec=false);
	bool					IsInside(CValue* testpoint,bool bBorderInclude=true);
	CValue *				Calc(VALUE_OPERATOR op, CValue *val);
	CValue *				CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	virtual CValue*			GetReplica();


#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:CEmptyValue"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // !defined _EMPTYVALUE_H

