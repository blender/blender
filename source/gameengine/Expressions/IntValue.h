/*
 * IntValue.h: interface for the CIntValue class.
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
#if !defined _INTVALUE_H
#define _INTVALUE_H


#include "Value.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

class CIntValue : public CPropValue  
{
	//PLUGIN_DECLARE_SERIAL (CIntValue,CValue)

public:
	virtual const STR_String& GetText();
	virtual float			GetNumber();
	
	int GetInt();
	CIntValue();
	CIntValue(int innie);
	CIntValue(int innie,
			  STR_String name,
			  AllocationTYPE alloctype=CValue::HEAPVALUE);
	
	virtual	CValue* Calc(VALUE_OPERATOR op,
						 CValue *val);

	virtual	CValue* CalcFinal(VALUE_DATA_TYPE dtype,
							  VALUE_OPERATOR op,
							  CValue *val);

	virtual void	SetValue(CValue* newval);

	void			Configure(CValue* menuvalue);
	void			AddConfigurationData(CValue* menuvalue);
	virtual CValue*	GetReplica();
	virtual PyObject*	ConvertValueToPython();

protected:
	virtual			~CIntValue();

private:
	int				m_int;
	STR_String*		m_pstrRep;

};

#endif // !defined _INTVALUE_H

