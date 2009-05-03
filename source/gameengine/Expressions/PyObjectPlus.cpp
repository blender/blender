/**
 * $Id$
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef NO_EXP_PYTHON_EMBEDDING

/*------------------------------
 * PyObjectPlus cpp
 *
 * C++ library routines for Crawl 3.2
 *
 * Derived from work by
 * David Redish
 * graduate student
 * Computer Science Department 
 * Carnegie Mellon University (CMU)
 * Center for the Neural Basis of Cognition (CNBC) 
 * http://www.python.org/doc/PyCPP.html
 *
------------------------------*/
#include <MT_assert.h>
#include "stdlib.h"
#include "PyObjectPlus.h"
#include "STR_String.h"
/*------------------------------
 * PyObjectPlus Type		-- Every class, even the abstract one should have a Type
------------------------------*/


PyTypeObject PyObjectPlus::Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,				/*ob_size*/
#endif
	"PyObjectPlus",			/*tp_name*/
	sizeof(PyObjectPlus_Proxy),		/*tp_basicsize*/
	0,				/*tp_itemsize*/
	/* methods */
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,
	py_base_getattro,
	py_base_setattro,
	0,0,0,0,0,0,0,0,0,
	Methods
};


PyObjectPlus::~PyObjectPlus()
{
	if(m_proxy) {
		Py_DECREF(m_proxy);			/* Remove own reference, python may still have 1 */
		BGE_PROXY_REF(m_proxy)= NULL;
	}
//	assert(ob_refcnt==0);
}

void PyObjectPlus::py_base_dealloc(PyObject *self)				// python wrapper
{
	PyObjectPlus *self_plus= BGE_PROXY_REF(self);
	if(self_plus) {
		if(BGE_PROXY_PYOWNS(self)) { /* Does python own this?, then delete it  */
			delete self_plus;
		}
		
		BGE_PROXY_REF(self)= NULL; // not really needed
	}
	PyObject_DEL( self );
};

PyObjectPlus::PyObjectPlus(PyTypeObject *T) 				// constructor
{
	MT_assert(T != NULL);
	m_proxy= NULL;
};
  
/*------------------------------
 * PyObjectPlus Methods 	-- Every class, even the abstract one should have a Methods
------------------------------*/
PyMethodDef PyObjectPlus::Methods[] = {
  {"isA",		 (PyCFunction) sPyisA,			METH_O},
  {NULL, NULL}		/* Sentinel */
};

PyAttributeDef PyObjectPlus::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("invalid",		PyObjectPlus, pyattr_get_invalid),
	{NULL} //Sentinel
};

