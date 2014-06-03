/*
 * BoolValue.h: interface for the CBoolValue class.
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

/** \file BoolValue.h
 *  \ingroup expressions
 */

#ifndef __BOOLVALUE_H__
#define __BOOLVALUE_H__

#include "Value.h"

/**
 *	Smart Boolean Value class.
 * Is used by parser when an expression tree is build containing booleans.
 */

class CBoolValue : public CPropValue  
{

	//PLUGIN_DECLARE_SERIAL(CBoolValue,CValue)

public:
	static const STR_String sTrueString;
	static const STR_String sFalseString;

	CBoolValue();
	CBoolValue(bool inBool);
	CBoolValue(bool innie, const char *name, AllocationTYPE alloctype = CValue::HEAPVALUE);

	virtual const STR_String& GetText();
	virtual double		GetNumber();
	virtual int			GetValueType();
	bool				GetBool();
	virtual void		SetValue(CValue* newval);
	
	virtual CValue*		Calc(VALUE_OPERATOR op, CValue *val);
	virtual CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	
	void				Configure(CValue* menuvalue);
	virtual CValue*		GetReplica();
#ifdef WITH_PYTHON
	virtual PyObject*	ConvertValueToPython();
#endif

private:
	bool				m_bool;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CBoolValue")
#endif
};

#endif  /* __BOOLVALUE_H__ */
