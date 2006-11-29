/**
 * $Id: IDProp.c
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
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
 
#include "DNA_ID.h"

#include "BKE_idprop.h"

#include "IDProp.h"
#include "gen_utils.h"

#include "MEM_guardedalloc.h"

#define BSTR_EQ(a, b)	(*(a) == *(b) && !strcmp(a, b))

/*** Function to wrap ID properties ***/
PyObject *BPy_Wrap_IDProperty(ID *id, IDProperty *prop, IDProperty *parent);

extern PyTypeObject IDArray_Type;

/*********************** ID Property Main Wrapper Stuff ***************/
void IDProperty_dealloc(BPy_IDProperty *self)
{
	if (self->data_wrap) {
		Py_XDECREF(self->data_wrap);
	}

	/*if (self->destroy) {
		IDP_FreeProperty(self->prop);
		MEM_freeN(self->prop);
	}*/

	PyObject_DEL(self);
}

PyObject *IDProperty_repr(BPy_IDProperty *self)
{
	return Py_BuildValue("s", "(ID Property)");
}

extern PyTypeObject IDGroup_Type;

PyObject *BPy_IDProperty_getattr(BPy_IDProperty *self, char *name)
{
	if (BSTR_EQ(name, "data")) {
		switch (self->prop->type) {
			case IDP_STRING:
				return Py_BuildValue("s", self->prop->data.pointer);
			case IDP_INT:
				return Py_BuildValue("i", self->prop->data.val);
			case IDP_FLOAT:
				return Py_BuildValue("f", *(float*)(&self->prop->data.val));
			case IDP_GROUP:
				/*blegh*/
				if (self->data_wrap) {
					Py_XINCREF(self->data_wrap);
					return self->data_wrap;
				} else {
					BPy_IDProperty *group = PyObject_New(BPy_IDProperty, &IDGroup_Type);
					group->id = self->id;
					group->data_wrap = NULL;
					group->prop = self->prop;
					Py_XINCREF(group);
					self->data_wrap = (PyObject*) group;
					return (PyObject*) group;
				}
			case IDP_ARRAY:
				if (self->data_wrap) {
					Py_XINCREF(self->data_wrap);
					return self->data_wrap;
				} else {
					BPy_IDProperty *array = PyObject_New(BPy_IDProperty, &IDArray_Type);
					array->id = self->id;
					array->data_wrap = NULL;
					array->prop = self->prop;
					Py_XINCREF(array);
					self->data_wrap = (PyObject*) array;
					return (PyObject*) array;
				}
			case IDP_MATRIX:
			case IDP_VECTOR:
				break;
		}
	} else if (BSTR_EQ(name, "name")) {
		return Py_BuildValue("s", self->prop->name);
	} else if (BSTR_EQ(name, "type")) {
		return Py_BuildValue("i", self->prop->type);
	//} else if (BSTR_EQ(name, "object")) {
		/*hrm the idea is here is to return the wrapped ID object. how the hell
		  do I do that? eek! */
	} else if (BSTR_EQ(name, "__members__"))
		return Py_BuildValue("[s, s, s]", "data", "name", "type");

	return NULL;
}

