/*
 * StringValue.h: interface for the CStringValue class.
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

/** \file EXP_StringValue.h
 *  \ingroup expressions
 */

#ifndef __EXP_STRINGVALUE_H__
#define __EXP_STRINGVALUE_H__

#include "EXP_Value.h"

class CStringValue : public CPropValue  
{

	
	//PLUGIN_DECLARE_SERIAL(CStringValue,CValue)
public:
	/// Construction / destruction
	CStringValue();
	CStringValue(const char *txt, const char *name, AllocationTYPE alloctype = CValue::HEAPVALUE);

	virtual ~CStringValue() {}
	/// CValue implementation
	virtual bool		IsEqual(const STR_String & other);
	virtual const STR_String &	GetText();
	virtual double		GetNumber();
	virtual int			GetValueType();
	
	virtual	CValue*		Calc(VALUE_OPERATOR op, CValue *val);
	virtual	CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	virtual void		SetValue(CValue* newval) { 	m_strString = newval->GetText(); SetModified(true);	}
	virtual CValue*		GetReplica();
#ifdef WITH_PYTHON
	virtual PyObject*	ConvertValueToPython() {
		return PyUnicode_From_STR_String(m_strString);
	}
#endif  /* WITH_PYTHON */

private:
	// data member
	STR_String				m_strString;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:CStringValue")
#endif
};

#endif  /* __EXP_STRINGVALUE_H__ */
