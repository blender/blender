/** \file gameengine/Expressions/Value.cpp
 *  \ingroup expressions
 */
// Value.cpp: implementation of the CValue class.
// developed at Eindhoven University of Technology, 1997
// by the OOPS team
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
#include "EXP_Value.h"
#include "EXP_BoolValue.h"
#include "EXP_FloatValue.h"
#include "EXP_IntValue.h"
#include "EXP_VectorValue.h"
#include "EXP_VoidValue.h"
#include "EXP_StringValue.h"
#include "EXP_ErrorValue.h"
#include "EXP_ListValue.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

double CValue::m_sZeroVec[3] = {0.0,0.0,0.0};

#ifdef WITH_PYTHON

PyTypeObject CValue::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"CValue",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	0,0,0,0,0,
	NULL,
	NULL,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef CValue::Methods[] = {
	{NULL,NULL} //Sentinel
};
#endif // WITH_PYTHON


/*#define CVALUE_DEBUG*/
#ifdef CVALUE_DEBUG
int gRefCount;
struct SmartCValueRef 
{
	CValue *m_ref;
	int m_count;
	SmartCValueRef(CValue *ref)
	{
		m_ref = ref;
		m_count = gRefCount++;
	}
};

#include <vector>

std::vector<SmartCValueRef> gRefList;
#endif

#ifdef DEBUG
//int gRefCountValue;
#endif

CValue::CValue()
		: PyObjectPlus(),
	
m_pNamedPropertyArray(NULL),
m_refcount(1)
/*
pre: false
effect: constucts a CValue
*/
{
	//debug(gRefCountValue++)	// debugging
#ifdef DEBUG
	//gRefCountValue++;
#ifdef CVALUE_DEBUG
	gRefList.push_back(SmartCValueRef(this));
#endif
#endif
}



CValue::~CValue()
/*
pre:
effect: deletes the object
*/
{
	ClearProperties();

	assertd (m_refcount==0);
#ifdef CVALUE_DEBUG
	std::vector<SmartCValueRef>::iterator it;
	for (it=gRefList.begin(); it!=gRefList.end(); it++)
	{
		if (it->m_ref == this)
		{
			*it = gRefList.back();
			gRefList.pop_back();
			break;
		}
	}
#endif
}


/* UNUSED */
#if 0
#define VALUE_SUB(val1, val2) (val1)->Calc(VALUE_SUB_OPERATOR, val2)
#define VALUE_MUL(val1, val2) (val1)->Calc(VALUE_MUL_OPERATOR, val2)
#define VALUE_DIV(val1, val2) (val1)->Calc(VALUE_DIV_OPERATOR, val2)
#define VALUE_NEG(val1)       (val1)->Calc(VALUE_NEG_OPERATOR, val1)
#define VALUE_POS(val1)       (val1)->Calc(VALUE_POS_OPERATOR, val1)
#endif

STR_String CValue::op2str(VALUE_OPERATOR op)
{
	//pre:
	//ret: the stringrepresentation of operator op
	
	STR_String opmsg;
	switch (op) {
	case VALUE_MOD_OPERATOR:
		opmsg = " % ";
		break;
	case VALUE_ADD_OPERATOR:
		opmsg = " + ";
		break;
	case VALUE_SUB_OPERATOR:
		opmsg = " - ";
		break;
	case VALUE_MUL_OPERATOR:
		opmsg = " * ";
		break;
	case VALUE_DIV_OPERATOR:
		opmsg = " / ";
		break;
	case VALUE_NEG_OPERATOR:
		opmsg = " -";
		break;
	case VALUE_POS_OPERATOR:
		opmsg = " +";
		break;
	case VALUE_AND_OPERATOR:
		opmsg = " & ";
		break;
	case VALUE_OR_OPERATOR:
		opmsg = " | ";
		break;
	case VALUE_EQL_OPERATOR:
		opmsg = " = ";
		break;
	case VALUE_NEQ_OPERATOR:
		opmsg = " != ";
		break;
	case VALUE_NOT_OPERATOR:
		opmsg = " !";
		break;
	default:
		opmsg="Error in Errorhandling routine.";
		//		AfxMessageBox("Invalid operator");
		break;
	}
	return opmsg;
}