int BPy_IDProperty_setattr(BPy_IDProperty *self, char *name, PyObject *val)
{
	if (BSTR_EQ(name, "type"))
		return EXPP_ReturnIntError(PyExc_TypeError, "attempt to set read-only attribute!");
	else if (BSTR_EQ(name, "name")) {
		char *st;
		if (!PyString_Check(val))
			return EXPP_ReturnIntError(PyExc_TypeError, "expected a string!");

		st = PyString_AsString(val);
		if (strlen(st) >= MAX_IDPROP_NAME)
			return EXPP_ReturnIntError(PyExc_TypeError, "string length cannot exceed 31 characters!");

		strcpy(self->prop->name, st);
		return 0;
	} else if (BSTR_EQ(name, "data")) {
		switch (self->prop->type) {
			case IDP_STRING:
			{
				char *st;
				if (!PyString_Check(val))
					return EXPP_ReturnIntError(PyExc_TypeError, "expected a string!");

				st = PyString_AsString(val);
				IDP_ResizeArray(self->prop, strlen(st)+1);
				strcpy(self->prop->data.pointer, st);
				return 0;
			}

			case IDP_INT:
			{
				int ival;
				if (!PyNumber_Check(val))
					return EXPP_ReturnIntError(PyExc_TypeError, "expected an int!");
				val = PyNumber_Int(val);
				if (!val)
					return EXPP_ReturnIntError(PyExc_TypeError, "expected an int!");
				ival = (int) PyInt_AsLong(val);
				self->prop->data.val = ival;
				Py_XDECREF(val);
				break;
			}
			case IDP_FLOAT:
			{
				float fval;
				if (!PyNumber_Check(val))
					return EXPP_ReturnIntError(PyExc_TypeError, "expected a float!");
				val = PyNumber_Float(val);
				if (!val)
					return EXPP_ReturnIntError(PyExc_TypeError, "expected a float!");
				fval = (float) PyFloat_AsDouble(val);
				*(float*)&self->prop->data.val = fval;
				Py_XDECREF(val);
				break;
			}

			default:
				return EXPP_ReturnIntError(PyExc_AttributeError, "attempt to set read-only attribute!");
		}
		return 0;
	}

	return EXPP_ReturnIntError(PyExc_TypeError, "invalid attribute!");
}

int BPy_IDProperty_Map_Len(BPy_IDProperty *self)
{
	if (self->prop->type != IDP_GROUP)
		return EXPP_ReturnIntError( PyExc_TypeError,
			"len() of unsized object");
			
	return self->prop->len;
}

PyObject *BPy_IDProperty_Map_GetItem(BPy_IDProperty *self, PyObject *item)
{
	IDProperty *loop;
	char *st;
	
	if (self->prop->type  != IDP_GROUP)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"unsubscriptable object");
			
	if (!PyString_Check(item)) 
		return EXPP_ReturnPyObjError( PyExc_TypeError,
			"only strings are allowed as keys of ID properties");
	
	st = PyString_AsString(item);
	for (loop=self->prop->data.group.first; loop; loop=loop->next) {
		if (BSTR_EQ(loop->name, st)) return BPy_Wrap_IDProperty(self->id, loop, self->prop);
	}
	return EXPP_ReturnPyObjError( PyExc_KeyError,
		"key not in subgroup dict");
}

/*returns NULL on success, error string on failure*/
char *BPy_IDProperty_Map_ValidateAndCreate(char *name, IDProperty *group, PyObject *ob)
{
	IDProperty *prop = NULL;
	IDPropertyTemplate val;
	
	if (PyFloat_Check(ob)) {
		val.f = (float) PyFloat_AsDouble(ob);
		prop = IDP_New(IDP_FLOAT, val, name);
	} else if (PyInt_Check(ob)) {
		val.i = (int) PyInt_AsLong(ob);
		prop = IDP_New(IDP_INT, val, name);
	} else if (PyString_Check(ob)) {
		val.str = PyString_AsString(ob);
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
			if (PyFloat_Check(item)) val.array.type = IDP_FLOAT;
			else if (!PyInt_Check(item)) return "only floats and ints are allowed in ID property arrays";
			Py_XDECREF(item);
		}
		
		prop = IDP_New(IDP_ARRAY, val, name);
		for (i=0; i<val.array.len; i++) {
			item = PySequence_GetItem(ob, i);
			Py_XDECREF(item);
			if (val.array.type == IDP_INT) {
				item = PyNumber_Int(item);
				((int*)prop->data.pointer)[i] = (int)PyInt_AsLong(item);
			} else {
				item = PyNumber_Float(item);
				((float*)prop->data.pointer)[i] = (float)PyFloat_AsDouble(item);
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
			if (!PyString_Check(key)) {
				IDP_FreeProperty(prop);
				MEM_freeN(prop);
				Py_XDECREF(key);
				Py_XDECREF(pval);
				return "invalid element in subgroup dict template!";
			}
			if (BPy_IDProperty_Map_ValidateAndCreate(PyString_AsString(key), prop, pval)) {
				IDP_FreeProperty(prop);
				MEM_freeN(prop);
				Py_XDECREF(key);
				Py_XDECREF(pval);
				return "invalid element in subgroup dict template!";
			}
			Py_XDECREF(key);
			Py_XDECREF(pval);
		}
		Py_XDECREF(keys);
		Py_XDECREF(vals);
	}
	
	if (IDP_GetPropertyFromGroup(group, prop->name) == NULL) {
		IDP_RemFromGroup(group, IDP_GetPropertyFromGroup(group, prop->name));
	}

	IDP_AddToGroup(group, prop);
	return NULL;
}

