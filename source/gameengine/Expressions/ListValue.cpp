// ListValue.cpp: implementation of the CListValue class.
//
//////////////////////////////////////////////////////////////////////
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

#include "ListValue.h"
#include "StringValue.h"
#include "VoidValue.h"
#include <algorithm>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if  ((PY_MAJOR_VERSION == 2) &&(PY_MINOR_VERSION < 5))
#define Py_ssize_t int
#endif

Py_ssize_t listvalue_bufferlen(PyObject* list)
{
	return (Py_ssize_t)( ((CListValue*)list)->GetCount());
}

PyObject* listvalue_buffer_item(PyObject* list,Py_ssize_t index)
{
	if (index >= 0 && index < ((CListValue*) list)->GetCount())
	{
		PyObject* pyobj = ((CListValue*) list)->GetValue(index)->ConvertValueToPython();
		if (pyobj)
			return pyobj;
		else
			return ((CListValue*) list)->GetValue(index)->AddRef();

	}
	PyErr_SetString(PyExc_IndexError, "Python ListIndex out of range");
	return NULL;
}

PyObject* listvalue_mapping_subscript(PyObject* list,PyObject* pyindex)
{
	if (PyString_Check(pyindex))
	{
		STR_String  index(PyString_AsString(pyindex));
		CValue *item = ((CListValue*) list)->FindValue(index);
		if (item)
			return (PyObject*) item;
			
	}
	if (PyInt_Check(pyindex))
	{
		int index = PyInt_AsLong(pyindex);
		return listvalue_buffer_item(list, index);
	}
	
	PyObject *pyindex_str = PyObject_Repr(pyindex); /* new ref */
	STR_String index_str(PyString_AsString(pyindex_str));
	PyErr_Format(PyExc_KeyError, "'%s' not in list", index_str.Ptr());
	Py_DECREF(pyindex_str);
	return NULL;
}


/* just slice it into a python list... */
PyObject* listvalue_buffer_slice(PyObject* list,Py_ssize_t ilow, Py_ssize_t ihigh)
{
	int i, j;
	PyListObject *newlist;

	if (ilow < 0) ilow = 0;

	int n = ((CListValue*) list)->GetCount();

	if (ihigh >= n)
		ihigh = n;
    if (ihigh < ilow)
        ihigh = ilow;

	newlist = (PyListObject *) PyList_New(ihigh - ilow);
	if (!newlist)
		return NULL;

	for (i = ilow, j = 0; i < ihigh; i++, j++)
	{
		PyObject* pyobj = ((CListValue*) list)->GetValue(i)->ConvertValueToPython();
		if (!pyobj)
			pyobj = ((CListValue*) list)->GetValue(i)->AddRef();
		newlist->ob_item[j] = pyobj;
	}	
	return (PyObject *) newlist;
}



static PyObject *
listvalue_buffer_concat(PyObject * self, PyObject * other)
{
	// for now, we support CListValue concatenated with items
	// and CListValue concatenated to Python Lists
	// and CListValue concatenated with another CListValue

	CListValue* listval = (CListValue*) self;
	listval->AddRef();
	if (other->ob_type == &PyList_Type)
	{
		bool error = false;

		int i;
		int numitems = PyList_Size(other);
		for (i=0;i<numitems;i++)
		{
			PyObject* listitem = PyList_GetItem(other,i);
			CValue* listitemval = listval->ConvertPythonToValue(listitem);
			if (listitemval)
			{
				listval->Add(listitemval);
			} else
			{
				error = true;
			}
		}

		if (error) {
			PyErr_SetString(PyExc_SystemError, "Python Error: couldn't add one or more items to a list");
			return NULL;
		}

	} else
	{
		if (other->ob_type == &CListValue::Type)
		{
			// add items from otherlist to this list
			CListValue* otherval = (CListValue*) other;
			

			for (int i=0;i<otherval->GetCount();i++)
			{
				otherval->Add(listval->GetValue(i)->AddRef());
			}
		}
		else
		{
			CValue* objval = listval->ConvertPythonToValue(other);
			if (objval)
			{
				listval->Add(objval);
			} else
			{
				PyErr_SetString(PyExc_SystemError, "Python Error: couldn't add item to a list");  
				return NULL;
			}
		}
	}

	return self;
}



