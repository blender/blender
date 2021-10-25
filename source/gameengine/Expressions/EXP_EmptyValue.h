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

/** \file EXP_EmptyValue.h
 *  \ingroup expressions
 */

#ifndef __EXP_EMPTYVALUE_H__
#define __EXP_EMPTYVALUE_H__

#include "EXP_Value.h"

class CListValue;

class CEmptyValue : public CPropValue  
{
	//PLUGIN_DECLARE_SERIAL (CEmptyValue,CValue)
public:
	CEmptyValue();
	virtual					~CEmptyValue();

	virtual const STR_String &	GetText();
	virtual double			GetNumber();
	virtual int				GetValueType();
	CListValue*				GetPolySoup();
	virtual double*			GetVector3(bool bGetTransformedVec=false);
	bool					IsInside(CValue* testpoint,bool bBorderInclude=true);
	CValue *				Calc(VALUE_OPERATOR op, CValue *val);
	CValue *				CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	virtual CValue*			GetReplica();


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CEmptyValue")
#endif
};

#endif  /* __EXP_EMPTYVALUE_H__ */
