/*
 * StringValue.h: interface for the CStringValue class.
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

#ifndef __STRINGVALUE_H__
#define __STRINGVALUE_H__

#include "Value.h"

class CStringValue : public CPropValue  
{

	
	//PLUGIN_DECLARE_SERIAL(CStringValue,CValue)		
public:
	/// Construction / destruction
	CStringValue();
	CStringValue (STR_String txt, STR_String name , AllocationTYPE alloctype = CValue::HEAPVALUE);

	virtual ~CStringValue() {
	};
	/// CValue implementation
	virtual bool		IsEqual(const STR_String & other);
	virtual const STR_String &	GetText();
	virtual float		GetNumber();
	
	virtual	CValue*		Calc(VALUE_OPERATOR op, CValue *val);
	virtual	CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	virtual void		SetValue(CValue* newval) { 	m_strString = newval->GetText(); SetModified(true);	};
	virtual CValue*		GetReplica();
	virtual PyObject*	ConvertValueToPython() {
		return PyString_FromString(m_strString.Ptr());
	}

private:
	// data member
	STR_String				m_strString;
};

#endif