int BPy_IDProperty_Map_SetItem(BPy_IDProperty *self, PyObject *key, PyObject *val)
{
	char *err;
	
	if (self->prop->type  != IDP_GROUP)
		return EXPP_ReturnIntError( PyExc_TypeError,
			"unsubscriptable object");
			
	if (!PyString_Check(key))
		return EXPP_ReturnIntError( PyExc_TypeError,
		   "only strings are allowed as subgroup keys" );
	
	err = BPy_IDProperty_Map_ValidateAndCreate(PyString_AsString(key), self->prop, val);
	if (err) return EXPP_ReturnIntError( PyExc_RuntimeError, err );
	
	return 0;
}

PyMappingMethods BPy_IDProperty_Mapping = {
	(inquiry)BPy_IDProperty_Map_Len, 			/*inquiry mp_length */
	(binaryfunc)BPy_IDProperty_Map_GetItem,		/*binaryfunc mp_subscript */
	(objobjargproc)BPy_IDProperty_Map_SetItem,	/*objobjargproc mp_ass_subscript */
};

PyTypeObject IDProperty_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender IDProperty",           /* char *tp_name; */
	sizeof( BPy_IDProperty ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) IDProperty_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	(getattrfunc) BPy_IDProperty_getattr,     /* getattrfunc tp_getattr; */
	(setattrfunc) BPy_IDProperty_setattr,     /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) IDProperty_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	&BPy_IDProperty_Mapping,     /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */
};

/*********** Main wrapping function *******/
PyObject *BPy_Wrap_IDProperty(ID *id, IDProperty *prop, IDProperty *parent)
{
	BPy_IDProperty *wrap = PyObject_New(BPy_IDProperty, &IDProperty_Type);
	wrap->prop = prop;
	wrap->parent = parent;
	wrap->id = id;
	wrap->data_wrap = NULL;
	//wrap->destroy = 0;
	return (PyObject*) wrap;
}


/********Array Wrapper********/

void IDArray_dealloc(void *self)
{
	PyObject_DEL(self);
}

PyObject *IDArray_repr(BPy_IDArray *self)
{
	return Py_BuildValue("s", "(ID Array)");
}

PyObject *BPy_IDArray_getattr(BPy_IDArray *self, char *name)
{
	if (BSTR_EQ(name, "len")) return Py_BuildValue("i", self->prop->len);
	else if (BSTR_EQ(name, "type")) return Py_BuildValue("i", self->prop->subtype);
	else return NULL;
}

int BPy_IDArray_setattr(BPy_IDArray *self, PyObject *val, char *name)
{
	return -1;
}

int BPy_IDArray_Len(BPy_IDArray *self)
{
	return self->prop->len;
}

PyObject *BPy_IDArray_GetItem(BPy_IDArray *self, int index)
{
	if (index < 0 || index >= self->prop->len)
		return EXPP_ReturnPyObjError( PyExc_IndexError,
				"index out of range!");

	switch (self->prop->subtype) {
		case IDP_FLOAT:
			return Py_BuildValue("f", ((float*)self->prop->data.pointer)[index]);
			break;
		case IDP_INT:
			return Py_BuildValue("i", ((int*)self->prop->data.pointer)[index]);
			break;
		/*case IDP_LISTARRAY:
			return BPy_Wrap_IDProperty(self->id, ((IDProperty**)self->prop->data.array)[index]);
			break;*/
	}
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"invalid/corrupt array type!");
}

