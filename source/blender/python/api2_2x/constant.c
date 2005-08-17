/* 
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * This is a new part of Blender.
 *
 * Contributor(s): Willian P. Germano, Joseph Gilbert
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include "constant.h" /*This must come first */

#include "gen_utils.h"

//------------------METHOD IMPLEMENTATIONS-----------------------------
//------------------------constant.items()
//Returns a list of key:value pairs like dict.items()
PyObject* constant_items(BPy_constant *self)
{
	return PyDict_Items(self->dict);
}
//------------------------constant.keys()
//Returns a list of keys like dict.keys()
PyObject* constant_keys(BPy_constant *self)
{
	return PyDict_Keys(self->dict);
}
//------------------------constant.values()
//Returns a list of values like dict.values()
PyObject* constant_values(BPy_constant *self)
{
	return PyDict_Values(self->dict);
}
//------------------ATTRIBUTE IMPLEMENTATION---------------------------
//------------------TYPE_OBECT IMPLEMENTATION--------------------------
//-----------------------(internal)
//Creates a new constant object
static PyObject *new_const(void) 
{				
	BPy_constant *constant;

	constant =	(BPy_constant *) PyObject_NEW(BPy_constant, &constant_Type);
	if(constant == NULL){
		return (EXPP_ReturnPyObjError(PyExc_MemoryError, 
			"couldn't create constant object"));
	}
	if((constant->dict = PyDict_New()) == NULL){
		return (EXPP_ReturnPyObjError(PyExc_MemoryError,
			"couldn't create constant object's dictionary"));
	}

	return EXPP_incr_ret((PyObject *)constant);
}
//------------------------tp_doc
//The __doc__ string for this object
static char BPy_constant_doc[] = "This is an internal subobject of armature\
designed to act as a Py_Bone dictionary.";

//------------------------tp_methods
//This contains a list of all methods the object contains
static PyMethodDef BPy_constant_methods[] = {
	{"items", (PyCFunction) constant_items, METH_NOARGS, 
		"() - Returns the key:value pairs from the dictionary"},
	{"keys", (PyCFunction) constant_keys, METH_NOARGS, 
		"() - Returns the keys the dictionary"},
	{"values", (PyCFunction) constant_values, METH_NOARGS, 
		"() - Returns the values from the dictionary"},
	{NULL}
};
//------------------------mp_length
static int constantLength(BPy_constant *self)
{
	return 0;
}
//------------------------mp_subscript
static PyObject *constantSubscript(BPy_constant *self, PyObject *key)
{
	if(self->dict) {
		PyObject *v = PyDict_GetItem(self->dict, key);
		if(v) {
			return EXPP_incr_ret(v);
		}
	}
	return NULL;
}
//------------------------mp_ass_subscript
static int constantAssSubscript(BPy_constant *self, PyObject *who, PyObject *cares)
{
	return 0; /* no user assignments allowed */
}
//------------------------tp_getattr
static PyObject *constant_getAttr(BPy_constant * self, char *name)
{
	if(self->dict) {
		PyObject *v;

		if(!strcmp(name, "__members__"))
			return PyDict_Keys(self->dict);

		v = PyDict_GetItemString(self->dict, name);
		if(v) {
			return EXPP_incr_ret(v); /* was a borrowed ref */
		}
		return (EXPP_ReturnPyObjError(PyExc_AttributeError,
						"attribute not found"));
	}
	return (EXPP_ReturnPyObjError(PyExc_RuntimeError,
					"constant object lacks a dictionary"));
}
//------------------------tp_repr
static PyObject *constant_repr(BPy_constant * self)
{
	PyObject *repr = PyObject_Repr(self->dict);
	return repr;
}
//------------------------tp_dealloc
static void constant_dealloc(BPy_constant * self)
{
	Py_DECREF(self->dict);
	PyObject_DEL(self);
}

//------------------TYPE_OBECT DEFINITION------------------------------
static PyMappingMethods constantAsMapping = {
	(inquiry) constantLength,					// mp_length 
	(binaryfunc) constantSubscript,				// mp_subscript
	(objobjargproc) constantAssSubscript,		// mp_ass_subscript
};

PyTypeObject constant_Type = {
	PyObject_HEAD_INIT(NULL)		//tp_head
	0,								//tp_internal
	"Constant",						//tp_name
	sizeof(BPy_constant),			//tp_basicsize
	0,								//tp_itemsize
	(destructor)constant_dealloc,	//tp_dealloc
	0,								//tp_print
	(getattrfunc)constant_getAttr,	//tp_getattr
	0,								//tp_setattr
	0,								//tp_compare
	(reprfunc) constant_repr,		//tp_repr
	0,								//tp_as_number
	0,								//tp_as_sequence
	&constantAsMapping,				//tp_as_mapping
	0,								//tp_hash
	0,								//tp_call
	0,								//tp_str
	0,								//tp_getattro
	0,								//tp_setattro
	0,								//tp_as_buffer
	Py_TPFLAGS_DEFAULT,				//tp_flags
	BPy_constant_doc,				//tp_doc
	0,								//tp_traverse
	0,								//tp_clear
	0,								//tp_richcompare
	0,								//tp_weaklistoffset
	0,								//tp_iter
	0,								//tp_iternext
	BPy_constant_methods,			//tp_methods
	0,								//tp_members
	0,								//tp_getset
	0,								//tp_base
	0,								//tp_dict
	0,								//tp_descr_get
	0,								//tp_descr_set
	0,								//tp_dictoffset
	0,								//tp_init
	0,								//tp_alloc
	0,								//tp_new
	0,								//tp_free
	0,								//tp_is_gc
	0,								//tp_bases
	0,								//tp_mro
	0,								//tp_cache
	0,								//tp_subclasses
	0,								//tp_weaklist
	0								//tp_del
};
//------------------VISIBLE PROTOTYPE IMPLEMENTATION-------------------
//Creates a default empty constant
PyObject *PyConstant_New(void)
{				
	return new_const();
}
//Inserts a key:value pair into the constant and then returns 0/1
int PyConstant_Insert(BPy_constant *self, char *name, PyObject *value)
{
	return PyDict_SetItemString(self->dict, name, value);
}
//This is a helper function for generating constants......
PyObject *PyConstant_NewInt(char *name, int value)
{
        PyObject *constant = PyConstant_New();
                
        PyConstant_Insert((BPy_constant*)constant, "name", PyString_FromString(name));
		PyConstant_Insert((BPy_constant*)constant, "value", PyInt_FromLong(value));
        return EXPP_incr_ret(constant);
}
//This is a helper function for generating constants......
PyObject *PyConstant_NewString(char *name, char *value)
{
        PyObject *constant = PyConstant_New();
       
        PyConstant_Insert((BPy_constant*)constant, "name", PyString_FromString(name));
		PyConstant_Insert((BPy_constant*)constant, "value", PyString_FromString(value));
        return EXPP_incr_ret(constant);
}
