/**
 * $Id$
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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "bpy_rna.h"
#include "bpy_compat.h"
//#include "blendef.h"
#include "BLI_dynstr.h"

#include "MEM_guardedalloc.h"
#include "BKE_global.h" /* evil G.* */

/* floats bigger then this are displayed as inf in the docstrings */
#define MAXFLOAT_DOC 10000000

static int pyrna_struct_compare( BPy_StructRNA * a, BPy_StructRNA * b )
{
	return (a->ptr.data==b->ptr.data) ? 0 : -1;
}

static int pyrna_prop_compare( BPy_PropertyRNA * a, BPy_PropertyRNA * b )
{
	return (a->prop==b->prop && a->ptr.data==b->ptr.data ) ? 0 : -1;
}


/* For some reason python3 needs these :/ */
static PyObject *pyrna_struct_richcmp(BPy_StructRNA * a, BPy_StructRNA * b, int op)
{
	int cmp_result= -1; /* assume false */
	if (BPy_StructRNA_Check(a) && BPy_StructRNA_Check(b)) {
		cmp_result= pyrna_struct_compare(a, b);
	}

	return Py_CmpToRich(op, cmp_result);
}

static PyObject *pyrna_prop_richcmp(BPy_PropertyRNA * a, BPy_PropertyRNA * b, int op)
{
	int cmp_result= -1; /* assume false */
	if (BPy_PropertyRNA_Check(a) && BPy_PropertyRNA_Check(b)) {
		cmp_result= pyrna_prop_compare(a, b);
	}

	return Py_CmpToRich(op, cmp_result);
}

/*----------------------repr--------------------------------------------*/
static PyObject *pyrna_struct_repr( BPy_StructRNA * self )
{
	return PyUnicode_FromFormat( "[BPy_StructRNA \"%s\"]", RNA_struct_identifier(&self->ptr));
}

static PyObject *pyrna_prop_repr( BPy_PropertyRNA * self )
{
	return PyUnicode_FromFormat( "[BPy_PropertyRNA \"%s\" -> \"%s\" ]", RNA_struct_identifier(&self->ptr), RNA_property_identifier(&self->ptr, self->prop) );
}

static long pyrna_struct_hash( BPy_StructRNA * self )
{
	return (long)self->ptr.data;
}

/* use our own dealloc so we can free a property if we use one */
static void pyrna_struct_dealloc( BPy_StructRNA * self )
{
	/* Note!! for some weired reason calling PyObject_DEL() directly crashes blender! */
	if (self->freeptr && self->ptr.data) {
		IDP_FreeProperty(self->ptr.data);
		MEM_freeN(self->ptr.data);
		self->ptr.data= NULL;
	}

	((PyObject *)self)->ob_type->tp_free(self);
	return;
}

static char *pyrna_enum_as_string(PointerRNA *ptr, PropertyRNA *prop)
{
	const EnumPropertyItem *item;
	int totitem, i;

	DynStr *dynstr= BLI_dynstr_new();
	char *cstring;

	RNA_property_enum_items(ptr, prop, &item, &totitem);

	for (i=0; i<totitem; i++) {
		
		BLI_dynstr_appendf(dynstr, i?", '%s'":"'%s'", item[i].identifier);
	}
	
	cstring = BLI_dynstr_get_cstring(dynstr);
	BLI_dynstr_free(dynstr);
	return cstring;
}