int BPy_IDArray_SetItem(BPy_IDArray *self, int index, PyObject *val)
{
	int i;
	float f;

	if (index < 0 || index >= self->prop->len)
		return EXPP_ReturnIntError( PyExc_RuntimeError,
				"index out of range!");

	switch (self->prop->subtype) {
		case IDP_FLOAT:
			if (!PyNumber_Check(val)) return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a float");
			val = PyNumber_Float(val);
			if (!val) return EXPP_ReturnIntError( PyExc_TypeError,
				"expected a float");

			f = (float) PyFloat_AsDouble(val);
			((float*)self->prop->data.pointer)[index] = f;
			Py_XDECREF(val);
			break;
		case IDP_INT:
			if (!PyNumber_Check(val)) return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int");
			val = PyNumber_Int(val);
			if (!val) return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an int");

			i = (int) PyInt_AsLong(val);
			((int*)self->prop->data.pointer)[index] = i;
			Py_XDECREF(val);
			break;
		/*case IDP_LISTARRAY:
			if (!PyObject_TypeCheck(val, &IDProperty_Type))
				return EXPP_ReturnIntError( PyExc_TypeError,
				"expected an IDProperty");

			if (((IDProperty**)self->prop->data.array)[index]) {
				IDP_FreeProperty(((IDProperty**)self->prop->data.array)[index]);
				MEM_freeN(((IDProperty**)self->prop->data.array)[index]);
			}
			((IDProperty**)self->prop->data.array)[index] = ((BPy_IDProperty*);
			break;*/
	}
	return 0;
}

static PySequenceMethods BPy_IDArray_Seq = {
	(inquiry) BPy_IDArray_Len,			/* inquiry sq_length */
	0,									/* binaryfunc sq_concat */
	0,									/* intargfunc sq_repeat */
	(intargfunc)BPy_IDArray_GetItem,	/* intargfunc sq_item */
	0,									/* intintargfunc sq_slice */
	(intobjargproc)BPy_IDArray_SetItem,	/* intobjargproc sq_ass_item */
	0,									/* intintobjargproc sq_ass_slice */
	0,									/* objobjproc sq_contains */
				/* Added in release 2.0 */
	0,									/* binaryfunc sq_inplace_concat */
	0,									/* intargfunc sq_inplace_repeat */
};

PyTypeObject IDArray_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender IDArray",           /* char *tp_name; */
	sizeof( BPy_IDArray ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) IDArray_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	( getattrfunc ) BPy_IDArray_getattr,     /* getattrfunc tp_getattr; */
	( setattrfunc ) BPy_IDArray_setattr,     /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) IDArray_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&BPy_IDArray_Seq,   			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,                       /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
	/* For PyObject_IS_GC */
	NULL,                       /* inquiry tp_is_gc;  */
	NULL,                       /* PyObject *tp_bases; */
	/* method resolution order */
	NULL,                       /* PyObject *tp_mro;  */
	NULL,                       /* PyObject *tp_cache; */
	NULL,                       /* PyObject *tp_subclasses; */
	NULL,                       /* PyObject *tp_weaklist; */
	NULL
};

/*********** ID Property Group iterator ********/
void IDGroup_Iter_dealloc(void *self)
{
	PyObject_DEL(self);
}

PyObject *IDGroup_Iter_repr(BPy_IDGroup_Iter *self)
{
	return Py_BuildValue("s", "(ID Property Group)");
}

PyObject *BPy_IDGroup_Iter_getattr(BPy_IDGroup_Iter *self, char *name)
{
	return Py_BuildValue("(s)", "None!! Not implemented!!");
}

PyObject *BPy_IDGroup_Iter_setattr(BPy_IDGroup_Iter *self, PyObject *val, char *name)
{
	return 0;
}

PyObject *BPy_Group_Iter_Next(BPy_IDGroup_Iter *self)
{
	IDProperty *cur=NULL;

	if (self->cur) {
		cur = self->cur;
		self->cur = self->cur->next;
		return BPy_Wrap_IDProperty(self->group->id, cur, self->group->prop);
	} else {
		return EXPP_ReturnPyObjError( PyExc_StopIteration,
				"iterator at end" );
	}
}

PyTypeObject IDGroup_Iter_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender IDGroup_Iter",           /* char *tp_name; */
	sizeof( BPy_IDGroup_Iter ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) IDGroup_Iter_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	(getattrfunc) BPy_IDGroup_Iter_getattr,     /* getattrfunc tp_getattr; */
	(setattrfunc) BPy_IDGroup_Iter_setattr,     /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) IDGroup_Iter_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,	        			/* PySequenceMethods *tp_as_sequence; */
	NULL,                       /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	(iternextfunc) BPy_Group_Iter_Next, /* iternextfunc tp_iternext; */
};

