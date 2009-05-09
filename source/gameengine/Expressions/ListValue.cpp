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

Py_ssize_t listvalue_bufferlen(PyObject* self)
{
	CListValue *list= static_cast<CListValue *>(BGE_PROXY_REF(self));
	if (list==NULL)
		return 0;
	
	return (Py_ssize_t)list->GetCount();
}

PyObject* listvalue_buffer_item(PyObject* self, Py_ssize_t index)
{
	CListValue *list= static_cast<CListValue *>(BGE_PROXY_REF(self));
	CValue *cval;
	
	if (list==NULL) {
		PyErr_SetString(PyExc_SystemError, "val = CList[i], "BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	int count = list->GetCount();
	
	if (index < 0)
		index = count+index;
	
	if (index < 0 || index >= count) {
		PyErr_SetString(PyExc_IndexError, "CList[i]: Python ListIndex out of range in CValueList");
		return NULL;
	}
	
	cval= list->GetValue(index);
	
	PyObject* pyobj = cval->ConvertValueToPython();
	if (pyobj)
		return pyobj;
	else
		return cval->GetProxy();
}

PyObject* listvalue_mapping_subscript(PyObject* self, PyObject* pyindex)
{
	CListValue *list= static_cast<CListValue *>(BGE_PROXY_REF(self));
	if (list==NULL) {
		PyErr_SetString(PyExc_SystemError, "value = CList[i], "BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	if (PyString_Check(pyindex))
	{
		STR_String  index(PyString_AsString(pyindex));
		CValue *item = ((CListValue*) list)->FindValue(index);
		if (item)
		{
			item->Release(); /* FindValue() AddRef's */ 
			return item->GetProxy();
		}
	}
	else if (PyInt_Check(pyindex))
	{
		int index = PyInt_AsLong(pyindex);
		return listvalue_buffer_item(self, index); /* wont add a ref */
	}
	
	PyObject *pyindex_str = PyObject_Repr(pyindex); /* new ref */
	PyErr_Format(PyExc_KeyError, "CList[key]: '%s' key not in list", PyString_AsString(pyindex_str));
	Py_DECREF(pyindex_str);
	return NULL;
}


/* just slice it into a python list... */
PyObject* listvalue_buffer_slice(PyObject* self,Py_ssize_t ilow, Py_ssize_t ihigh)
{
	CListValue *list= static_cast<CListValue *>(BGE_PROXY_REF(self));
	if (list==NULL) {
		PyErr_SetString(PyExc_SystemError, "val = CList[i:j], "BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	int i, j;
	PyObject *newlist;

	if (ilow < 0) ilow = 0;

	int n = ((CListValue*) list)->GetCount();

	if (ihigh >= n)
		ihigh = n;
    if (ihigh < ilow)
        ihigh = ilow;

	newlist = PyList_New(ihigh - ilow);
	if (!newlist)
		return NULL;

	for (i = ilow, j = 0; i < ihigh; i++, j++)
	{
		PyObject* pyobj = list->GetValue(i)->ConvertValueToPython();
		if (!pyobj)
			pyobj = list->GetValue(i)->GetProxy();
		PyList_SET_ITEM(newlist, i, pyobj);
	}	
	return newlist;
}


/* clist + list, return a list that python owns */
static PyObject *listvalue_buffer_concat(PyObject * self, PyObject * other)
{
	CListValue *listval= static_cast<CListValue *>(BGE_PROXY_REF(self));
	int i, numitems, numitems_orig;
	
	if (listval==NULL) {
		PyErr_SetString(PyExc_SystemError, "CList+other, "BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	numitems_orig= listval->GetCount();
	
	// for now, we support CListValue concatenated with items
	// and CListValue concatenated to Python Lists
	// and CListValue concatenated with another CListValue
	
	/* Shallow copy, dont use listval->GetReplica(), it will screw up with KX_GameObjects */
	CListValue* listval_new = new CListValue();
	
	if (PyList_Check(other))
	{
		CValue* listitemval;
		bool error = false;
		
		numitems = PyList_Size(other);
		
		/* copy the first part of the list */
		listval_new->Resize(numitems_orig + numitems);
		for (i=0;i<numitems_orig;i++)
			listval_new->SetValue(i, listval->GetValue(i)->AddRef());
		
		for (i=0;i<numitems;i++)
		{
			listitemval = listval->ConvertPythonToValue(PyList_GetItem(other,i), "cList + pyList: CListValue, ");
			
			if (listitemval) {
				listval_new->SetValue(i+numitems_orig, listitemval);
			} else {
				error= true;
				break;
			}
		}
		
		if (error) {
			listval_new->Resize(numitems_orig+i); /* resize so we dont try release NULL pointers */
			listval_new->Release();
			return NULL; /* ConvertPythonToValue above sets the error */ 
		}
	
	}
	else if (PyObject_TypeCheck(other, &CListValue::Type)) {
		// add items from otherlist to this list
		CListValue* otherval = static_cast<CListValue *>(BGE_PROXY_REF(other));
		if(otherval==NULL) {
			listval_new->Release();
			PyErr_SetString(PyExc_SystemError, "CList+other, "BGE_PROXY_ERROR_MSG);
			return NULL;
		}
		
		numitems = otherval->GetCount();
		
		/* copy the first part of the list */
		listval_new->Resize(numitems_orig + numitems); /* resize so we dont try release NULL pointers */
		for (i=0;i<numitems_orig;i++)
			listval_new->SetValue(i, listval->GetValue(i)->AddRef());
		
		/* now copy the other part of the list */
		for (i=0;i<numitems;i++)
			listval_new->SetValue(i+numitems_orig, otherval->GetValue(i)->AddRef());
		
	}
	return listval_new->NewProxy(true); /* python owns this list */
}



static  PySequenceMethods listvalue_as_sequence = {
	listvalue_bufferlen,//(inquiry)buffer_length, /*sq_length*/
	listvalue_buffer_concat, /*sq_concat*/
 	NULL, /*sq_repeat*/
	listvalue_buffer_item, /*sq_item*/
#if (PY_VERSION_HEX >= 0x03000000) // TODO, slicing in py3?
	NULL,
	NULL,
	NULL,
#else
	listvalue_buffer_slice, /*sq_slice*/
 	NULL, /*sq_ass_item*/
 	NULL, /*sq_ass_slice*/
#endif
};



/* Is this one used ? */
static  PyMappingMethods instance_as_mapping = {
	listvalue_bufferlen, /*mp_length*/
	listvalue_mapping_subscript, /*mp_subscript*/
	NULL /*mp_ass_subscript*/
};



PyTypeObject CListValue::Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,				/*ob_size*/
#endif
	"CListValue",			/*tp_name*/
	sizeof(PyObjectPlus_Proxy), /*tp_basicsize*/
	0,				/*tp_itemsize*/
	/* methods */
	py_base_dealloc,	  		/*tp_dealloc*/
	0,			 	/*tp_print*/
	0, 			/*tp_getattr*/
	0, 			/*tp_setattr*/
	0,			        /*tp_compare*/
	py_base_repr,			        /*tp_repr*/
	0,			        /*tp_as_number*/
	&listvalue_as_sequence, /*tp_as_sequence*/
	&instance_as_mapping,	        /*tp_as_mapping*/
	0,			        /*tp_hash*/
	0,				/*tp_call */
	0,
	py_base_getattro,
	py_base_setattro,
	0,0,0,0,0,0,0,0,0,
	Methods
};



PyParentObject CListValue::Parents[] = {
	&CListValue::Type,
	&CValue::Type,
		NULL
};




PyMethodDef CListValue::Methods[] = {
	{"append", (PyCFunction)CListValue::sPyappend,METH_O},
	{"reverse", (PyCFunction)CListValue::sPyreverse,METH_NOARGS},
	{"index", (PyCFunction)CListValue::sPyindex,METH_O},
	{"count", (PyCFunction)CListValue::sPycount,METH_O},
	{"from_id", (PyCFunction)CListValue::sPyfrom_id,METH_O},
	
	{NULL,NULL} //Sentinel
};

PyAttributeDef CListValue::Attributes[] = {
	{ NULL }	//Sentinel
};

PyObject* CListValue::py_getattro(PyObject* attr) {
	py_getattro_up(CValue);
}

PyObject* CListValue::py_getattro_dict() {
	py_getattro_dict_up(CValue);
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

	replica->ProcessReplica();

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


PyObject* CListValue::Pyappend(PyObject* value)
{
	CValue* objval = ConvertPythonToValue(value, "CList.append(i): CValueList, ");

	if (!objval) /* ConvertPythonToValue sets the error */
		return NULL;
	
	if (!BGE_PROXY_PYOWNS(m_proxy)) {
		PyErr_SetString(PyExc_TypeError, "CList.append(i): this CValueList is used internally for the game engine and can't be modified");
		return NULL;
	}
	
	Add(objval);
	
	Py_RETURN_NONE;
}



PyObject* CListValue::Pyreverse()
{
	std::reverse(m_pValueArray.begin(),m_pValueArray.end());
	Py_RETURN_NONE;
}



bool CListValue::CheckEqual(CValue* first,CValue* second)
{
	bool result = false;

	CValue* eqval =  ((CValue*)first)->Calc(VALUE_EQL_OPERATOR,(CValue*)second);
	
	if (eqval==NULL)
		return false;
		
	STR_String txt = eqval->GetText();
	eqval->Release();
	if (txt=="TRUE")
	{
		result = true;
	}
	return result;

}



PyObject* CListValue::Pyindex(PyObject *value)
{
	PyObject* result = NULL;

	CValue* checkobj = ConvertPythonToValue(value, "val = cList[i]: CValueList, ");
	if (checkobj==NULL)
		return NULL; /* ConvertPythonToValue sets the error */

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

	if (result==NULL) {
		PyErr_SetString(PyExc_ValueError, "CList.index(x): x not in CListValue");
	}
	return result;
	
}



PyObject* CListValue::Pycount(PyObject* value)
{
	int numfound = 0;

	CValue* checkobj = ConvertPythonToValue(value, ""); /* error ignored */
	
	if (checkobj==NULL) { /* in this case just return that there are no items in the list */
		PyErr_Clear();
		return PyInt_FromLong(0);
	}

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

	return PyInt_FromLong(numfound);
}



PyObject* CListValue::Pyfrom_id(PyObject* value)
{
	uintptr_t id= (uintptr_t)PyLong_AsVoidPtr(value);
	
	if (PyErr_Occurred())
		return NULL;

	int numelem = GetCount();
	for (int i=0;i<numelem;i++)
	{
		if (reinterpret_cast<uintptr_t>(m_pValueArray[i]->m_proxy) == id)
			return GetValue(i)->GetProxy();
	}
	PyErr_SetString(PyExc_IndexError, "from_id(#): id not found in CValueList");
	return NULL;	

}


/* --------------------------------------------------------------------- 
 * Some stuff taken from the header
 * --------------------------------------------------------------------- */
CValue* CListValue::Calc(VALUE_OPERATOR op,CValue *val) 
{
	//assert(false); // todo: implement me!
	static int error_printed =  0;
	if (error_printed==0) {
		fprintf(stderr, "CValueList::Calc not yet implimented\n");
		error_printed = 1;
	}
	return NULL;
}

CValue* CListValue::CalcFinal(VALUE_DATA_TYPE dtype,
							  VALUE_OPERATOR op, 
							  CValue* val) 
{
	//assert(false); // todo: implement me!
	static int error_printed =  0;
	if (error_printed==0) {
		fprintf(stderr, "CValueList::CalcFinal not yet implimented\n");
		error_printed = 1;
	}
	return NULL;
}



void CListValue::Add(CValue* value)
{
	m_pValueArray.push_back(value);
}



double CListValue::GetNumber()
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