static  PySequenceMethods listvalue_as_sequence = {
	listvalue_bufferlen,//(inquiry)buffer_length, /*sq_length*/
	listvalue_buffer_concat, /*sq_concat*/
 	NULL, /*sq_repeat*/
	listvalue_buffer_item, /*sq_item*/
	listvalue_buffer_slice, /*sq_slice*/
 	NULL, /*sq_ass_item*/
 	NULL /*sq_ass_slice*/
};



/* Is this one used ? */
static  PyMappingMethods instance_as_mapping = {
	listvalue_bufferlen, /*mp_length*/
	listvalue_mapping_subscript, /*mp_subscript*/
	NULL /*mp_ass_subscript*/
};



PyTypeObject CListValue::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
	0,				/*ob_size*/
	"CListValue",			/*tp_name*/
	sizeof(CListValue),		/*tp_basicsize*/
	0,				/*tp_itemsize*/
	/* methods */
	PyDestructor,	  		/*tp_dealloc*/
	0,			 	/*tp_print*/
	__getattr, 			/*tp_getattr*/
	__setattr, 			/*tp_setattr*/
	0,			        /*tp_compare*/
	__repr,			        /*tp_repr*/
	0,			        /*tp_as_number*/
	&listvalue_as_sequence, /*tp_as_sequence*/
	&instance_as_mapping,	        /*tp_as_mapping*/
	0,			        /*tp_hash*/
	0,				/*tp_call */
};



PyParentObject CListValue::Parents[] = {
	&CListValue::Type,
	&CValue::Type,
		NULL
};




PyMethodDef CListValue::Methods[] = {
	{"append", (PyCFunction)CListValue::sPyappend,METH_VARARGS},
	{"reverse", (PyCFunction)CListValue::sPyreverse,METH_VARARGS},
	{"index", (PyCFunction)CListValue::sPyindex,METH_VARARGS},
	{"count", (PyCFunction)CListValue::sPycount,METH_VARARGS},
	
	{NULL,NULL} //Sentinel
};



