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
#include "MT_Vector3.h"
/*------------------------------
 * PyObjectPlus Type		-- Every class, even the abstract one should have a Type
------------------------------*/


PyTypeObject PyObjectPlus::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	NULL // no subtype
};


PyObjectPlus::~PyObjectPlus()
{
	if(m_proxy) {
		BGE_PROXY_REF(m_proxy)= NULL;
		Py_DECREF(m_proxy);			/* Remove own reference, python may still have 1 */
	}
//	assert(ob_refcnt==0);
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


PyObject * PyObjectPlus::py_base_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyTypeObject *base_type;
	PyObjectPlus_Proxy *base = NULL;

	if (!PyArg_ParseTuple(args, "O:Base PyObjectPlus", &base))
		return NULL;

	/* the 'base' PyObject may be subclassed (multiple times even)
	 * we need to find the first C++ defined class to check 'type'
	 * is a subclass of the base arguments type.
	 *
	 * This way we can share one tp_new function for every PyObjectPlus
	 *
	 * eg.
	 *
	 * # CustomOb is called 'type' in this C code
	 * class CustomOb(GameTypes.KX_GameObject):
	 *     pass
	 *
	 * # this calls py_base_new(...), the type of 'CustomOb' is checked to be a subclass of the 'cont.owner' type
	 * ob = CustomOb(cont.owner)
	 *
	 * */
	base_type= Py_TYPE(base);
	while(base_type && !BGE_PROXY_CHECK_TYPE(base_type))
		base_type= base_type->tp_base;

	if(base_type==NULL || !BGE_PROXY_CHECK_TYPE(base_type)) {
		PyErr_SetString(PyExc_TypeError, "can't subclass from a blender game type because the argument given is not a game class or subclass");
		return NULL;
	}

	/* use base_type rather then Py_TYPE(base) because we could alredy be subtyped */
	if(!PyType_IsSubtype(type, base_type)) {
		PyErr_Format(PyExc_TypeError, "can't subclass blender game type <%s> from <%s> because it is not a subclass", base_type->tp_name, type->tp_name);
		return NULL;
	}

	/* invalidate the existing base and return a new subclassed one,
	 * this is a bit dodgy in that it also attaches its self to the existing object
	 * which is not really 'correct' python OO but for our use its OK. */

	PyObjectPlus_Proxy *ret = (PyObjectPlus_Proxy *) type->tp_alloc(type, 0); /* starts with 1 ref, used for the return ref' */
	ret->ref= base->ref;
	base->ref= NULL;		/* invalidate! disallow further access */

	ret->py_owns= base->py_owns;

	ret->ref->m_proxy= NULL;

	/* 'base' may be free'd after this func finished but not necessarily
	 * there is no reference to the BGE data now so it will throw an error on access */
	Py_DECREF(base);

	ret->ref->m_proxy= (PyObject *)ret; /* no need to add a ref because one is added when creating. */
	Py_INCREF(ret); /* we return a new ref but m_proxy holds a ref so we need to add one */


	/* 'ret' will have 2 references.
	 * - One ref is needed because ret->ref->m_proxy holds a refcount to the current proxy.
	 * - Another is needed for returning the value.
	 *
	 * So we should be ok with 2 refs, but for some reason this crashes. so adding a new ref...
	 * */

	return (PyObject *)ret;
}

void PyObjectPlus::py_base_dealloc(PyObject *self)				// python wrapper
{
	PyObjectPlus *self_plus= BGE_PROXY_REF(self);
	if(self_plus) {
		if(BGE_PROXY_PYOWNS(self)) { /* Does python own this?, then delete it  */
			self_plus->m_proxy = NULL; /* Need this to stop ~PyObjectPlus from decrefing m_proxy otherwise its decref'd twice and py-debug crashes */
			delete self_plus;
		}

		BGE_PROXY_REF(self)= NULL; // not really needed
	}

#if 0
	/* is ok normally but not for subtyping, use tp_free instead. */
	PyObject_DEL( self );
#else
	Py_TYPE(self)->tp_free(self);
#endif
};

PyObjectPlus::PyObjectPlus() : SG_QList()				// constructor
{
	m_proxy= NULL;
};

/*------------------------------
 * PyObjectPlus Methods 	-- Every class, even the abstract one should have a Methods
------------------------------*/
PyMethodDef PyObjectPlus::Methods[] = {
  {NULL, NULL}		/* Sentinel */
};

#define attr_invalid (&(PyObjectPlus::Attributes[0]))
PyAttributeDef PyObjectPlus::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("invalid",		PyObjectPlus, pyattr_get_invalid),
	{NULL} //Sentinel
};