//---------------------------------------------------------------------------------------------------------------------
//	Property Management
//---------------------------------------------------------------------------------------------------------------------



//
// Set property <ioProperty>, overwrites and releases a previous property with the same name if needed
//
void CValue::SetProperty(const STR_String & name,CValue* ioProperty)
{
	if (ioProperty==NULL)
	{	// Check if somebody is setting an empty property
		trace("Warning:trying to set empty property!");
		return;
	}

	if (m_pNamedPropertyArray)
	{	// Try to replace property (if so -> exit as soon as we replaced it)
		CValue* oldval = (*m_pNamedPropertyArray)[name];
		if (oldval)
			oldval->Release();
	}
	else { // Make sure we have a property array
		m_pNamedPropertyArray = new std::map<STR_String,CValue *>;
	}
	
	// Add property at end of array
	(*m_pNamedPropertyArray)[name] = ioProperty->AddRef();//->Add(ioProperty);
}

void CValue::SetProperty(const char* name,CValue* ioProperty)
{
	if (ioProperty==NULL)
	{	// Check if somebody is setting an empty property
		trace("Warning:trying to set empty property!");
		return;
	}

	if (m_pNamedPropertyArray)
	{	// Try to replace property (if so -> exit as soon as we replaced it)
		CValue* oldval = (*m_pNamedPropertyArray)[name];
		if (oldval)
			oldval->Release();
	}
	else { // Make sure we have a property array
		m_pNamedPropertyArray = new std::map<STR_String,CValue *>;
	}
	
	// Add property at end of array
	(*m_pNamedPropertyArray)[name] = ioProperty->AddRef();//->Add(ioProperty);
}

//
// Get pointer to a property with name <inName>, returns NULL if there is no property named <inName>
//
CValue* CValue::GetProperty(const STR_String & inName)
{
	if (m_pNamedPropertyArray) {
		std::map<STR_String,CValue*>::iterator it = m_pNamedPropertyArray->find(inName);
		if (it != m_pNamedPropertyArray->end())
			return (*it).second;
	}
	return NULL;
}

CValue* CValue::GetProperty(const char *inName)
{
	if (m_pNamedPropertyArray) {
		std::map<STR_String,CValue*>::iterator it = m_pNamedPropertyArray->find(inName);
		if (it != m_pNamedPropertyArray->end())
			return (*it).second;
	}
	return NULL;
}

//
// Get text description of property with name <inName>, returns an empty string if there is no property named <inName>
//
const STR_String& CValue::GetPropertyText(const STR_String & inName)
{
	const static STR_String sEmpty("");

	CValue *property = GetProperty(inName);
	if (property)
		return property->GetText();
	else
		return sEmpty;
}

float CValue::GetPropertyNumber(const STR_String& inName,float defnumber)
{
	CValue *property = GetProperty(inName);
	if (property)
		return property->GetNumber(); 
	else
		return defnumber;
}



//
// Remove the property named <inName>, returns true if the property was succesfully removed, false if property was not found or could not be removed
//
bool CValue::RemoveProperty(const char *inName)
{
	// Check if there are properties at all which can be removed
	if (m_pNamedPropertyArray)
	{
		std::map<STR_String,CValue*>::iterator it = m_pNamedPropertyArray->find(inName);
		if (it != m_pNamedPropertyArray->end())
		{
			((*it).second)->Release();
			m_pNamedPropertyArray->erase(it);
			return true;
		}
	}
	
	return false;
}

//
// Get Property Names
//
vector<STR_String> CValue::GetPropertyNames()
{
	vector<STR_String> result;
	if (!m_pNamedPropertyArray) return result;
	result.reserve(m_pNamedPropertyArray->size());
	
	std::map<STR_String,CValue*>::iterator it;
	for (it= m_pNamedPropertyArray->begin(); (it != m_pNamedPropertyArray->end()); it++)
	{
		result.push_back((*it).first);
	}
	return result;
}