PyObject* CListValue::_getattr(const STR_String& attr) {
	_getattr_up(CValue);
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CListValue::CListValue(PyTypeObject *T ) 
: CPropValue(T)
{
	m_bReleaseContents=true;	
}



CListValue::~CListValue()
{

	if (m_bReleaseContents) {
		for (unsigned int i=0;i<m_pValueArray.size();i++) {
			m_pValueArray[i]->Release();
		}
	}
}


static STR_String gstrListRep=STR_String("List");

const STR_String & CListValue::GetText()
{
	gstrListRep = "[";
	STR_String commastr = "";

	for (int i=0;i<GetCount();i++)
	{
		gstrListRep += commastr;
		gstrListRep += GetValue(i)->GetText();
		commastr = ",";
	}
	gstrListRep += "]";

	return gstrListRep;
}



CValue* CListValue::GetReplica() { 
	CListValue* replica = new CListValue(*this);

	CValue::AddDataToReplica(replica);

	replica->m_bReleaseContents=true; // for copy, complete array is copied for now...
	// copy all values
	int numelements = m_pValueArray.size();
	unsigned int i=0;
	replica->m_pValueArray.resize(numelements);
	for (i=0;i<m_pValueArray.size();i++)
		replica->m_pValueArray[i] = m_pValueArray[i]->GetReplica();


	return replica;
};



void CListValue::SetValue(int i, CValue *val)
{
	assertd(i < m_pValueArray.size());
	m_pValueArray[i]=val;
}



void CListValue::Resize(int num)
{
	m_pValueArray.resize(num);
}



void CListValue::Remove(int i)
{
	assertd(i<m_pValueArray.size());
	m_pValueArray.erase(m_pValueArray.begin()+i);
}



void CListValue::ReleaseAndRemoveAll()
{
	for (unsigned int i=0;i<m_pValueArray.size();i++)
		m_pValueArray[i]->Release();
	m_pValueArray.clear();//.Clear();
}



CValue* CListValue::FindValue(const STR_String & name)
{
	CValue* resultval = NULL;
	int i=0;
	
	while (!resultval && i < GetCount())
	{
		CValue* myval = GetValue(i);
				
		if (myval->GetName() == name)
			resultval = GetValue(i)->AddRef(); // add referencecount
		else
			i++;
		
	}
	return resultval;
}



bool CListValue::SearchValue(CValue *val)
{
	for (int i=0;i<GetCount();i++)
		if (val == GetValue(i))
			return true;
	return false;
}



void CListValue::SetReleaseOnDestruct(bool bReleaseContents)
{
	m_bReleaseContents = bReleaseContents;
}



bool CListValue::RemoveValue(CValue *val)
{
	bool result=false;

	for (int i=GetCount()-1;i>=0;i--)
		if (val == GetValue(i))
		{
			Remove(i);
			result=true;
		}
	return result;
}



void CListValue::MergeList(CListValue *otherlist)
{

	int numelements = this->GetCount();
	int numotherelements = otherlist->GetCount();


	Resize(numelements+numotherelements);

	for (int i=0;i<numotherelements;i++)
	{
		SetValue(i+numelements,otherlist->GetValue(i)->AddRef());
	}

}



PyObject* CListValue::Pyappend(PyObject* self, 
			       PyObject* args, 
			       PyObject* kwds)
{

	PyObject* pyobj = NULL;
	if (PyArg_ParseTuple(args,"O",&pyobj))
	{
		return listvalue_buffer_concat(self,pyobj);
	}
	else
	{
	   return NULL;	     
	}

	
}



PyObject* CListValue::Pyreverse(PyObject* self, 
			       PyObject* args, 
			       PyObject* kwds)
{
	std::reverse(m_pValueArray.begin(),m_pValueArray.end());

	Py_Return;
	
}



bool CListValue::CheckEqual(CValue* first,CValue* second)
{
	bool result = false;

	CValue* eqval =  ((CValue*)first)->Calc(VALUE_EQL_OPERATOR,(CValue*)second);
	STR_String txt = eqval->GetText();
	eqval->Release();
	if (txt=="TRUE")
	{
		result = true;
	}
	return result;

}



PyObject* CListValue::Pyindex(PyObject* self, 
			       PyObject* args, 
			       PyObject* kwds)
{
	PyObject* result = NULL;

	PyObject* pyobj = NULL;
	if (PyArg_ParseTuple(args,"O",&pyobj))
	{
	
		CValue* checkobj = ConvertPythonToValue(pyobj);
		int numelem = GetCount();
		for (int i=0;i<numelem;i++)
		{
			CValue* elem = 			GetValue(i);
			if (CheckEqual(checkobj,elem))
			{
				result = PyInt_FromLong(i);
				break;
			}
		}
		checkobj->Release();
	}

	return result;
	
}



PyObject* CListValue::Pycount(PyObject* self, 
			       PyObject* args, 
			       PyObject* kwds)
{
	
	int numfound = 0;

	PyObject* pyobj = NULL;
	if (PyArg_ParseTuple(args,"O",&pyobj))
	{
		CValue* checkobj = ConvertPythonToValue(pyobj);
		int numelem = GetCount();
		for (int i=0;i<numelem;i++)
		{
			CValue* elem = 			GetValue(i);
			if (CheckEqual(checkobj,elem))
			{
				numfound ++;
			}
		}
		checkobj->Release();
	}

	return PyInt_FromLong(numfound);
}



/* --------------------------------------------------------------------- 
 * Some stuff taken from the header
 * --------------------------------------------------------------------- */
CValue* CListValue::Calc(VALUE_OPERATOR op,CValue *val) 
{
	assert(false); // todo: implement me!
	return NULL;
}



CValue* CListValue::CalcFinal(VALUE_DATA_TYPE dtype,
							  VALUE_OPERATOR op, 
							  CValue* val) 
{
	assert(false); // todo: implement me!
	return NULL;
}



void CListValue::Add(CValue* value)
{
	m_pValueArray.push_back(value);
}



float CListValue::GetNumber()
{
	return -1;
}



void CListValue::SetModified(bool bModified)
{	
	CValue::SetModified(bModified);
	int numels = GetCount();

	for (int i=0;i<numels;i++)
		GetValue(i)->SetModified(bModified);
}



bool CListValue::IsModified()
{
	bool bmod = CValue::IsModified(); //normal own flag
	int numels = GetCount();

	for (int i=0;i<numels;i++)
		bmod = bmod || GetValue(i)->IsModified();

	return bmod;
}