PyObject* PyObjectPlus::pyattr_get_invalid(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	return PyBool_FromLong(self_v ? 1:0);
}

/* note, this is called as a python 'getset, where the PyAttributeDef is the closure */
PyObject *PyObjectPlus::py_get_attrdef(PyObject *self_py, const PyAttributeDef *attrdef)
{
	void *self= (void *)(BGE_PROXY_REF(self_py));
	if(self==NULL) {
		if(attrdef == attr_invalid)
			Py_RETURN_TRUE; // dont bother running the function

		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return NULL;
	}


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
					PyList_SET_ITEM(resultlist,i,PyLong_FromSsize_t(*val));
					break;
				}
			case KX_PYATTRIBUTE_TYPE_SHORT:
				{
					short int *val = reinterpret_cast<short int*>(ptr);
					ptr += sizeof(short int);
					PyList_SET_ITEM(resultlist,i,PyLong_FromSsize_t(*val));
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
					PyList_SET_ITEM(resultlist,i,PyLong_FromSsize_t(*val));
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
				return PyLong_FromSsize_t(*val);
			}
		case KX_PYATTRIBUTE_TYPE_SHORT:
			{
				short int *val = reinterpret_cast<short int*>(ptr);
				return PyLong_FromSsize_t(*val);
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
				return PyLong_FromSsize_t(*val);
			}
		case KX_PYATTRIBUTE_TYPE_FLOAT:
			{
				float *val = reinterpret_cast<float*>(ptr);
				return PyFloat_FromDouble(*val);
			}
		case KX_PYATTRIBUTE_TYPE_VECTOR:
			{
				MT_Vector3 *val = reinterpret_cast<MT_Vector3*>(ptr);
#ifdef USE_MATHUTILS
				float fval[3]= {(*val)[0], (*val)[1], (*val)[2]};
				return newVectorObject(fval, 3, Py_NEW, NULL);
#else
				PyObject* resultlist = PyList_New(3);
				for (unsigned int i=0; i<3; i++)
				{
					PyList_SET_ITEM(resultlist,i,PyFloat_FromDouble((*val)[i]));
				}
				return resultlist;
#endif
			}
		case KX_PYATTRIBUTE_TYPE_STRING:
			{
				STR_String *val = reinterpret_cast<STR_String*>(ptr);
				return PyUnicode_FromString(*val);
			}
		default:
			return NULL;
		}
	}
}