PyObject * pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop)
{
	PyObject *ret;
	int type = RNA_property_type(ptr, prop);
	int len = RNA_property_array_length(ptr, prop);

	if (len > 0) {
		/* resolve the array from a new pytype */
		return pyrna_prop_CreatePyObject(ptr, prop);
	}
	
	/* see if we can coorce into a python type - PropertyType */
	switch (type) {
	case PROP_BOOLEAN:
		ret = PyBool_FromLong( RNA_property_boolean_get(ptr, prop) );
		break;
	case PROP_INT:
		ret = PyLong_FromSize_t( (size_t)RNA_property_int_get(ptr, prop) );
		break;
	case PROP_FLOAT:
		ret = PyFloat_FromDouble( RNA_property_float_get(ptr, prop) );
		break;
	case PROP_STRING:
	{
		char *buf;
		buf = RNA_property_string_get_alloc(ptr, prop, NULL, -1);
		ret = PyUnicode_FromString( buf );
		MEM_freeN(buf);
		break;
	}
	case PROP_ENUM:
	{
		const char *identifier;
		int val = RNA_property_enum_get(ptr, prop);
		
		if (RNA_property_enum_identifier(ptr, prop, val, &identifier)) {
			ret = PyUnicode_FromString( identifier );
		} else {
			PyErr_Format(PyExc_AttributeError, "RNA Error: Current value \"%d\" matches no enum", val);
			ret = NULL;
		}

		break;
	}
	case PROP_POINTER:
	{
		PointerRNA newptr;
		newptr= RNA_property_pointer_get(ptr, prop);
		if (newptr.data) {
			ret = pyrna_struct_CreatePyObject(&newptr);
		} else {
			ret = Py_None;
			Py_INCREF(ret);
		}
		break;
	}
	case PROP_COLLECTION:
		ret = pyrna_prop_CreatePyObject(ptr, prop);
		break;
	default:
		PyErr_Format(PyExc_AttributeError, "RNA Error: unknown type \"%d\" (pyrna_prop_to_py)", type);
		ret = NULL;
		break;
	}
	
	return ret;
}


int pyrna_py_to_prop(PointerRNA *ptr, PropertyRNA *prop, PyObject *value)
{
	int type = RNA_property_type(ptr, prop);
	int len = RNA_property_array_length(ptr, prop);
	
	if (len > 0) {
		PyObject *item;
		int i;
		
		if (!PySequence_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "expected a python sequence type assigned to an RNA array.");
			return -1;
		}
		
		if ((int)PySequence_Length(value) != len) {
			PyErr_SetString(PyExc_AttributeError, "python sequence length did not match the RNA array.");
			return -1;
		}
		
		/* for arrays we have a limited number of types */
		switch (type) {
		case PROP_BOOLEAN:
		{
			int *param_arr = MEM_mallocN(sizeof(char) * len, "pyrna bool array");
			
			/* collect the variables before assigning, incase one of them is incorrect */
			for (i=0; i<len; i++) {
				item = PySequence_GetItem(value, i);
				param_arr[i] = PyObject_IsTrue( item );
				Py_DECREF(item);
				
				if (param_arr[i] < 0) {
					MEM_freeN(param_arr);
					PyErr_SetString(PyExc_AttributeError, "one or more of the values in the sequence is not a boolean");
					return -1;
				}
			}
			
			RNA_property_boolean_set_array(ptr, prop, param_arr);
			
			MEM_freeN(param_arr);
			break;
		}
		case PROP_INT:
		{
			int *param_arr = MEM_mallocN(sizeof(int) * len, "pyrna int array");
			
			/* collect the variables before assigning, incase one of them is incorrect */
			for (i=0; i<len; i++) {
				item = PySequence_GetItem(value, i);
				param_arr[i] = (int)PyLong_AsSsize_t(item); /* deal with any errors later */
				Py_DECREF(item);
			}
			
			if (PyErr_Occurred()) {
				MEM_freeN(param_arr);
				PyErr_SetString(PyExc_AttributeError, "one or more of the values in the sequence could not be used as an int");
				return -1;
			}
			
			RNA_property_int_set_array(ptr, prop, param_arr);
			
			MEM_freeN(param_arr);
			break;
		}
		case PROP_FLOAT:
		{
			float *param_arr = MEM_mallocN(sizeof(float) * len, "pyrna float array");
			
			/* collect the variables before assigning, incase one of them is incorrect */
			for (i=0; i<len; i++) {
				item = PySequence_GetItem(value, i);
				param_arr[i] = (float)PyFloat_AsDouble(item); /* deal with any errors later */
				Py_DECREF(item);
			}
			
			if (PyErr_Occurred()) {
				MEM_freeN(param_arr);
				PyErr_SetString(PyExc_AttributeError, "one or more of the values in the sequence could not be used as a float");
				return -1;
			}
			
			RNA_property_float_set_array(ptr, prop, param_arr);
			
			MEM_freeN(param_arr);
			break;
		}
		}
	} else {
		/* Normal Property (not an array) */
		
		/* see if we can coorce into a python type - PropertyType */
		switch (type) {
		case PROP_BOOLEAN:
		{
			int param = PyObject_IsTrue( value );
			
			if( param < 0 ) {
				PyErr_SetString(PyExc_TypeError, "expected True/False or 0/1");
				return -1;
			} else {
				RNA_property_boolean_set(ptr, prop, param);
			}
			break;
		}
		case PROP_INT:
		{
			int param = PyLong_AsSsize_t(value);
			if (PyErr_Occurred()) {
				PyErr_SetString(PyExc_TypeError, "expected an int type");
				return -1;
			} else {
				RNA_property_int_set(ptr, prop, param);
			}
			break;
		}
		case PROP_FLOAT:
		{
			float param = PyFloat_AsDouble(value);
			if (PyErr_Occurred()) {
				PyErr_SetString(PyExc_TypeError, "expected a float type");
				return -1;
			} else {
				RNA_property_float_set(ptr, prop, param);
			}
			break;
		}
		case PROP_STRING:
		{
			char *param = _PyUnicode_AsString(value);
			
			if (param==NULL) {
				PyErr_SetString(PyExc_TypeError, "expected a string type");
				return -1;
			} else {
				RNA_property_string_set(ptr, prop, param);
			}
			break;
		}
		case PROP_ENUM:
		{
			char *param = _PyUnicode_AsString(value);
			
			if (param==NULL) {
				char *enum_str= pyrna_enum_as_string(ptr, prop);
				PyErr_Format(PyExc_TypeError, "expected a string enum type in (%s)", enum_str);
				MEM_freeN(enum_str);
				return -1;
			} else {
				int val;
				if (RNA_property_enum_value(ptr, prop, param, &val)) {
					RNA_property_enum_set(ptr, prop, val);
				} else {
					char *enum_str= pyrna_enum_as_string(ptr, prop);
					PyErr_Format(PyExc_AttributeError, "enum \"%s\" not found in (%s)", param, enum_str);
					MEM_freeN(enum_str);
					return -1;
				}
			}
			
			break;
		}
		case PROP_POINTER:
		{
			PyErr_SetString(PyExc_AttributeError, "cant assign pointers yet");
			return -1;
			break;
		}
		case PROP_COLLECTION:
			PyErr_SetString(PyExc_AttributeError, "cant assign to collections");
			return -1;
			break;
		default:
			PyErr_SetString(PyExc_AttributeError, "unknown property type (pyrna_py_to_prop)");
			return -1;
			break;
		}
	}
	
	return 0;
}


