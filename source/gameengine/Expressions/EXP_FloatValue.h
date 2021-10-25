/*
 * FloatValue.h: interface for the CFloatValue class.
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

/** \file EXP_FloatValue.h
 *  \ingroup expressions
 */

#ifndef __EXP_FLOATVALUE_H__
#define __EXP_FLOATVALUE_H__

#include "EXP_Value.h"

class CFloatValue : public CPropValue 
{
	//PLUGIN_DECLARE_SERIAL (CFloatValue,CValue)
public:
	CFloatValue();
	CFloatValue(float fl);
	CFloatValue(float fl,const char *name,AllocationTYPE alloctype=CValue::HEAPVALUE);

	virtual const STR_String & GetText();

	void Configure(CValue* menuvalue);
	virtual double GetNumber();
	virtual int GetValueType();
	virtual void SetValue(CValue* newval);
	float GetFloat();
	void SetFloat(float fl);
	virtual ~CFloatValue();
	virtual CValue* GetReplica();
	virtual CValue* Calc(VALUE_OPERATOR op, CValue *val);
	virtual CValue* CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
#ifdef WITH_PYTHON
	virtual PyObject*	ConvertValueToPython();
#endif

protected:
	float m_float;
	STR_String* m_pstrRep;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CFloatValue")
#endif
};

#endif  /* __EXP_FLOATVALUE_H__ */