/* note, this is called as a python getset */
int PyObjectPlus::py_set_attrdef(PyObject *self_py, PyObject *value, const PyAttributeDef *attrdef)
{
	void *self= (void *)(BGE_PROXY_REF(self_py));
	if(self==NULL) {
		PyErr_SetString(PyExc_SystemError, BGE_PROXY_ERROR_MSG);
		return PY_SET_ATTR_FAIL;
	}

	void *undoBuffer = NULL;
	void *sourceBuffer = NULL;
	size_t bufferSize = 0;
	
	char *ptr = reinterpret_cast<char*>(self)+attrdef->m_offset;
	if (attrdef->m_length > 1)
	{
		if (!PySequence_Check(value)) 
		{
			PyErr_Format(PyExc_TypeError, "expected a sequence for attribute \"%s\"", attrdef->m_name);
			return PY_SET_ATTR_FAIL;
		}
		if (PySequence_Size(value) != attrdef->m_length)
		{
			PyErr_Format(PyExc_TypeError, "incorrect number of elements in sequence for attribute \"%s\"", attrdef->m_name);
			return PY_SET_ATTR_FAIL;
		}
		switch (attrdef->m_type) 
		{
		case KX_PYATTRIBUTE_TYPE_FUNCTION:
			if (attrdef->m_setFunction == NULL) 
			{
				PyErr_Format(PyExc_AttributeError, "function attribute without function for attribute \"%s\", report to blender.org", attrdef->m_name);
				return PY_SET_ATTR_FAIL;
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
			return PY_SET_ATTR_FAIL;
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
					if (PyLong_Check(item)) 
					{
						*var = (PyLong_AsSsize_t(item) != 0);
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
					if (PyLong_Check(item)) 
					{
						long val = PyLong_AsSsize_t(item);
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
					if (PyLong_Check(item)) 
					{
						long val = PyLong_AsSsize_t(item);
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
				return PY_SET_ATTR_FAIL;
			}
		}
		if (undoBuffer)
			free(undoBuffer);
		return PY_SET_ATTR_SUCCESS;
	}
	else	// simple attribute value
	{
		if (attrdef->m_type == KX_PYATTRIBUTE_TYPE_FUNCTION)
		{
			if (attrdef->m_setFunction == NULL)
			{
				PyErr_Format(PyExc_AttributeError, "function attribute without function \"%s\", report to blender.org", attrdef->m_name);
				return PY_SET_ATTR_FAIL;
			}
			return (*attrdef->m_setFunction)(self, attrdef, value);
		}
		if (attrdef->m_checkFunction != NULL || attrdef->m_type == KX_PYATTRIBUTE_TYPE_VECTOR)
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
			case KX_PYATTRIBUTE_TYPE_VECTOR:
				bufferSize = sizeof(MT_Vector3);
				break;
			default:
				PyErr_Format(PyExc_AttributeError, "unknown type for attribute \"%s\", report to blender.org", attrdef->m_name);
				return PY_SET_ATTR_FAIL;
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
				if (PyLong_Check(value)) 
				{
					*var = (PyLong_AsSsize_t(value) != 0);
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
				if (PyLong_Check(value)) 
				{
					long val = PyLong_AsSsize_t(value);
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
				if (PyLong_Check(value)) 
				{
					long val = PyLong_AsSsize_t(value);
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
		case KX_PYATTRIBUTE_TYPE_VECTOR:
			{
				if (!PySequence_Check(value) || PySequence_Size(value) != 3) 
				{
					PyErr_Format(PyExc_TypeError, "expected a sequence of 3 floats for attribute \"%s\"", attrdef->m_name);
					return PY_SET_ATTR_FAIL;
				}
				MT_Vector3 *var = reinterpret_cast<MT_Vector3*>(ptr);
				for (int i=0; i<3; i++)
				{
					PyObject *item = PySequence_GetItem(value, i); /* new ref */
					// we can decrement the reference immediately, the reference count
					// is at least 1 because the item is part of an array
					Py_DECREF(item);
					double val = PyFloat_AsDouble(item);
					if (val == -1.0 && PyErr_Occurred())
					{
						PyErr_Format(PyExc_TypeError, "expected a sequence of 3 floats for attribute \"%s\"", attrdef->m_name);
						goto RESTORE_AND_ERROR;
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
						goto RESTORE_AND_ERROR;
					}
					(*var)[i] = (MT_Scalar)val;
				}
				break;
			}
		case KX_PYATTRIBUTE_TYPE_STRING:
			{
				STR_String *var = reinterpret_cast<STR_String*>(ptr);
				if (PyUnicode_Check(value)) 
				{
					Py_ssize_t val_len;
					char *val = _PyUnicode_AsStringAndSize(value, &val_len);
					if (attrdef->m_clamp)
					{
						if (val_len < attrdef->m_imin)
						{
							// can't increase the length of the string
							PyErr_Format(PyExc_ValueError, "string length too short for attribute \"%s\"", attrdef->m_name);
							goto FREE_AND_ERROR;
						}
						else if (val_len > attrdef->m_imax)
						{
							// trim the string
							char c = val[attrdef->m_imax];
							val[attrdef->m_imax] = 0;
							*var = val;
							val[attrdef->m_imax] = c;
							break;
						}
					} else if (val_len < attrdef->m_imin || val_len > attrdef->m_imax)
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

void PyDebugLine()
{
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

					printf("\t%s:%d\n", _PyUnicode_AsString(co_filename), (int)PyLong_AsSsize_t(f_lineno));

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

void PyObjectPlus::ShowDeprecationWarning_func(const char* old_way,const char* new_way)
{
	printf("Method %s is deprecated, please use %s instead.\n", old_way, new_way);
	PyDebugLine();
}

void PyObjectPlus::ClearDeprecationWarning()
{
	WarnLink *wlink_next;
	WarnLink *wlink = GetDeprecationWarningLinkFirst();
	
	while(wlink)
	{
		wlink->warn_done= false; /* no need to NULL the link, its cleared before adding to the list next time round */
		wlink_next= reinterpret_cast<WarnLink *>(wlink->link);
		wlink->link= NULL;
		wlink= wlink_next;
	}
	NullDeprecationWarning();
}

WarnLink*		m_base_wlink_first= NULL;
WarnLink*		m_base_wlink_last= NULL;

WarnLink*		PyObjectPlus::GetDeprecationWarningLinkFirst(void) {return m_base_wlink_first;}
WarnLink*		PyObjectPlus::GetDeprecationWarningLinkLast(void) {return m_base_wlink_last;}
void			PyObjectPlus::SetDeprecationWarningFirst(WarnLink* wlink) {m_base_wlink_first= wlink;}
void			PyObjectPlus::SetDeprecationWarningLinkLast(WarnLink* wlink) {m_base_wlink_last= wlink;}
void			PyObjectPlus::NullDeprecationWarning() {m_base_wlink_first= m_base_wlink_last= NULL;}

#endif //NO_EXP_PYTHON_EMBEDDING