static PyObject * pyrna_prop_to_py_index(PointerRNA *ptr, PropertyRNA *prop, int index)
{
	PyObject *ret;
	int type = RNA_property_type(ptr, prop);
	
	/* see if we can coorce into a python type - PropertyType */
	switch (type) {
	case PROP_BOOLEAN:
		ret = PyBool_FromLong( RNA_property_boolean_get_index(ptr, prop, index) );
		break;
	case PROP_INT:
		ret = PyLong_FromSize_t( (size_t)RNA_property_int_get_index(ptr, prop, index) );
		break;
	case PROP_FLOAT:
		ret = PyFloat_FromDouble( RNA_property_float_get_index(ptr, prop, index) );
		break;
	default:
		PyErr_SetString(PyExc_AttributeError, "not an array type");
		ret = NULL;
		break;
	}
	
	return ret;
}

static int pyrna_py_to_prop_index(PointerRNA *ptr, PropertyRNA *prop, int index, PyObject *value)
{
	int ret = 0;
	int type = RNA_property_type(ptr, prop);
	
	/* see if we can coorce into a python type - PropertyType */
	switch (type) {
	case PROP_BOOLEAN:
	{
		int param = PyObject_IsTrue( value );
		
		if( param < 0 ) {
			PyErr_SetString(PyExc_TypeError, "expected True/False or 0/1");
			ret = -1;
		} else {
			RNA_property_boolean_set_index(ptr, prop, index, param);
		}
		break;
	}
	case PROP_INT:
	{
		int param = PyLong_AsSsize_t(value);
		if (PyErr_Occurred()) {
			PyErr_SetString(PyExc_TypeError, "expected an int type");
			ret = -1;
		} else {
			RNA_property_int_set_index(ptr, prop, index, param);
		}
		break;
	}
	case PROP_FLOAT:
	{
		float param = PyFloat_AsDouble(value);
		if (PyErr_Occurred()) {
			PyErr_SetString(PyExc_TypeError, "expected a float type");
			ret = -1;
		} else {
			RNA_property_float_set_index(ptr, prop, index, param);
		}
		break;
	}
	default:
		PyErr_SetString(PyExc_AttributeError, "not an array type");
		ret = -1;
		break;
	}
	
	return ret;
}