//
// Clear all properties
//
void CValue::ClearProperties()
{
	// Check if we have any properties
	if (m_pNamedPropertyArray == NULL)
		return;

	// Remove all properties
	std::map<STR_String,CValue*>::iterator it;
	for (it= m_pNamedPropertyArray->begin();(it != m_pNamedPropertyArray->end()); it++)
	{
		CValue* tmpval = (*it).second;
		//STR_String name = (*it).first;
		tmpval->Release();
	}

	// Delete property array
	delete m_pNamedPropertyArray;
	m_pNamedPropertyArray=NULL;
}



//
// Set all properties' modified flag to <inModified>
//
void CValue::SetPropertiesModified(bool inModified)
{
	if (!m_pNamedPropertyArray) return;
	std::map<STR_String,CValue*>::iterator it;
	
	for (it= m_pNamedPropertyArray->begin();(it != m_pNamedPropertyArray->end()); it++)
		((*it).second)->SetModified(inModified);
}



//
// Check if any of the properties in this value have been modified
//
bool CValue::IsAnyPropertyModified()
{
	if (!m_pNamedPropertyArray) return false;
	std::map<STR_String,CValue*>::iterator it;
	
	for (it= m_pNamedPropertyArray->begin();(it != m_pNamedPropertyArray->end()); it++)
		if (((*it).second)->IsModified())
			return true;
	
	return false;
}



//
// Get property number <inIndex>
//
CValue* CValue::GetProperty(int inIndex)
{

	int count=0;
	CValue* result = NULL;

	if (m_pNamedPropertyArray)
	{
		std::map<STR_String,CValue*>::iterator it;
		for (it= m_pNamedPropertyArray->begin(); (it != m_pNamedPropertyArray->end()); it++)
		{
			if (count++ == inIndex)
			{
				result = (*it).second;
				break;
			}
		}

	}
	return result;
}



//
// Get the amount of properties assiocated with this value
//
int CValue::GetPropertyCount()
{
	if (m_pNamedPropertyArray)
		return m_pNamedPropertyArray->size();
	else
		return 0;
}


double*		CValue::GetVector3(bool bGetTransformedVec)
{
	assertd(false); // don't get vector from me
	return m_sZeroVec;//::sZero;
}


/*---------------------------------------------------------------------------------------------------------------------
	Reference Counting
---------------------------------------------------------------------------------------------------------------------*/



//
// Release a reference to this value (when reference count reaches 0, the value is removed from the heap)
//



//
// Disable reference counting for this value
//
void CValue::DisableRefCount()
{
	assertd(m_refcount == 1);
	m_refcount--;

	//debug(gRefCountValue--);
#ifdef DEBUG
	//gRefCountValue--;
#endif
	m_ValFlags.RefCountDisabled=true;
}



void CValue::ProcessReplica() /* was AddDataToReplica in 2.48 */
{
	m_refcount = 1;
	
#ifdef DEBUG
	//gRefCountValue++;
#endif
	PyObjectPlus::ProcessReplica();

	m_ValFlags.RefCountDisabled = false;

	/* copy all props */
	if (m_pNamedPropertyArray)
	{
		std::map<STR_String,CValue*> *pOldArray = m_pNamedPropertyArray;
		m_pNamedPropertyArray=NULL;
		std::map<STR_String,CValue*>::iterator it;
		for (it= pOldArray->begin(); (it != pOldArray->end()); it++)
		{
			CValue *val = (*it).second->GetReplica();
			SetProperty((*it).first,val);
			val->Release();
		}
	}
}



int CValue::GetValueType()
{
	return VALUE_NO_TYPE;
}



CValue*	CValue::FindIdentifier(const STR_String& identifiername)
{

	CValue* result = NULL;

	int pos = 0;
	// if a dot exists, explode the name into pieces to get the subcontext
	if ((pos=identifiername.Find('.'))>=0)
	{
		const STR_String rightstring = identifiername.Right(identifiername.Length() -1 - pos);
		const STR_String leftstring = identifiername.Left(pos);
		CValue* tempresult = GetProperty(leftstring);
		if (tempresult)
		{
			result=tempresult->FindIdentifier(rightstring);
		} 
	} else
	{
		result = GetProperty(identifiername);
		if (result)
			return result->AddRef();
	}
	if (!result)
	{
		// warning here !!!
		result = new CErrorValue(identifiername+" not found");
	}
	return result;
}