/*********** ID Property Group wrapper **********/
PyObject *BPy_IDGroup_NewProperty(BPy_IDProperty *self, PyObject *args);
PyObject *BPy_IDGroup_DeleteProperty(BPy_IDProperty *self, PyObject *args);

static PyMethodDef BPy_IDGroup_methods[] = {
	/* name, method, flags, doc */
	{"newProperty", ( PyCFunction ) BPy_IDGroup_NewProperty, METH_VARARGS,
	 "Create a new ID property and attach to group."},
	 {"deleteProperty", ( PyCFunction ) BPy_IDGroup_DeleteProperty, METH_VARARGS,
	  "Delete an ID property.  Takes either a string of the id property to be deleted,\n \
or a reference to the ID property itself as an argument"},
	{NULL},
};

void IDGroup_dealloc(void *self)
{
	PyObject_DEL(self);
}

PyObject *IDGroup_repr(BPy_IDProperty *self)
{
	return Py_BuildValue("s", "(ID Property Group)");
}

PyObject *BPy_IDGroup_getattr(BPy_IDProperty *self, char *name)
{
	if (BSTR_EQ(name, "__members__")) return Py_BuildValue("[]");
	else return Py_FindMethod( BPy_IDGroup_methods, ( PyObject * ) self, name );;
}

PyObject *BPy_IDGroup_setattr(BPy_IDProperty *self, PyObject *val, char *name)
{
	return 0;
}

PyObject *BPy_IDGroup_SpawnIterator(BPy_IDProperty *self)
{
	BPy_IDGroup_Iter *iter = PyObject_New(BPy_IDGroup_Iter, &IDGroup_Iter_Type);
	iter->group = self;
	iter->cur = self->prop->data.group.first;
	Py_XINCREF(iter);
	return (PyObject*) iter;
}