PyObject* PyObjectPlus::pyattr_get_invalid(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{	
	Py_RETURN_FALSE;
}

/*------------------------------
 * PyObjectPlus Parents		-- Every class, even the abstract one should have parents
------------------------------*/
PyParentObject PyObjectPlus::Parents[] = {&PyObjectPlus::Type, NULL};

/*------------------------------
 * PyObjectPlus attributes	-- attributes
------------------------------*/


/* This should be the entry in Type since it takes the C++ class from PyObjectPlus_Proxy */
PyObject *PyObjectPlus::py_base_getattro(PyObject * self, PyObject *attr)
{
	PyObjectPlus *self_plus= BGE_PROXY_REF(self);
	if(self_plus==NULL) {
		if(!strcmp("invalid", PyString_AsString(attr))) {
			Py_RETURN_TRUE;
		}
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	PyObject *ret= self_plus->py_getattro(attr);
	
	/* Attribute not found, was this a __dict__ lookup?, otherwise set an error if none is set */
	if(ret==NULL) {
		char *attr_str= PyString_AsString(attr);
		
		if (strcmp(attr_str, "__dict__")==0)
		{
			/* the error string will probably not
			 * be set but just incase clear it */
			PyErr_Clear(); 
			ret= self_plus->py_getattro_dict();
		}
		else if (!PyErr_Occurred()) {
			/* We looked for an attribute but it wasnt found
			 * since py_getattro didnt set the error, set it here */
			PyErr_Format(PyExc_AttributeError, "'%s' object has no attribute '%s'", self->ob_type->tp_name, attr_str);
		}
	}
	return ret;
}

/* This should be the entry in Type since it takes the C++ class from PyObjectPlus_Proxy */
int PyObjectPlus::py_base_setattro(PyObject *self, PyObject *attr, PyObject *value)
{
	PyObjectPlus *self_plus= BGE_PROXY_REF(self);
	if(self_plus==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return -1;
	}
	
	if (value==NULL)
		return self_plus->py_delattro(attr);
	
	return self_plus->py_setattro(attr, value); 
}

PyObject *PyObjectPlus::py_base_repr(PyObject *self)			// This should be the entry in Type.
{
	
	PyObjectPlus *self_plus= BGE_PROXY_REF(self);
	if(self_plus==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return NULL;
	}
	
	return self_plus->py_repr();  
}

PyObject *PyObjectPlus::py_getattro(PyObject* attr)
{
	PyObject *descr = PyDict_GetItem(Type.tp_dict, attr); \
	if (descr == NULL) {
		return NULL; /* py_base_getattro sets the error, this way we can avoid setting the error at many levels */
	} else {
		/* Copied from py_getattro_up */
		if (PyCObject_Check(descr)) {
			return py_get_attrdef((void *)this, (const PyAttributeDef*)PyCObject_AsVoidPtr(descr));
		} else if (descr->ob_type->tp_descr_get) {
			return PyCFunction_New(((PyMethodDescrObject *)descr)->d_method, this->m_proxy);
		} else {
			return NULL;
		}
		/* end py_getattro_up copy */
	}
}

PyObject* PyObjectPlus::py_getattro_dict() {
	return py_getattr_dict(NULL, Type.tp_dict);
}

int PyObjectPlus::py_delattro(PyObject* attr)
{
	PyErr_SetString(PyExc_AttributeError, "attribute cant be deleted");
	return 1;
}

int PyObjectPlus::py_setattro(PyObject *attr, PyObject* value)
{
	PyErr_SetString(PyExc_AttributeError, "attribute cant be set");
	return PY_SET_ATTR_MISSING;
}

PyObject *PyObjectPlus::py_get_attrdef(void *self, const PyAttributeDef *attrdef)
{
	if (attrdef->m_type == KX_PYATTRIBUTE_TYPE_DUMMY)
	{
		// fake attribute, ignore
		return NULL;
	}
	if (attrdef->m_type == KX_PYATTRIBUTE_TYPE_FUNCTION)
	{
		// the attribute has no field correspondance, handover processing to function.
		if (attrdef->m_getFunction == NULL)
			return NULL;
		return (*attrdef->m_getFunction)(self, attrdef);
	}
	char *ptr = reinterpret_cast<char*>(self)+attrdef->m_offset;
	if (attrdef->m_length > 1)
	{
		PyObject* resultlist = PyList_New(attrdef->m_length);
		for (unsigned int i=0; i<attrdef->m_length; i++)
		{
			switch (attrdef->m_type) {
			case KX_PYATTRIBUTE_TYPE_BOOL:
				{
					bool *val = reinterpret_cast<bool*>(ptr);
					ptr += sizeof(bool);
					PyList_SET_ITEM(resultlist,i,PyInt_FromLong(*val));
					break;
				}
			case KX_PYATTRIBUTE_TYPE_SHORT:
				{
					short int *val = reinterpret_cast<short int*>(ptr);
					ptr += sizeof(short int);
					PyList_SET_ITEM(resultlist,i,PyInt_FromLong(*val));
					break;
				}
			case KX_PYATTRIBUTE_TYPE_ENUM:
				// enum are like int, just make sure the field size is the same
				if (sizeof(int) != attrdef->m_size)
				{
					Py_DECREF(resultlist);
					return NULL;
				}
				// walkthrough
			case KX_PYATTRIBUTE_TYPE_INT:
				{
					int *val = reinterpret_cast<int*>(ptr);
					ptr += sizeof(int);
					PyList_SET_ITEM(resultlist,i,PyInt_FromLong(*val));
					break;
				}
			case KX_PYATTRIBUTE_TYPE_FLOAT:
				{
					float *val = reinterpret_cast<float*>(ptr);
					ptr += sizeof(float);
					PyList_SET_ITEM(resultlist,i,PyFloat_FromDouble(*val));
					break;
				}
			default:
				// no support for array of complex data
				Py_DECREF(resultlist);
				return NULL;
			}
		}
		return resultlist;
	}
	else
	{
		switch (attrdef->m_type) {
		case KX_PYATTRIBUTE_TYPE_BOOL:
			{
				bool *val = reinterpret_cast<bool*>(ptr);
				return PyInt_FromLong(*val);
			}
		case KX_PYATTRIBUTE_TYPE_SHORT:
			{
				short int *val = reinterpret_cast<short int*>(ptr);
				return PyInt_FromLong(*val);
			}
		case KX_PYATTRIBUTE_TYPE_ENUM:
			// enum are like int, just make sure the field size is the same
			if (sizeof(int) != attrdef->m_size)
			{
				return NULL;
			}
			// walkthrough
		case KX_PYATTRIBUTE_TYPE_INT:
			{
				int *val = reinterpret_cast<int*>(ptr);
				return PyInt_FromLong(*val);
			}
		case KX_PYATTRIBUTE_TYPE_FLOAT:
			{
				float *val = reinterpret_cast<float*>(ptr);
				return PyFloat_FromDouble(*val);
			}
		case KX_PYATTRIBUTE_TYPE_STRING:
			{
				STR_String *val = reinterpret_cast<STR_String*>(ptr);
				return PyString_FromString(*val);
			}
		default:
			return NULL;
		}
	}
}

int PyObjectPlus::py_set_attrdef(void *self, const PyAttributeDef *attrdef, PyObject *value)
{
	void *undoBuffer = NULL;
	void *sourceBuffer = NULL;
	size_t bufferSize = 0;
	
	char *ptr = reinterpret_cast<char*>(self)+attrdef->m_offset;
	if (attrdef->m_length > 1)
	{
		if (!PySequence_Check(value)) 
		{
			PyErr_Format(PyExc_TypeError, "expected a sequence for attribute \"%s\"", attrdef->m_name);
			return 1;
		}
		if (PySequence_Size(value) != attrdef->m_length)
		{
			PyErr_Format(PyExc_TypeError, "incorrect number of elements in sequence for attribute \"%s\"", attrdef->m_name);
			return 1;
		}
		switch (attrdef->m_type) 
		{
		case KX_PYATTRIBUTE_TYPE_FUNCTION:
			if (attrdef->m_setFunction == NULL) 
			{
				PyErr_Format(PyExc_AttributeError, "function attribute without function for attribute \"%s\", report to blender.org", attrdef->m_name);
				return 1;
			}
			return (*attrdef->m_setFunction)(self, attrdef, value);
		case KX_PYATTRIBUTE_TYPE_BOOL:
			bufferSize = sizeof(bool);
			break;
		case KX_PYATTRIBUTE_TYPE_SHORT:
			bufferSize = sizeof(short int);
			break;
		case KX_PYATTRIBUTE_TYPE_ENUM:
		case KX_PYATTRIBUTE_TYPE_INT:
			bufferSize = sizeof(int);
			break;
		case KX_PYATTRIBUTE_TYPE_FLOAT:
			bufferSize = sizeof(float);
			break;
		default:
			// should not happen
			PyErr_Format(PyExc_AttributeError, "Unsupported attribute type for attribute \"%s\", report to blender.org", attrdef->m_name);
			return 1;
		}
		// let's implement a smart undo method
		bufferSize *= attrdef->m_length;
		undoBuffer = malloc(bufferSize);
		sourceBuffer = ptr;
		if (undoBuffer)
		{
			memcpy(undoBuffer, sourceBuffer, bufferSize);
		}
		for (int i=0; i<attrdef->m_length; i++)
		{
			PyObject *item = PySequence_GetItem(value, i); /* new ref */
			// we can decrement the reference immediately, the reference count
			// is at least 1 because the item is part of an array
			Py_DECREF(item);
			switch (attrdef->m_type) 
			{
			case KX_PYATTRIBUTE_TYPE_BOOL:
				{
					bool *var = reinterpret_cast<bool*>(ptr);
					ptr += sizeof(bool);
					if (PyInt_Check(item)) 
					{
						*var = (PyInt_AsLong(item) != 0);
					} 
					else if (PyBool_Check(item))
					{
						*var = (item == Py_True);
					}
					else
					{
						PyErr_Format(PyExc_TypeError, "expected an integer or a bool for attribute \"%s\"", attrdef->m_name);
						goto UNDO_AND_ERROR;
					}
					break;
				}
			case KX_PYATTRIBUTE_TYPE_SHORT:
				{
					short int *var = reinterpret_cast<short int*>(ptr);
					ptr += sizeof(short int);
					if (PyInt_Check(item)) 
					{
						long val = PyInt_AsLong(item);
						if (attrdef->m_clamp)
						{
							if (val < attrdef->m_imin)
								val = attrdef->m_imin;
							else if (val > attrdef->m_imax)
								val = attrdef->m_imax;
						}
						else if (val < attrdef->m_imin || val > attrdef->m_imax)
						{
							PyErr_Format(PyExc_ValueError, "item value out of range for attribute \"%s\"", attrdef->m_name);
							goto UNDO_AND_ERROR;
						}
						*var = (short int)val;
					}
					else
					{
						PyErr_Format(PyExc_TypeError, "expected an integer for attribute \"%s\"", attrdef->m_name);
						goto UNDO_AND_ERROR;
					}
					break;
				}
			case KX_PYATTRIBUTE_TYPE_ENUM:
				// enum are equivalent to int, just make sure that the field size matches:
				if (sizeof(int) != attrdef->m_size)
				{
					PyErr_Format(PyExc_AttributeError, "Size check error for attribute, \"%s\", report to blender.org", attrdef->m_name);
					goto UNDO_AND_ERROR;
				}
				// walkthrough
			case KX_PYATTRIBUTE_TYPE_INT:
				{
					int *var = reinterpret_cast<int*>(ptr);
					ptr += sizeof(int);
					if (PyInt_Check(item)) 
					{
						long val = PyInt_AsLong(item);
						if (attrdef->m_clamp)
						{
							if (val < attrdef->m_imin)
								val = attrdef->m_imin;
							else if (val > attrdef->m_imax)
								val = attrdef->m_imax;
						}
						else if (val < attrdef->m_imin || val > attrdef->m_imax)
						{
							PyErr_Format(PyExc_ValueError, "item value out of range for attribute \"%s\"", attrdef->m_name);
							goto UNDO_AND_ERROR;
						}
						*var = (int)val;
					}
					else
					{
						PyErr_Format(PyExc_TypeError, "expected an integer for attribute \"%s\"", attrdef->m_name);
						goto UNDO_AND_ERROR;
					}
					break;
				}
			case KX_PYATTRIBUTE_TYPE_FLOAT:
				{
					float *var = reinterpret_cast<float*>(ptr);
					ptr += sizeof(float);
					double val = PyFloat_AsDouble(item);
					if (val == -1.0 && PyErr_Occurred())
					{
						PyErr_Format(PyExc_TypeError, "expected a float for attribute \"%s\"", attrdef->m_name);
						goto UNDO_AND_ERROR;
					}
					else if (attrdef->m_clamp) 
					{
						if (val < attrdef->m_fmin)
							val = attrdef->m_fmin;
						else if (val > attrdef->m_fmax)
							val = attrdef->m_fmax;
					}
					else if (val < attrdef->m_fmin || val > attrdef->m_fmax)
					{
						PyErr_Format(PyExc_ValueError, "item value out of range for attribute \"%s\"", attrdef->m_name);
						goto UNDO_AND_ERROR;
					}
					*var = (float)val;
					break;
				}
			default:
				// should not happen
				PyErr_Format(PyExc_AttributeError, "type check error for attribute \"%s\", report to blender.org", attrdef->m_name);
				goto UNDO_AND_ERROR;
			}
		}
		// no error, call check function if any
		if (attrdef->m_checkFunction != NULL)
		{
			if ((*attrdef->m_checkFunction)(self, attrdef) != 0)
			{
				// if the checing function didnt set an error then set a generic one here so we dont set an error with no exception
				if (PyErr_Occurred()==0)
					PyErr_Format(PyExc_AttributeError, "type check error for attribute \"%s\", reasion unknown", attrdef->m_name);
				
				// post check returned an error, restore values
			UNDO_AND_ERROR:
				if (undoBuffer)
				{
					memcpy(sourceBuffer, undoBuffer, bufferSize);
					free(undoBuffer);
				}
				return 1;
			}
		}
		if (undoBuffer)
			free(undoBuffer);
		return 0;
	}
	else	// simple attribute value
	{
		if (attrdef->m_type == KX_PYATTRIBUTE_TYPE_FUNCTION)
		{
			if (attrdef->m_setFunction == NULL)
			{
				PyErr_Format(PyExc_AttributeError, "function attribute without function \"%s\", report to blender.org", attrdef->m_name);
				return 1;
			}
			return (*attrdef->m_setFunction)(self, attrdef, value);
		}
		if (attrdef->m_checkFunction != NULL)
		{
			// post check function is provided, prepare undo buffer
			sourceBuffer = ptr;
			switch (attrdef->m_type) 
			{
			case KX_PYATTRIBUTE_TYPE_BOOL:
				bufferSize = sizeof(bool);
				break;
			case KX_PYATTRIBUTE_TYPE_SHORT:
				bufferSize = sizeof(short);
				break;
			case KX_PYATTRIBUTE_TYPE_ENUM:
			case KX_PYATTRIBUTE_TYPE_INT:
				bufferSize = sizeof(int);
				break;
			case KX_PYATTRIBUTE_TYPE_FLOAT:
				bufferSize = sizeof(float);
				break;
			case KX_PYATTRIBUTE_TYPE_STRING:
				sourceBuffer = reinterpret_cast<STR_String*>(ptr)->Ptr();
				if (sourceBuffer)
					bufferSize = strlen(reinterpret_cast<char*>(sourceBuffer))+1;
				break;
			default:
				PyErr_Format(PyExc_AttributeError, "unknown type for attribute \"%s\", report to blender.org", attrdef->m_name);
				return 1;
			}
			if (bufferSize)
			{
				undoBuffer = malloc(bufferSize);
				if (undoBuffer)
				{
					memcpy(undoBuffer, sourceBuffer, bufferSize);
				}
			}
		}
			
		switch (attrdef->m_type) 
		{
		case KX_PYATTRIBUTE_TYPE_BOOL:
			{
				bool *var = reinterpret_cast<bool*>(ptr);
				if (PyInt_Check(value)) 
				{
					*var = (PyInt_AsLong(value) != 0);
				} 
				else if (PyBool_Check(value))
				{
					*var = (value == Py_True);
				}
				else
				{
					PyErr_Format(PyExc_TypeError, "expected an integer or a bool for attribute \"%s\"", attrdef->m_name);
					goto FREE_AND_ERROR;
				}
				break;
			}
		case KX_PYATTRIBUTE_TYPE_SHORT:
			{
				short int *var = reinterpret_cast<short int*>(ptr);
				if (PyInt_Check(value)) 
				{
					long val = PyInt_AsLong(value);
					if (attrdef->m_clamp)
					{
						if (val < attrdef->m_imin)
							val = attrdef->m_imin;
						else if (val > attrdef->m_imax)
							val = attrdef->m_imax;
					}
					else if (val < attrdef->m_imin || val > attrdef->m_imax)
					{
						PyErr_Format(PyExc_ValueError, "value out of range for attribute \"%s\"", attrdef->m_name);
						goto FREE_AND_ERROR;
					}
					*var = (short int)val;
				}
				else
				{
					PyErr_Format(PyExc_TypeError, "expected an integer for attribute \"%s\"", attrdef->m_name);
					goto FREE_AND_ERROR;
				}
				break;
			}
		case KX_PYATTRIBUTE_TYPE_ENUM:
			// enum are equivalent to int, just make sure that the field size matches:
			if (sizeof(int) != attrdef->m_size)
			{
				PyErr_Format(PyExc_AttributeError, "attribute size check error for attribute \"%s\", report to blender.org", attrdef->m_name);
				goto FREE_AND_ERROR;
			}
			// walkthrough
		case KX_PYATTRIBUTE_TYPE_INT:
			{
				int *var = reinterpret_cast<int*>(ptr);
				if (PyInt_Check(value)) 
				{
					long val = PyInt_AsLong(value);
					if (attrdef->m_clamp)
					{
						if (val < attrdef->m_imin)
							val = attrdef->m_imin;
						else if (val > attrdef->m_imax)
							val = attrdef->m_imax;
					}
					else if (val < attrdef->m_imin || val > attrdef->m_imax)
					{
						PyErr_Format(PyExc_ValueError, "value out of range for attribute \"%s\"", attrdef->m_name);
						goto FREE_AND_ERROR;
					}
					*var = (int)val;
				}
				else
				{
					PyErr_Format(PyExc_TypeError, "expected an integer for attribute \"%s\"", attrdef->m_name);
					goto FREE_AND_ERROR;
				}
				break;
			}
		case KX_PYATTRIBUTE_TYPE_FLOAT:
			{
				float *var = reinterpret_cast<float*>(ptr);
				double val = PyFloat_AsDouble(value);
				if (val == -1.0 && PyErr_Occurred())
				{
					PyErr_Format(PyExc_TypeError, "expected a float for attribute \"%s\"", attrdef->m_name);
					goto FREE_AND_ERROR;
				}
				else if (attrdef->m_clamp)
				{
					if (val < attrdef->m_fmin)
						val = attrdef->m_fmin;
					else if (val > attrdef->m_fmax)
						val = attrdef->m_fmax;
				}
				else if (val < attrdef->m_fmin || val > attrdef->m_fmax)
				{
					PyErr_Format(PyExc_ValueError, "value out of range for attribute \"%s\"", attrdef->m_name);
					goto FREE_AND_ERROR;
				}
				*var = (float)val;
				break;
			}
		case KX_PYATTRIBUTE_TYPE_STRING:
			{
				STR_String *var = reinterpret_cast<STR_String*>(ptr);
				if (PyString_Check(value)) 
				{
					char *val = PyString_AsString(value);
					if (attrdef->m_clamp)
					{
						if (strlen(val) < attrdef->m_imin)
						{
							// can't increase the length of the string
							PyErr_Format(PyExc_ValueError, "string length too short for attribute \"%s\"", attrdef->m_name);
							goto FREE_AND_ERROR;
						}
						else if (strlen(val) > attrdef->m_imax)
						{
							// trim the string
							char c = val[attrdef->m_imax];
							val[attrdef->m_imax] = 0;
							*var = val;
							val[attrdef->m_imax] = c;
							break;
						}
					} else if (strlen(val) < attrdef->m_imin || strlen(val) > attrdef->m_imax)
					{
						PyErr_Format(PyExc_ValueError, "string length out of range for attribute \"%s\"", attrdef->m_name);
						goto FREE_AND_ERROR;
					}
					*var = val;
				}
				else
				{
					PyErr_Format(PyExc_TypeError, "expected a string for attribute \"%s\"", attrdef->m_name);
					goto FREE_AND_ERROR;
				}
				break;
			}
		default:
			// should not happen
			PyErr_Format(PyExc_AttributeError, "unknown type for attribute \"%s\", report to blender.org", attrdef->m_name);
			goto FREE_AND_ERROR;
		}
	}
	// check if post processing is needed
	if (attrdef->m_checkFunction != NULL)
	{
		if ((*attrdef->m_checkFunction)(self, attrdef) != 0)
		{
			// restore value
		RESTORE_AND_ERROR:
			if (undoBuffer)
			{
				if (attrdef->m_type == KX_PYATTRIBUTE_TYPE_STRING)
				{
					// special case for STR_String: restore the string
					STR_String *var = reinterpret_cast<STR_String*>(ptr);
					*var = reinterpret_cast<char*>(undoBuffer);
				}
				else
				{
					// other field type have direct values
					memcpy(ptr, undoBuffer, bufferSize);
				}
			}
		FREE_AND_ERROR:
			if (undoBuffer)
				free(undoBuffer);
			return 1;
		}
	}
	if (undoBuffer)
		free(undoBuffer);
	return 0;	
}



/*------------------------------
 * PyObjectPlus repr		-- representations
------------------------------*/
PyObject *PyObjectPlus::py_repr(void)
{
	PyErr_SetString(PyExc_SystemError, "Representation not overridden by object.");  
	return NULL;
}

/*------------------------------
 * PyObjectPlus isA		-- the isA functions
------------------------------*/
bool PyObjectPlus::isA(PyTypeObject *T)		// if called with a Type, use "typename"
{
	int i;
	PyParentObject  P;
	PyParentObject *Ps = GetParents();

	for (P = Ps[i=0]; P != NULL; P = Ps[i++])
		if (P==T)
			return true;

	return false;
}


bool PyObjectPlus::isA(const char *mytypename)		// check typename of each parent
{
	int i;
	PyParentObject  P;
	PyParentObject *Ps = GetParents();
  
	for (P = Ps[i=0]; P != NULL; P = Ps[i++])
		if (strcmp(P->tp_name, mytypename)==0)
			return true;

	return false;
}

PyObject *PyObjectPlus::PyisA(PyObject *value)		// Python wrapper for isA
{
	if (PyType_Check(value)) {
		return PyBool_FromLong(isA((PyTypeObject *)value));
	} else if (PyString_Check(value)) {
		return PyBool_FromLong(isA(PyString_AsString(value)));
	}
    PyErr_SetString(PyExc_TypeError, "object.isA(value): expected a type or a string");
    return NULL;	
}


void PyObjectPlus::ProcessReplica()
{
	/* Clear the proxy, will be created again if needed with GetProxy()
	 * otherwise the PyObject will point to the wrong reference */
	m_proxy= NULL;
}

/* Sometimes we might want to manually invalidate a BGE type even if
 * it hasnt been released by the BGE, say for example when an object
 * is removed from a scene, accessing it may cause problems.
 * 
 * In this case the current proxy is made invalid, disowned,
 * and will raise an error on access. However if python can get access
 * to this class again it will make a new proxy and work as expected.
 */
void PyObjectPlus::InvalidateProxy()		// check typename of each parent
{
	if(m_proxy) { 
		BGE_PROXY_REF(m_proxy)=NULL;
		Py_DECREF(m_proxy);
		m_proxy= NULL;
	}
}

/* Utility function called by the macro py_getattro_up()
 * for getting ob.__dict__() values from our PyObject
 * this is used by python for doing dir() on an object, so its good
 * if we return a list of attributes and methods.
 * 
 * Other then making dir() useful the value returned from __dict__() is not useful
 * since every value is a Py_None
 * */
PyObject *py_getattr_dict(PyObject *pydict, PyObject *tp_dict)
{
    if(pydict==NULL) { /* incase calling __dict__ on the parent of this object raised an error */
    	PyErr_Clear();
    	pydict = PyDict_New();
    }
	
	PyDict_Update(pydict, tp_dict);
	return pydict;
}



PyObject *PyObjectPlus::GetProxy_Ext(PyObjectPlus *self, PyTypeObject *tp)
{
	if (self->m_proxy==NULL)
	{
		self->m_proxy = reinterpret_cast<PyObject *>PyObject_NEW( PyObjectPlus_Proxy, tp);
		BGE_PROXY_PYOWNS(self->m_proxy) = false;
	}
	//PyObject_Print(self->m_proxy, stdout, 0);
	//printf("ref %d\n", self->m_proxy->ob_refcnt);
	
	BGE_PROXY_REF(self->m_proxy) = self; /* Its possible this was set to NULL, so set it back here */
	Py_INCREF(self->m_proxy); /* we own one, thos ones fore the return */
	return self->m_proxy;
}

PyObject *PyObjectPlus::NewProxy_Ext(PyObjectPlus *self, PyTypeObject *tp, bool py_owns)
{
	if (self->m_proxy)
	{
		if(py_owns)
		{	/* Free */
			BGE_PROXY_REF(self->m_proxy) = NULL;
			Py_DECREF(self->m_proxy);
			self->m_proxy= NULL;
		}
		else {
			Py_INCREF(self->m_proxy);
			return self->m_proxy;
		}
		
	}
	
	GetProxy_Ext(self, tp);
	if(py_owns) {
		BGE_PROXY_PYOWNS(self->m_proxy) = py_owns;
		Py_DECREF(self->m_proxy); /* could avoid thrashing here but for now its ok */
	}
	return self->m_proxy;
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////
/* deprecation warning management */
bool PyObjectPlus::m_ignore_deprecation_warnings(false);
void PyObjectPlus::SetDeprecationWarnings(bool ignoreDeprecationWarnings)
{
	m_ignore_deprecation_warnings = ignoreDeprecationWarnings;
}

void PyObjectPlus::ShowDeprecationWarning(const char* old_way,const char* new_way)
{
	if (!m_ignore_deprecation_warnings) {
		printf("Method %s is deprecated, please use %s instead.\n", old_way, new_way);
		
		// import sys; print '\t%s:%d' % (sys._getframe(0).f_code.co_filename, sys._getframe(0).f_lineno)
		
		PyObject *getframe, *frame;
		PyObject *f_lineno, *f_code, *co_filename;
		
		getframe = PySys_GetObject((char *)"_getframe"); // borrowed
		if (getframe) {
			frame = PyObject_CallObject(getframe, NULL);
			if (frame) {
				f_lineno= PyObject_GetAttrString(frame, "f_lineno");
				f_code= PyObject_GetAttrString(frame, "f_code");
				if (f_lineno && f_code) {
					co_filename= PyObject_GetAttrString(f_code, "co_filename");
					if (co_filename) {
						
						printf("\t%s:%d\n", PyString_AsString(co_filename), (int)PyInt_AsLong(f_lineno));
						
						Py_DECREF(f_lineno);
						Py_DECREF(f_code);
						Py_DECREF(co_filename);
						Py_DECREF(frame);
						return;
					}
				}
				
				Py_XDECREF(f_lineno);
				Py_XDECREF(f_code);
				Py_DECREF(frame);
			}
			
		}
		PyErr_Clear();
		printf("\tERROR - Could not access sys._getframe(0).f_lineno or sys._getframe().f_code.co_filename\n");
	}
}


#endif //NO_EXP_PYTHON_EMBEDDING