//---------------sequence-------------------------------------------
static Py_ssize_t pyrna_prop_len( BPy_PropertyRNA * self )
{
	Py_ssize_t len;
	
	if (RNA_property_type(&self->ptr, self->prop) == PROP_COLLECTION) {
		len = RNA_property_collection_length(&self->ptr, self->prop);
	} else {
		len = RNA_property_array_length(&self->ptr, self->prop);
		
		if (len==0) { /* not an array*/
			PyErr_SetString(PyExc_AttributeError, "len() only available for collection RNA types");
			return -1;
		}
	}
	
	return len;
}

static PyObject *pyrna_prop_subscript( BPy_PropertyRNA * self, PyObject *key )
{
	PyObject *ret;
	PointerRNA newptr;
	int keynum = 0;
	char *keyname = NULL;
	
	if (PyUnicode_Check(key)) {
		keyname = _PyUnicode_AsString(key);
	} else if (PyLong_Check(key)) {
		keynum = PyLong_AsSsize_t(key);
	} else {
		PyErr_SetString(PyExc_AttributeError, "invalid key, key must be a string or an int");
		return NULL;
	}
	
	if (RNA_property_type(&self->ptr, self->prop) == PROP_COLLECTION) {
		int ok;
		if (keyname)	ok = RNA_property_collection_lookup_string(&self->ptr, self->prop, keyname, &newptr);
		else			ok = RNA_property_collection_lookup_int(&self->ptr, self->prop, keynum, &newptr);
		
		if (ok) {
			ret = pyrna_struct_CreatePyObject(&newptr);
		} else {
			PyErr_SetString(PyExc_AttributeError, "out of range");
			ret = NULL;
		}
		
	} else if (keyname) {
		PyErr_SetString(PyExc_AttributeError, "string keys are only supported for collections");
		ret = NULL;
	} else {
		int len = RNA_property_array_length(&self->ptr, self->prop);
		
		if (len==0) { /* not an array*/
			PyErr_Format(PyExc_AttributeError, "not an array or collection %d", keynum);
			ret = NULL;
		}
		
		if (keynum >= len){
			PyErr_SetString(PyExc_AttributeError, "index out of range");
			ret = NULL;
		} else { /* not an array*/
			ret = pyrna_prop_to_py_index(&self->ptr, self->prop, keynum);
		}
	}
	
	return ret;
}


static int pyrna_prop_assign_subscript( BPy_PropertyRNA * self, PyObject *key, PyObject *value )
{
	int ret = 0;
	int keynum = 0;
	char *keyname = NULL;
	
	if (!RNA_property_editable(&self->ptr, self->prop)) {
		PyErr_Format( PyExc_AttributeError, "Attribute \"%s\" from \"%s\" is read-only", RNA_property_identifier(&self->ptr, self->prop), RNA_struct_identifier(&self->ptr) );
		return -1;
	}
	
	if (PyUnicode_Check(key)) {
		keyname = _PyUnicode_AsString(key);
	} else if (PyLong_Check(key)) {
		keynum = PyLong_AsSsize_t(key);
	} else {
		PyErr_SetString(PyExc_AttributeError, "invalid key, key must be a string or an int");
		return -1;
	}
	
	if (RNA_property_type(&self->ptr, self->prop) == PROP_COLLECTION) {
		PyErr_SetString(PyExc_AttributeError, "assignment is not supported for collections (yet)");
		ret = -1;
	} else if (keyname) {
		PyErr_SetString(PyExc_AttributeError, "string keys are only supported for collections");
		ret = -1;
	} else {
		int len = RNA_property_array_length(&self->ptr, self->prop);
		
		if (len==0) { /* not an array*/
			PyErr_Format(PyExc_AttributeError, "not an array or collection %d", keynum);
			ret = -1;
		}
		
		if (keynum >= len){
			PyErr_SetString(PyExc_AttributeError, "index out of range");
			ret = -1;
		} else {
			ret = pyrna_py_to_prop_index(&self->ptr, self->prop, keynum, value);
		}
	}
	
	return ret;
}



