/*
 * BoolValue.h: interface for the CBoolValue class.
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
#if !defined _BOOLVALUE_H
#define _BOOLVALUE_H

#include "Value.h"

/**
 *	Smart Boolean Value class.
 * Is used by parser when an expression tree is build containing booleans.
 */

class CBoolValue : public CPropValue  
{

	//PLUGIN_DECLARE_SERIAL(CBoolValue,CValue)	

public:
	CBoolValue();
	CBoolValue(bool inBool);
	CBoolValue(bool innie, STR_String name, AllocationTYPE alloctype = CValue::HEAPVALUE);

	virtual const STR_String& GetText();
	virtual float		GetNumber();
	bool				GetBool();
	virtual void		SetValue(CValue* newval);
	
	virtual CValue*		Calc(VALUE_OPERATOR op, CValue *val);
	virtual CValue*		CalcFinal(VALUE_DATA_TYPE dtype, VALUE_OPERATOR op, CValue *val);
	
	void				Configure(CValue* menuvalue);
	virtual CValue*		GetReplica();
	virtual PyObject*	ConvertValueToPython();

private:
	bool				m_bool;
};

#endif // !defined _BOOLVALUE_H