#ifdef WITH_PYTHON

PyAttributeDef CValue::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("name",	CValue, pyattr_get_name),
	{ NULL }	//Sentinel
};

PyObject *CValue::pyattr_get_name(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	CValue * self = static_cast<CValue *> (self_v);
	return PyUnicode_From_STR_String(self->GetName());
}

/**
 * There are 2 reasons this could return NULL
 * - unsupported type.
 * - error converting (overflow).
 *
 * \param do_type_exception Use to skip raising an exception for unknown types.
 */
CValue *CValue::ConvertPythonToValue(PyObject *pyobj, const bool do_type_exception, const char *error_prefix)
{

	CValue *vallie;
	/* refcounting is broking here! - this crashes anyway, just store a python list for KX_GameObject */
#if 0
	if (PyList_Check(pyobj))
	{
		CListValue* listval = new CListValue();
		bool error = false;

		Py_ssize_t i;
		Py_ssize_t numitems = PyList_GET_SIZE(pyobj);
		for (i=0;i<numitems;i++)
		{
			PyObject *listitem = PyList_GET_ITEM(pyobj,i); /* borrowed ref */
			CValue* listitemval = ConvertPythonToValue(listitem, error_prefix);
			if (listitemval)
			{
				listval->Add(listitemval);
			} else
			{
				error = true;
			}
		}
		if (!error)
		{
			// jippie! could be converted
			vallie = listval;
		} else
		{
			// list could not be converted... bad luck
			listval->Release();
		}

	} else
#endif
	/* note: Boolean check should go before Int check [#34677] */
	if (PyBool_Check(pyobj))
	{
		vallie = new CBoolValue( (bool)PyLong_AsLongLong(pyobj) );
	} else
	if (PyFloat_Check(pyobj))
	{
		const double tval = PyFloat_AsDouble(pyobj);
		if (tval > (double)FLT_MAX || tval < (double)-FLT_MAX) {
			PyErr_Format(PyExc_OverflowError, "%soverflow converting from float, out of internal range", error_prefix);
			vallie = NULL;
		}
		else {
			vallie = new CFloatValue((float)tval);
		}
	} else
	if (PyLong_Check(pyobj))
	{
		vallie = new CIntValue( (cInt)PyLong_AsLongLong(pyobj) );
	} else
	if (PyUnicode_Check(pyobj))
	{
		vallie = new CStringValue(_PyUnicode_AsString(pyobj),"");
	} else
	if (PyObject_TypeCheck(pyobj, &CValue::Type)) /* Note, don't let these get assigned to GameObject props, must check elsewhere */
	{
		vallie = (static_cast<CValue *>(BGE_PROXY_REF(pyobj)))->AddRef();
	}
	else {
		if (do_type_exception) {
			/* return an error value from the caller */
			PyErr_Format(PyExc_TypeError, "%scould convert python value to a game engine property", error_prefix);
		}
		vallie = NULL;
	}
	return vallie;

}

PyObject *CValue::ConvertKeysToPython(void)
{
	if (m_pNamedPropertyArray)
	{
		PyObject *pylist= PyList_New(m_pNamedPropertyArray->size());
		Py_ssize_t i= 0;

		std::map<STR_String,CValue*>::iterator it;
		for (it= m_pNamedPropertyArray->begin(); (it != m_pNamedPropertyArray->end()); it++)
		{
			PyList_SET_ITEM(pylist, i++, PyUnicode_From_STR_String((*it).first));
		}

		return pylist;
	}
	else {
		return PyList_New(0);
	}
}

#endif // WITH_PYTHON


///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
/* These implementations were moved out of the header */

void CValue::SetOwnerExpression(class CExpression* expr)
{
	/* intentionally empty */
}

void CValue::SetColorOperator(VALUE_OPERATOR op)
{
	/* intentionally empty */
}
void CValue::SetValue(CValue* newval)
{ 
	// no one should get here
	assertd(newval->GetNumber() == 10121969);
}