static PyMappingMethods pyrna_prop_as_mapping = {
	( inquiry ) pyrna_prop_len,	/* mp_length */
	( binaryfunc ) pyrna_prop_subscript,	/* mp_subscript */
	( objobjargproc ) pyrna_prop_assign_subscript,	/* mp_ass_subscript */
};

//---------------getattr--------------------------------------------
static PyObject *pyrna_struct_getattro( BPy_StructRNA * self, PyObject *pyname )
{
	char *name = _PyUnicode_AsString(pyname);
	PyObject *ret;
	PropertyRNA *prop;
	
	/* Include this incase this instance is a subtype of a python class
	 * In these instances we may want to return a function or variable provided by the subtype
	 * */
	ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
	if (ret)	return ret;
	else		PyErr_Clear();
	/* done with subtypes */

	prop = RNA_struct_find_property(&self->ptr, name);
	
	if (prop) {
		ret = pyrna_prop_to_py(&self->ptr, prop);
	}
	else if (strcmp(name, "__dict__")==0) { /* Not quite correct, adding this so dir() gives good feedback */
		PropertyRNA *iterprop, *nameprop;
		CollectionPropertyIterator iter;
		char name[256], *nameptr;

		iterprop= RNA_struct_iterator_property(&self->ptr);
		RNA_property_collection_begin(&self->ptr, iterprop, &iter);

		ret = PyDict_New();
		for(; iter.valid; RNA_property_collection_next(&iter)) {
			if(iter.ptr.data && (nameprop = RNA_struct_name_property(&iter.ptr))) {
				nameptr= RNA_property_string_get_alloc(&iter.ptr, nameprop, name, sizeof(name));
				PyDict_SetItemString(ret, nameptr, Py_None);
				
				if ((char *)&name != nameptr)
					MEM_freeN(nameptr);
			}
		}
		
		RNA_property_collection_end(&iter);
	}
	else {
		PyErr_Format( PyExc_AttributeError, "Attribute \"%s\" not found", name);
		ret = NULL;
	}
	
	return ret;
}

//--------------- setattr-------------------------------------------
static int pyrna_struct_setattro( BPy_StructRNA * self, PyObject *pyname, PyObject * value )
{
	char *name = _PyUnicode_AsString(pyname);
	PropertyRNA *prop = RNA_struct_find_property(&self->ptr, name);
	
	if (prop==NULL) {
		PyErr_Format( PyExc_AttributeError, "Attribute \"%s\" not found", name);
		return -1;
	}		
	
	if (!RNA_property_editable(&self->ptr, prop)) {
		PyErr_Format( PyExc_AttributeError, "Attribute \"%s\" from \"%s\" is read-only", RNA_property_identifier(&self->ptr, prop), RNA_struct_identifier(&self->ptr) );
		return -1;
	}
		
	/* pyrna_py_to_prop sets its own exceptions */
	return pyrna_py_to_prop(&self->ptr, prop, value);
}