#define Default_Return "expected a string or int, a string plus variable number of additional arguments"
PyObject *BPy_IDGroup_NewProperty(BPy_IDProperty *self, PyObject *args)
{
	IDProperty *prop = NULL;
	IDPropertyTemplate val = {0};
	PyObject *pyob, *pyprop=NULL;
	char *name;
	int nargs;
	long type=0;

	/*the arguments required differs depending on the ID type.  so we read
	  the first argument first before doing anything else. */
	nargs = PySequence_Size(args);
	if (nargs < 2) {
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				Default_Return);
	}
	pyob = PyTuple_GET_ITEM(args, 0);

	/*we got a string instead of a type number as argument.*/
	if (PyString_Check(pyob)) {
		char *st = PyString_AsString(pyob);
		if (BSTR_EQ(st, "String")) type = IDP_STRING;
		else if (BSTR_EQ(st, "Group")) type = IDP_GROUP;
		else if (BSTR_EQ(st, "Int")) type = IDP_INT;
		else if (BSTR_EQ(st, "Float")) type = IDP_FLOAT;
		else if (BSTR_EQ(st, "Array")) type = IDP_ARRAY;
		else return EXPP_ReturnPyObjError( PyExc_TypeError, "invalid id property type!");
	} else {
		if (!PyNumber_Check(pyob)) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,	Default_Return);
		}
		pyob = PyNumber_Int(pyob);
		if (pyob == NULL) {
			return EXPP_ReturnPyObjError( PyExc_TypeError,	Default_Return);
		}
		type = PyInt_AsLong(pyob);
		Py_XDECREF(pyob);
	}

	pyob = PyTuple_GET_ITEM(args, 1);
	if (!PyString_Check(pyob))
		return EXPP_ReturnPyObjError( PyExc_TypeError,	Default_Return);

	name = PyString_AsString(pyob);
	//printf("name: %p %s\n", name, name);
	//printf("group name: %s\n", self->prop->name);
	switch (type) {
		case IDP_STRING:
		{
			if (nargs > 3) {
				return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string and optionally a string");
			}
			if (nargs == 3) {
				val.str = PyString_AsString(PyTuple_GET_ITEM(args, 2));
			} else val.str = NULL;

			prop = IDP_New(IDP_STRING, val, name);
			break;
		}
		case IDP_GROUP:
			if (nargs != 2) {
				return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, and a string");
			}
			prop = IDP_New(IDP_GROUP, val, name);
			break;
		case IDP_INT:
			if (nargs != 2 && nargs != 3)
				return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string and optionally an int");
			val.i = 0;
			if (nargs == 3) {
				pyob = PyTuple_GET_ITEM(args, 2);
				if (!PyNumber_Check(pyob))
					return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string and optionally an int");
				pyob = PyNumber_Int(pyob);
				if (pyob == NULL)
					return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string and optionally an int");
				val.i = (int) PyInt_AsLong(pyob);
				Py_XDECREF(pyob);
			}
			prop = IDP_New(IDP_INT, val, name);
			break;
		case IDP_FLOAT:
			if (nargs != 2 && nargs != 3)
				return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string and optionally an int");
			val.i = 0;
			if (nargs == 3) {
				pyob = PyTuple_GET_ITEM(args, 2);
				if (!PyNumber_Check(pyob))
					return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string and optionally an int");
				pyob = PyNumber_Float(pyob);
				if (pyob == NULL)
					return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string and optionally an int");
				val.f = (int) PyFloat_AS_DOUBLE(pyob);
				Py_XDECREF(pyob);
			}
			prop = IDP_New(IDP_FLOAT, val, name);
			break;
		case IDP_ARRAY:
		{
			int arrtype=0;
			if (nargs != 4 && !PyNumber_Check(PyTuple_GET_ITEM(args, 2))
			     && (!PyNumber_Check(PyTuple_GET_ITEM(args, 3)) || !PyString_Check(PyTuple_GET_ITEM(args, 3))))
				return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string, an int or a string, and an int");

			pyob = PyTuple_GET_ITEM(args, 2);
			if (PyNumber_Check(pyob)) {
				pyob = PyNumber_Int(pyob);
				if (pyob == NULL)
					return EXPP_ReturnPyObjError( PyExc_TypeError,
					"expected a string or an int, a string, an int or a string, and an int");

				arrtype = (int)PyInt_AsLong(pyob);
				Py_XDECREF(pyob);
				if (arrtype != IDP_FLOAT && arrtype != IDP_INT)
					EXPP_ReturnPyObjError( PyExc_TypeError, "invalid array type constant");
			} else {
				char *st = PyString_AsString(pyob);
				if (BSTR_EQ(st, "Float")) arrtype = IDP_FLOAT;
				else if (BSTR_EQ(st, "Int")) arrtype = IDP_INT;
				else return EXPP_ReturnPyObjError( PyExc_TypeError, "invalid array type");
			}

			if (arrtype == 0) return NULL;

			val.array.type = arrtype;
			pyob = PyNumber_Int(PyTuple_GET_ITEM(args, 3));
			if (pyob == NULL)
				return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a string or an int, a string, an int or a string, and an int");

			val.array.len = (int)PyInt_AsLong(pyob);

			if (val.array.len <= 0)
				return EXPP_ReturnPyObjError( PyExc_TypeError,
				"array len must be greater then zero!");

			prop = IDP_New(IDP_ARRAY, val, name);
			if (!prop) return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"error creating array!");
			break;
		}
		default:
			return EXPP_ReturnPyObjError( PyExc_TypeError,
				"invalid id property type");
	}

	if (!IDP_AddToGroup(self->prop, prop)) {
		IDP_FreeProperty(prop);
		MEM_freeN(prop);
		return EXPP_ReturnPyObjError( PyExc_RuntimeError,
				"property name already exists in group");
	}
	pyprop = BPy_Wrap_IDProperty(self->id, prop, self->prop);
	//Py_XINCREF(pyprop);
	return pyprop;
}

#undef Default_Return

