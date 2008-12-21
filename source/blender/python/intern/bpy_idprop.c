/**
 * $Id: IDProp.c
 *
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
 * Contributor(s): Joseph Eagar, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 
 #include "DNA_ID.h"

#include "BKE_idprop.h"

#include "bpy_idprop.h"
#include "bpy_compat.h"

#include "MEM_guardedalloc.h"

#define BSTR_EQ(a, b)	(*(a) == *(b) && !strcmp(a, b))

static PyObject *EXPP_ReturnPyObjError( PyObject * type, char *error_msg )
{				/* same as above, just to change its name smoothly */
	PyErr_SetString( type, error_msg );
	return NULL;
}

static int EXPP_ReturnIntError( PyObject * type, char *error_msg )
{
	PyErr_SetString( type, error_msg );
	return -1;
}

 
/*returns NULL on success, error string on failure*/
static char *BPy_IDProperty_Map_ValidateAndCreate(char *name, IDProperty *group, PyObject *ob)
{
	IDProperty *prop = NULL;
	IDPropertyTemplate val = {0};
	
	if (PyFloat_Check(ob)) {
		val.d = PyFloat_AsDouble(ob);
		prop = IDP_New(IDP_DOUBLE, val, name);
	} else if (PyLong_Check(ob)) {
		val.i = (int) PyLong_AsLong(ob);
		prop = IDP_New(IDP_INT, val, name);
	} else if (PyUnicode_Check(ob)) {
		val.str = _PyUnicode_AsString(ob);
		prop = IDP_New(IDP_STRING, val, name);
	} else if (PySequence_Check(ob)) {
		PyObject *item;
		int i;
		
		/*validate sequence and derive type.
		we assume IDP_INT unless we hit a float
		number; then we assume it's */
		val.array.type = IDP_INT;
		val.array.len = PySequence_Length(ob);
		for (i=0; i<val.array.len; i++) {
			item = PySequence_GetItem(ob, i);
			if (PyFloat_Check(item)) val.array.type = IDP_DOUBLE;
			else if (!PyLong_Check(item)) return "only floats and ints are allowed in ID property arrays";
			Py_XDECREF(item);
		}
		
		prop = IDP_New(IDP_ARRAY, val, name);
		for (i=0; i<val.array.len; i++) {
			item = PySequence_GetItem(ob, i);
			if (val.array.type == IDP_INT) {
				item = PyNumber_Int(item);
				((int*)prop->data.pointer)[i] = (int)PyLong_AsLong(item);
			} else {
				item = PyNumber_Float(item);
				((double*)prop->data.pointer)[i] = (float)PyFloat_AsDouble(item);
			}
			Py_XDECREF(item);
		}
	} else if (PyMapping_Check(ob)) {
		PyObject *keys, *vals, *key, *pval;
		int i, len;
		/*yay! we get into recursive stuff now!*/
		keys = PyMapping_Keys(ob);
		vals = PyMapping_Values(ob);
		
		/*we allocate the group first; if we hit any invalid data,
		  we can delete it easily enough.*/
		prop = IDP_New(IDP_GROUP, val, name);
		len = PyMapping_Length(ob);
		for (i=0; i<len; i++) {
			key = PySequence_GetItem(keys, i);
			pval = PySequence_GetItem(vals, i);
			if (!PyUnicode_Check(key)) {
				IDP_FreeProperty(prop);
				MEM_freeN(prop);
				Py_XDECREF(keys);
				Py_XDECREF(vals);
				Py_XDECREF(key);
				Py_XDECREF(pval);
				return "invalid element in subgroup dict template!";
			}
			if (BPy_IDProperty_Map_ValidateAndCreate(_PyUnicode_AsString(key), prop, pval)) {
				IDP_FreeProperty(prop);
				MEM_freeN(prop);
				Py_XDECREF(keys);
				Py_XDECREF(vals);
				Py_XDECREF(key);
				Py_XDECREF(pval);
				return "invalid element in subgroup dict template!";
			}
			Py_XDECREF(key);
			Py_XDECREF(pval);
		}
		Py_XDECREF(keys);
		Py_XDECREF(vals);
	} else return "invalid property value";
	
	IDP_ReplaceInGroup(group, prop);
	return NULL;
}


static int BPy_IDGroup_Map_SetItem(IDProperty *prop, PyObject *key, PyObject *val)
{
	char *err;
	
	if (prop->type  != IDP_GROUP)
		return EXPP_ReturnIntError( PyExc_TypeError,
			"unsubscriptable object");
			
	if (!PyUnicode_Check(key))
		return EXPP_ReturnIntError( PyExc_TypeError,
		   "only strings are allowed as subgroup keys" );

	if (val == NULL) {
		IDProperty *pkey = IDP_GetPropertyFromGroup(prop, _PyUnicode_AsString(key));
		if (pkey) {
			IDP_RemFromGroup(prop, pkey);
			IDP_FreeProperty(pkey);
			MEM_freeN(pkey);
			return 0;
		} else return EXPP_ReturnIntError( PyExc_RuntimeError, "property not found in group" );
	}
	
	err = BPy_IDProperty_Map_ValidateAndCreate(_PyUnicode_AsString(key), prop, val);
	if (err) return EXPP_ReturnIntError( PyExc_RuntimeError, err );
	
	return 0;
}


PyObject *BPy_IDGroup_Update(IDProperty *prop, PyObject *value)
{
	PyObject *pkey, *pval;
	Py_ssize_t i=0;
	
	if (!PyDict_Check(value))
		return EXPP_ReturnPyObjError( PyExc_TypeError,
		   "expected an object derived from dict.");
		   
	while (PyDict_Next(value, &i, &pkey, &pval)) {
		BPy_IDGroup_Map_SetItem(prop, pkey, pval);
		if (PyErr_Occurred()) return NULL;
	}
	
	Py_RETURN_NONE;
}