PyObject *pyrna_prop_keys(BPy_PropertyRNA *self)
{
	PyObject *ret;
	if (RNA_property_type(&self->ptr, self->prop) != PROP_COLLECTION) {
		PyErr_SetString( PyExc_TypeError, "keys() is only valid for collection types" );
		ret = NULL;
	} else {
		PyObject *item;
		CollectionPropertyIterator iter;
		PropertyRNA *nameprop;
		char name[256], *nameptr;

		ret = PyList_New(0);
		
		RNA_property_collection_begin(&self->ptr, self->prop, &iter);
		for(; iter.valid; RNA_property_collection_next(&iter)) {
			if(iter.ptr.data && (nameprop = RNA_struct_name_property(&iter.ptr))) {
				nameptr= RNA_property_string_get_alloc(&iter.ptr, nameprop, name, sizeof(name));				
				
				/* add to python list */
				item = PyUnicode_FromString( nameptr );
				PyList_Append(ret, item);
				Py_DECREF(item);
				/* done */
				
				if ((char *)&name != nameptr)
					MEM_freeN(nameptr);
			}
		}
		RNA_property_collection_end(&iter);
	}
	
	return ret;
}

PyObject *pyrna_prop_items(BPy_PropertyRNA *self)
{
	PyObject *ret;
	if (RNA_property_type(&self->ptr, self->prop) != PROP_COLLECTION) {
		PyErr_SetString( PyExc_TypeError, "items() is only valid for collection types" );
		ret = NULL;
	} else {
		PyObject *item;
		CollectionPropertyIterator iter;
		PropertyRNA *nameprop;
		char name[256], *nameptr;

		ret = PyList_New(0);
		
		RNA_property_collection_begin(&self->ptr, self->prop, &iter);
		for(; iter.valid; RNA_property_collection_next(&iter)) {
			if(iter.ptr.data && (nameprop = RNA_struct_name_property(&iter.ptr))) {
				nameptr= RNA_property_string_get_alloc(&iter.ptr, nameprop, name, sizeof(name));
				
				/* add to python list */
				item = Py_BuildValue("(NN)", PyUnicode_FromString( nameptr ), pyrna_struct_CreatePyObject(&iter.ptr));
				PyList_Append(ret, item);
				Py_DECREF(item);
				/* done */
				
				if ((char *)&name != nameptr)
					MEM_freeN(nameptr);
			}
		}
		RNA_property_collection_end(&iter);
	}
	
	return ret;
}


PyObject *pyrna_prop_values(BPy_PropertyRNA *self)
{
	PyObject *ret;
	if (RNA_property_type(&self->ptr, self->prop) != PROP_COLLECTION) {
		PyErr_SetString( PyExc_TypeError, "values() is only valid for collection types" );
		ret = NULL;
	} else {
		PyObject *item;
		CollectionPropertyIterator iter;
		PropertyRNA *nameprop;
		
		ret = PyList_New(0);
		
		RNA_property_collection_begin(&self->ptr, self->prop, &iter);
		for(; iter.valid; RNA_property_collection_next(&iter)) {
			if(iter.ptr.data && (nameprop = RNA_struct_name_property(&iter.ptr))) {
				item = pyrna_struct_CreatePyObject(&iter.ptr);
				PyList_Append(ret, item);
				Py_DECREF(item);
			}
		}
		RNA_property_collection_end(&iter);
	}
	
	return ret;
}

/* A bit of a kludge, make a list out of a collection or array,
 * then return the lists iter function, not especially fast but convenient for now */
PyObject *pyrna_prop_iter(BPy_PropertyRNA *self)
{
	/* Try get values from a collection */
	PyObject *ret = pyrna_prop_values(self);
	
	if (ret==NULL) {
		/* collection did not work, try array */
		int len = RNA_property_array_length(&self->ptr, self->prop);
		
		if (len) {
			int i;
			PyErr_Clear();
			ret = PyList_New(len);
			
			for (i=0; i < len; i++) {
				PyList_SET_ITEM(ret, i, pyrna_prop_to_py_index(&self->ptr, self->prop, i));
			}
		}
	}
	
	if (ret) {
		/* we know this is a list so no need to PyIter_Check */
		PyObject *iter = PyObject_GetIter(ret); 
		Py_DECREF(ret);
		return iter;
	}
	
	PyErr_SetString( PyExc_TypeError, "this BPy_PropertyRNA object is not iterable" );
	return NULL;
}

/*static struct PyMethodDef pyrna_struct_methods[] = {
	{"__dir__", (PyCFunction)pyrna_struct_dir, METH_NOARGS, ""},
	{NULL, NULL, 0, NULL}
};*/