PyObject *BPy_IDGroup_DeleteProperty(BPy_IDProperty *self, PyObject *args)
{
	IDProperty *loop;
	PyObject *ob;
	char *name;
	int nargs;

	nargs = PySequence_Size(args);
	if (nargs != 1)
		return EXPP_ReturnPyObjError( PyExc_TypeError,
				"expected a string or IDProperty object");

	ob = PyTuple_GET_ITEM(args, 0);
	if (PyString_Check(ob)) {
		name = PyString_AsString(ob);
		for (loop=self->prop->data.group.first; loop; loop=loop->next) {
			if (BSTR_EQ(name, loop->name)) {
				IDP_RemFromGroup(self->prop, loop);
				IDP_FreeProperty(loop);
				MEM_freeN(loop);
				Py_INCREF( Py_None );
				return Py_None;
			}
		}
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"IDProperty not in group!");

	} else if (PyObject_TypeCheck(ob, &IDProperty_Type)) {
		for (loop=self->prop->data.group.first; loop; loop=loop->next) {
			if (loop == ((BPy_IDProperty*)ob)->prop) {
				IDP_RemFromGroup(self->prop, loop);
				IDP_FreeProperty(loop);
				MEM_freeN(loop);
				Py_INCREF( Py_None );
				return Py_None;
			}
		}
		return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"IDProperty not in group!");
	}

	return EXPP_ReturnPyObjError( PyExc_AttributeError,
				"expected a string or IDProperty object");
}

int BPy_IDGroup_Len(BPy_IDArray *self)
{
	return self->prop->len;
}

PyObject *BPy_IDGroup_GetItem(BPy_IDProperty *self, int index)
{
	IDProperty *prop;
	int i;

	for (prop=self->prop->data.group.first, i=0; prop; prop=prop->next, i++) {
		if (i == index) return BPy_Wrap_IDProperty(self->id, prop, self->prop);
	}

	return EXPP_ReturnPyObjError( PyExc_IndexError,
					"index out of range!");
}

static PySequenceMethods BPy_IDGroup_Seq = {
	(inquiry) BPy_IDGroup_Len,			/* inquiry sq_length */
	0,									/* binaryfunc sq_concat */
	0,									/* intargfunc sq_repeat */
	(intargfunc)BPy_IDGroup_GetItem,	/* intargfunc sq_item */
	0,									/* intintargfunc sq_slice */
	0,									/* intobjargproc sq_ass_item */
	0,									/* intintobjargproc sq_ass_slice */
	0,									/* objobjproc sq_contains */
				/* Added in release 2.0 */
	0,									/* binaryfunc sq_inplace_concat */
	0,									/* intargfunc sq_inplace_repeat */
};

PyTypeObject IDGroup_Type = {
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
	/*  For printing, in format "<module>.<name>" */
	"Blender IDGroup",           /* char *tp_name; */
	sizeof( BPy_IDProperty ),       /* int tp_basicsize; */
	0,                          /* tp_itemsize;  For allocation */

	/* Methods to implement standard operations */

	( destructor ) IDGroup_dealloc,/* destructor tp_dealloc; */
	NULL,                       /* printfunc tp_print; */
	( getattrfunc ) BPy_IDGroup_getattr, /* getattrfunc tp_getattr; */
	( setattrfunc ) BPy_IDGroup_setattr, /* setattrfunc tp_setattr; */
	NULL,                       /* cmpfunc tp_compare; */
	( reprfunc ) IDGroup_repr,     /* reprfunc tp_repr; */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	&BPy_IDGroup_Seq,   			/* PySequenceMethods *tp_as_sequence; */
	&BPy_IDProperty_Mapping,      /* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,                       /* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL,                       /* getattrofunc tp_getattro; */
	NULL,                       /* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT,         /* long tp_flags; */

	NULL,                       /*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	NULL,                       /* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	( getiterfunc ) BPy_IDGroup_SpawnIterator,  /* getiterfunc tp_iter; */
	NULL,   					/* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	BPy_IDGroup_methods,        /* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,       /* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	NULL,                       /* newfunc tp_new; */
	/*  Low-level free-memory routine */
	NULL,                       /* freefunc tp_free;  */
};

void IDProp_Init_Types(void)
{
	PyType_Ready( &IDProperty_Type );
	PyType_Ready( &IDGroup_Type );
	PyType_Ready( &IDGroup_Iter_Type );
	PyType_Ready( &IDArray_Type );
}
