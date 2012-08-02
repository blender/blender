/*
 * ListValue.h: interface for the CListValue class.
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

/** \file ListValue.h
 *  \ingroup expressions
 */

#ifndef __LISTVALUE_H__
#define __LISTVALUE_H__

#include "Value.h"

class CListValue : public CPropValue  
{
	Py_Header
	//PLUGIN_DECLARE_SERIAL (CListValue,CValue)

public:
	CListValue();
	virtual ~CListValue();

	void AddConfigurationData(CValue* menuvalue);
	void Configure(CValue* menuvalue);
	void Add(CValue* value);

	/** \attention not implemented yet :( */
	virtual CValue* Calc(VALUE_OPERATOR op,CValue *val);
	virtual CValue* CalcFinal(VALUE_DATA_TYPE dtype,
							  VALUE_OPERATOR op,
							  CValue* val);
	virtual double GetNumber();
	virtual CValue* GetReplica();

public:
	void MergeList(CListValue* otherlist);
	bool RemoveValue(CValue* val);
	void SetReleaseOnDestruct(bool bReleaseContents);
	bool SearchValue(CValue* val);
	
	CValue* FindValue(const STR_String & name);
	CValue* FindValue(const char *name);

	void ReleaseAndRemoveAll();
	virtual void SetModified(bool bModified);
	virtual inline bool IsModified();
	void Remove(int i);
	void Resize(int num);
	void SetValue(int i,CValue* val);
	CValue* GetValue(int i) { assertd(i < m_pValueArray.size()); return m_pValueArray[i]; }
	int GetCount() { return m_pValueArray.size(); }
	virtual const STR_String & GetText();

	bool CheckEqual(CValue* first,CValue* second);

#ifdef WITH_PYTHON
	virtual PyObject* py_repr(void) {
		PyObject *py_proxy= this->GetProxy();
		PyObject *py_list= PySequence_List(py_proxy);
		PyObject *py_string= PyObject_Repr(py_list);
		Py_DECREF(py_list);
		Py_DECREF(py_proxy);
		return py_string;
	}

	KX_PYMETHOD_O(CListValue,append);
	KX_PYMETHOD_NOARGS(CListValue,reverse);
	KX_PYMETHOD_O(CListValue,index);
	KX_PYMETHOD_O(CListValue,count);
	KX_PYMETHOD_VARARGS(CListValue,get);
	KX_PYMETHOD_O(CListValue,from_id);
#endif
	
private:

	std::vector<CValue*> m_pValueArray;
	bool	m_bReleaseContents;
};

#endif // !defined __LISTVALUE_H__