static struct PyMethodDef pyrna_prop_methods[] = {
	{"keys", (PyCFunction)pyrna_prop_keys, METH_NOARGS, ""},
	{"items", (PyCFunction)pyrna_prop_items, METH_NOARGS, ""},
	{"values", (PyCFunction)pyrna_prop_values, METH_NOARGS, ""},
	{NULL, NULL, 0, NULL}
};

/* only needed for subtyping, so a new class gets a valid BPy_StructRNA
 * todo - also accept useful args */
static PyObject * pyrna_struct_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	BPy_StructRNA *base = NULL;
	
	if (!PyArg_ParseTuple(args, "O!:Base BPy_StructRNA", &pyrna_struct_Type, &base))
		return NULL;
	
	if (type == &pyrna_struct_Type) {
		return pyrna_struct_CreatePyObject(&base->ptr);
	} else {
		BPy_StructRNA *ret = (BPy_StructRNA *) type->tp_alloc(type, 0);
		ret->ptr = base->ptr;
		return (PyObject *)ret;
	}
}

/* only needed for subtyping, so a new class gets a valid BPy_StructRNA
 * todo - also accept useful args */
static PyObject * pyrna_prop_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {

	BPy_PropertyRNA *base = NULL;
	
	if (!PyArg_ParseTuple(args, "O!:Base BPy_PropertyRNA", &pyrna_prop_Type, &base))
		return NULL;
	
	if (type == &pyrna_prop_Type) {
		return pyrna_prop_CreatePyObject(&base->ptr, base->prop);
	} else {
		BPy_PropertyRNA *ret = (BPy_PropertyRNA *) type->tp_alloc(type, 0);
		ret->ptr = base->ptr;
		ret->prop = base->prop;
		return (PyObject *)ret;
	}
}

/*-----------------------BPy_StructRNA method def------------------------------*/
PyTypeObject pyrna_struct_Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif
	"StructRNA",			/* tp_name */
	sizeof( BPy_StructRNA ),	/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	( destructor ) pyrna_struct_dealloc,/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,						/* getattrfunc tp_getattr; */
	NULL,						/* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */ /* DEPRECATED in python 3.0! */
	( reprfunc ) pyrna_struct_repr,	/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,						/* PySequenceMethods *tp_as_sequence; */
	NULL,						/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	( hashfunc )pyrna_struct_hash,	/* hashfunc tp_hash; */
	NULL,						/* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	( getattrofunc ) pyrna_struct_getattro,	/* getattrofunc tp_getattro; */
	( setattrofunc ) pyrna_struct_setattro,	/* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* long tp_flags; */

	NULL,						/*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	(richcmpfunc)pyrna_struct_richcmp,	/* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	NULL,                       /* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	NULL,			/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,      					/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	pyrna_struct_new,			/* newfunc tp_new; */
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

/*-----------------------BPy_PropertyRNA method def------------------------------*/
PyTypeObject pyrna_prop_Type = {
#if (PY_VERSION_HEX >= 0x02060000)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	/* python 2.5 and below */
	PyObject_HEAD_INIT( NULL )  /* required py macro */
	0,                          /* ob_size */
#endif
	
	"PropertyRNA",		/* tp_name */
	sizeof( BPy_PropertyRNA ),			/* tp_basicsize */
	0,			/* tp_itemsize */
	/* methods */
	NULL,						/* tp_dealloc */
	NULL,                       /* printfunc tp_print; */
	NULL,						/* getattrfunc tp_getattr; */
	NULL,                       /* setattrfunc tp_setattr; */
	NULL,						/* tp_compare */ /* DEPRECATED in python 3.0! */
	( reprfunc ) pyrna_prop_repr,	/* tp_repr */

	/* Method suites for standard classes */

	NULL,                       /* PyNumberMethods *tp_as_number; */
	NULL,						/* PySequenceMethods *tp_as_sequence; */
	&pyrna_prop_as_mapping,		/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */
	NULL, /*PyObject_GenericGetAttr - MINGW Complains, assign later */	/* getattrofunc tp_getattro; */ /* will only use these if this is a subtype of a py class */
	NULL, /*PyObject_GenericSetAttr - MINGW Complains, assign later */	/* setattrofunc tp_setattro; */

	/* Functions to access object as input/output buffer */
	NULL,                       /* PyBufferProcs *tp_as_buffer; */

  /*** Flags to define presence of optional/expanded features ***/
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* long tp_flags; */

	NULL,						/*  char *tp_doc;  Documentation string */
  /*** Assigned meaning in release 2.0 ***/
	/* call function for all accessible objects */
	NULL,                       /* traverseproc tp_traverse; */

	/* delete references to contained objects */
	NULL,                       /* inquiry tp_clear; */

  /***  Assigned meaning in release 2.1 ***/
  /*** rich comparisons ***/
	(richcmpfunc)pyrna_prop_richcmp,	/* richcmpfunc tp_richcompare; */

  /***  weak reference enabler ***/
	0,                          /* long tp_weaklistoffset; */

  /*** Added in release 2.2 ***/
	/*   Iterators */
	(getiterfunc)pyrna_prop_iter,	/* getiterfunc tp_iter; */
	NULL,                       /* iternextfunc tp_iternext; */

  /*** Attribute descriptor and subclassing stuff ***/
	pyrna_prop_methods,			/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	NULL,      					/* struct PyGetSetDef *tp_getset; */
	NULL,                       /* struct _typeobject *tp_base; */
	NULL,                       /* PyObject *tp_dict; */
	NULL,                       /* descrgetfunc tp_descr_get; */
	NULL,                       /* descrsetfunc tp_descr_set; */
	0,                          /* long tp_dictoffset; */
	NULL,                       /* initproc tp_init; */
	NULL,                       /* allocfunc tp_alloc; */
	pyrna_prop_new,				/* newfunc tp_new; */
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

/*-----------------------CreatePyObject---------------------------------*/
PyObject *pyrna_struct_CreatePyObject( PointerRNA *ptr )
{
	BPy_StructRNA *pyrna;
	
	if (ptr->data==NULL) {
		Py_RETURN_NONE;
	}

	pyrna = ( BPy_StructRNA * ) PyObject_NEW( BPy_StructRNA, &pyrna_struct_Type );

	if( !pyrna ) {
		PyErr_SetString( PyExc_MemoryError, "couldn't create BPy_StructRNA object" );
		return NULL;
	}
	
	pyrna->ptr= *ptr;
	pyrna->freeptr= 0;
	
	return ( PyObject * ) pyrna;
}

PyObject *pyrna_prop_CreatePyObject( PointerRNA *ptr, PropertyRNA *prop )
{
	BPy_PropertyRNA *pyrna;

	pyrna = ( BPy_PropertyRNA * ) PyObject_NEW( BPy_PropertyRNA, &pyrna_prop_Type );

	if( !pyrna ) {
		PyErr_SetString( PyExc_MemoryError, "couldn't create BPy_rna object" );
		return NULL;
	}
	
	pyrna->ptr = *ptr;
	pyrna->prop = prop;
		
	return ( PyObject * ) pyrna;
}


PyObject *BPY_rna_module( void )
{
	PointerRNA ptr;
	
	if( PyType_Ready( &pyrna_struct_Type ) < 0 )
		return NULL;
	
	/* This can't be set in the pytype struct because some compilers complain */
	pyrna_prop_Type.tp_getattro = PyObject_GenericGetAttr; 
	pyrna_prop_Type.tp_setattro = PyObject_GenericSetAttr; 
	
	if( PyType_Ready( &pyrna_prop_Type ) < 0 )
		return NULL;
	
	/* for now, return the base RNA type rather then a real module */
	RNA_main_pointer_create(G.main, &ptr);
	
	//submodule = Py_InitModule3( "rna", M_rna_methods, "rna module" );
	return pyrna_struct_CreatePyObject(&ptr);
}

/* This is a way we can access docstrings for RNA types
 * without having the datatypes in blender */
PyObject *BPY_rna_doc( void )
{
	PointerRNA ptr;
	
	/* for now, return the base RNA type rather then a real module */
	RNA_blender_rna_pointer_create(&ptr);
	
	return pyrna_struct_CreatePyObject(&ptr);
}


