
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
#include "bpy_props.h"
#include "bpy_util.h"
//#include "blendef.h"
#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "float.h" /* FLT_MIN/MAX */

#include "RNA_access.h"
#include "RNA_define.h" /* for defining our own rna */

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BKE_context.h"
#include "BKE_global.h" /* evil G.* */
#include "BKE_report.h"

#include "BKE_animsys.h"
#include "BKE_fcurve.h"

/* only for keyframing */
#include "DNA_scene_types.h"
#include "DNA_anim_types.h"
#include "ED_keyframing.h"

#define USE_MATHUTILS

#ifdef USE_MATHUTILS
#include "../generic/Mathutils.h" /* so we can have mathutils callbacks */
#include "../generic/IDProp.h" /* for IDprop lookups */

static PyObject *prop_subscript_array_slice(BPy_PropertyRNA *self, PointerRNA *ptr, PropertyRNA *prop, int start, int stop, int length);

/* bpyrna vector/euler/quat callbacks */
static int mathutils_rna_array_cb_index= -1; /* index for our callbacks */

static int mathutils_rna_generic_check(BPy_PropertyRNA *self)
{
	return self->prop?1:0;
}

static int mathutils_rna_vector_get(BPy_PropertyRNA *self, int subtype, float *vec_from)
{
	if(self->prop==NULL)
		return 0;
	
	RNA_property_float_get_array(&self->ptr, self->prop, vec_from);
	return 1;
}

static int mathutils_rna_vector_set(BPy_PropertyRNA *self, int subtype, float *vec_to)
{
	if(self->prop==NULL)
		return 0;

	RNA_property_float_set_array(&self->ptr, self->prop, vec_to);
	RNA_property_update(BPy_GetContext(), &self->ptr, self->prop);
	return 1;
}

static int mathutils_rna_vector_get_index(BPy_PropertyRNA *self, int subtype, float *vec_from, int index)
{
	if(self->prop==NULL)
		return 0;
	
	vec_from[index]= RNA_property_float_get_index(&self->ptr, self->prop, index);
	return 1;
}

static int mathutils_rna_vector_set_index(BPy_PropertyRNA *self, int subtype, float *vec_to, int index)
{
	if(self->prop==NULL)
		return 0;

	RNA_property_float_set_index(&self->ptr, self->prop, index, vec_to[index]);
	RNA_property_update(BPy_GetContext(), &self->ptr, self->prop);
	return 1;
}

Mathutils_Callback mathutils_rna_array_cb = {
	(BaseMathCheckFunc)		mathutils_rna_generic_check,
	(BaseMathGetFunc)		mathutils_rna_vector_get,
	(BaseMathSetFunc)		mathutils_rna_vector_set,
	(BaseMathGetIndexFunc)	mathutils_rna_vector_get_index,
	(BaseMathSetIndexFunc)	mathutils_rna_vector_set_index
};


/* bpyrna matrix callbacks */
static int mathutils_rna_matrix_cb_index= -1; /* index for our callbacks */

static int mathutils_rna_matrix_get(BPy_PropertyRNA *self, int subtype, float *mat_from)
{
	if(self->prop==NULL)
		return 0;

	RNA_property_float_get_array(&self->ptr, self->prop, mat_from);
	return 1;
}

static int mathutils_rna_matrix_set(BPy_PropertyRNA *self, int subtype, float *mat_to)
{
	if(self->prop==NULL)
		return 0;

	RNA_property_float_set_array(&self->ptr, self->prop, mat_to);
	RNA_property_update(BPy_GetContext(), &self->ptr, self->prop);
	return 1;
}

Mathutils_Callback mathutils_rna_matrix_cb = {
	(BaseMathCheckFunc)		mathutils_rna_generic_check,
	(BaseMathGetFunc)		mathutils_rna_matrix_get,
	(BaseMathSetFunc)		mathutils_rna_matrix_set,
	(BaseMathGetIndexFunc)	NULL,
	(BaseMathSetIndexFunc)	NULL
};

#define PROP_ALL_VECTOR_SUBTYPES PROP_TRANSLATION: case PROP_DIRECTION: case PROP_VELOCITY: case PROP_ACCELERATION: case PROP_XYZ: case PROP_XYZ|PROP_UNIT_LENGTH

PyObject *pyrna_math_object_from_array(PointerRNA *ptr, PropertyRNA *prop)
{
	PyObject *ret= NULL;

#ifdef USE_MATHUTILS
	int subtype, totdim;
	int len;
	int is_thick;
	int flag= RNA_property_flag(prop);

	/* disallow dynamic sized arrays to be wrapped since the size could change
	 * to a size mathutils does not support */
	if ((RNA_property_type(prop) != PROP_FLOAT) || (flag & PROP_DYNAMIC))
		return NULL;

	len= RNA_property_array_length(ptr, prop);
	subtype= RNA_property_subtype(prop);
	totdim= RNA_property_array_dimension(ptr, prop, NULL);
	is_thick = (flag & PROP_THICK_WRAP);

	if (totdim == 1 || (totdim == 2 && subtype == PROP_MATRIX)) {
		if(!is_thick)
			ret = pyrna_prop_CreatePyObject(ptr, prop); /* owned by the Mathutils PyObject */

		switch(RNA_property_subtype(prop)) {
		case PROP_ALL_VECTOR_SUBTYPES:
			if(len>=2 && len <= 4) {
				if(is_thick) {
					ret= newVectorObject(NULL, len, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((VectorObject *)ret)->vec);
				}
				else {
					PyObject *vec_cb= newVectorObject_cb(ret, len, mathutils_rna_array_cb_index, FALSE);
					Py_DECREF(ret); /* the vector owns now */
					ret= vec_cb; /* return the vector instead */
				}
			}
			break;
		case PROP_MATRIX:
			if(len==16) {
				if(is_thick) {
					ret= newMatrixObject(NULL, 4, 4, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((MatrixObject *)ret)->contigPtr);
				}
				else {
					PyObject *mat_cb= newMatrixObject_cb(ret, 4,4, mathutils_rna_matrix_cb_index, FALSE);
					Py_DECREF(ret); /* the matrix owns now */
					ret= mat_cb; /* return the matrix instead */
				}
			}
			else if (len==9) {
				if(is_thick) {
					ret= newMatrixObject(NULL, 3, 3, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((MatrixObject *)ret)->contigPtr);
				}
				else {
					PyObject *mat_cb= newMatrixObject_cb(ret, 3,3, mathutils_rna_matrix_cb_index, FALSE);
					Py_DECREF(ret); /* the matrix owns now */
					ret= mat_cb; /* return the matrix instead */
				}
			}
			break;
		case PROP_EULER:
		case PROP_QUATERNION:
			if(len==3) { /* euler */
				if(is_thick) {
					ret= newEulerObject(NULL, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((EulerObject *)ret)->eul);
				}
				else {
					PyObject *eul_cb= newEulerObject_cb(ret, mathutils_rna_array_cb_index, FALSE);
					Py_DECREF(ret); /* the matrix owns now */
					ret= eul_cb; /* return the matrix instead */
				}
			}
			else if (len==4) {
				if(is_thick) {
					ret= newQuaternionObject(NULL, Py_NEW, NULL);
					RNA_property_float_get_array(ptr, prop, ((QuaternionObject *)ret)->quat);
				}
				else {
					PyObject *quat_cb= newQuaternionObject_cb(ret, mathutils_rna_array_cb_index, FALSE);
					Py_DECREF(ret); /* the matrix owns now */
					ret= quat_cb; /* return the matrix instead */
				}
			}
			break;
		default:
			break;
		}
	}

	if(ret==NULL) {
		if(is_thick) {
			/* this is an array we cant reference (since its not thin wrappable)
			 * and cannot be coerced into a mathutils type, so return as a list */
			ret = prop_subscript_array_slice(NULL, ptr, prop, 0, len, len);
		} else {
			ret = pyrna_prop_CreatePyObject(ptr, prop); /* owned by the Mathutils PyObject */
		}
	}

#endif

	return ret;
}

#endif

static int pyrna_struct_compare( BPy_StructRNA * a, BPy_StructRNA * b )
{
	return (a->ptr.data==b->ptr.data) ? 0 : -1;
}

static int pyrna_prop_compare( BPy_PropertyRNA * a, BPy_PropertyRNA * b )
{
	return (a->prop==b->prop && a->ptr.data==b->ptr.data ) ? 0 : -1;
}

static PyObject *pyrna_struct_richcmp(PyObject *a, PyObject *b, int op)
{
	PyObject *res;
	int ok= -1; /* zero is true */

	if (BPy_StructRNA_Check(a) && BPy_StructRNA_Check(b))
		ok= pyrna_struct_compare((BPy_StructRNA *)a, (BPy_StructRNA *)b);

	switch (op) {
	case Py_NE:
		ok = !ok; /* pass through */
	case Py_EQ:
		res = ok ? Py_False : Py_True;
		break;

	case Py_LT:
	case Py_LE:
	case Py_GT:
	case Py_GE:
		res = Py_NotImplemented;
		break;
	default:
		PyErr_BadArgument();
		return NULL;
	}

	Py_INCREF(res);
	return res;
}

static PyObject *pyrna_prop_richcmp(PyObject *a, PyObject *b, int op)
{
	PyObject *res;
	int ok= -1; /* zero is true */

	if (BPy_PropertyRNA_Check(a) && BPy_PropertyRNA_Check(b))
		ok= pyrna_prop_compare((BPy_PropertyRNA *)a, (BPy_PropertyRNA *)b);

	switch (op) {
	case Py_NE:
		ok = !ok; /* pass through */
	case Py_EQ:
		res = ok ? Py_False : Py_True;
		break;

	case Py_LT:
	case Py_LE:
	case Py_GT:
	case Py_GE:
		res = Py_NotImplemented;
		break;
	default:
		PyErr_BadArgument();
		return NULL;
	}

	Py_INCREF(res);
	return res;
}

/*----------------------repr--------------------------------------------*/
static PyObject *pyrna_struct_repr( BPy_StructRNA *self )
{
	PyObject *pyob;
	char *name;

	/* print name if available */
	name= RNA_struct_name_get_alloc(&self->ptr, NULL, FALSE);
	if(name) {
		pyob= PyUnicode_FromFormat( "[BPy_StructRNA \"%.200s\" -> \"%.200s\"]", RNA_struct_identifier(self->ptr.type), name);
		MEM_freeN(name);
		return pyob;
	}

	return PyUnicode_FromFormat( "[BPy_StructRNA \"%.200s\"]", RNA_struct_identifier(self->ptr.type));
}

static PyObject *pyrna_prop_repr( BPy_PropertyRNA *self )
{
	PyObject *pyob;
	PointerRNA ptr;
	char *name;

	/* if a pointer, try to print name of pointer target too */
	if(RNA_property_type(self->prop) == PROP_POINTER) {
		ptr= RNA_property_pointer_get(&self->ptr, self->prop);
		name= RNA_struct_name_get_alloc(&ptr, NULL, FALSE);

		if(name) {
			pyob= PyUnicode_FromFormat( "[BPy_PropertyRNA \"%.200s\" -> \"%.200s\" -> \"%.200s\" ]", RNA_struct_identifier(self->ptr.type), RNA_property_identifier(self->prop), name);
			MEM_freeN(name);
			return pyob;
		}
	}

	return PyUnicode_FromFormat( "[BPy_PropertyRNA \"%.200s\" -> \"%.200s\"]", RNA_struct_identifier(self->ptr.type), RNA_property_identifier(self->prop));
}

static long pyrna_struct_hash( BPy_StructRNA *self )
{
	return (long)self->ptr.data;
}

/* use our own dealloc so we can free a property if we use one */
static void pyrna_struct_dealloc( BPy_StructRNA *self )
{
	if (self->freeptr && self->ptr.data) {
		IDP_FreeProperty(self->ptr.data);
		if (self->ptr.type != &RNA_Context)
		{
			MEM_freeN(self->ptr.data);
			self->ptr.data= NULL;
		}
	}

	/* Note, for subclassed PyObjects we cant just call PyObject_DEL() directly or it will crash */
	Py_TYPE(self)->tp_free(self);
	return;
}

static char *pyrna_enum_as_string(PointerRNA *ptr, PropertyRNA *prop)
{
	EnumPropertyItem *item;
	char *result;
	int free= FALSE;
	
	RNA_property_enum_items(BPy_GetContext(), ptr, prop, &item, NULL, &free);
	if(item) {
		result= (char*)BPy_enum_as_string(item);
	}
	else {
		result= "";
	}
	
	if(free)
		MEM_freeN(item);
	
	return result;
}

static int pyrna_string_to_enum(PyObject *item, PointerRNA *ptr, PropertyRNA *prop, int *val, const char *error_prefix)
{
	char *param= _PyUnicode_AsString(item);

	if (param==NULL) {
		char *enum_str= pyrna_enum_as_string(ptr, prop);
		PyErr_Format(PyExc_TypeError, "%.200s expected a string enum type in (%.200s)", error_prefix, enum_str);
		MEM_freeN(enum_str);
		return 0;
	} else {
		if (!RNA_property_enum_value(BPy_GetContext(), ptr, prop, param, val)) {
			char *enum_str= pyrna_enum_as_string(ptr, prop);
			PyErr_Format(PyExc_TypeError, "%.200s enum \"%.200s\" not found in (%.200s)", error_prefix, param, enum_str);
			MEM_freeN(enum_str);
			return 0;
		}
	}

	return 1;
}

PyObject *pyrna_enum_bitfield_to_py(EnumPropertyItem *items, int value)
{
	PyObject *ret= PySet_New(NULL);
	const char *identifier[RNA_ENUM_BITFLAG_SIZE + 1];

	if(RNA_enum_bitflag_identifiers(items, value, identifier)) {
		PyObject *item;
		int index;
		for(index=0; identifier[index]; index++) {
			item= PyUnicode_FromString(identifier[index]);
			PySet_Add(ret, item);
			Py_DECREF(item);
		}
	}

	return ret;
}

static PyObject *pyrna_enum_to_py(PointerRNA *ptr, PropertyRNA *prop, int val)
{
	PyObject *item, *ret= NULL;

	if(RNA_property_flag(prop) & PROP_ENUM_FLAG) {
		const char *identifier[RNA_ENUM_BITFLAG_SIZE + 1];

		ret= PySet_New(NULL);

		if (RNA_property_enum_bitflag_identifiers(BPy_GetContext(), ptr, prop, val, identifier)) {
			int index;

			for(index=0; identifier[index]; index++) {
				item= PyUnicode_FromString(identifier[index]);
				PySet_Add(ret, item);
				Py_DECREF(item);
			}

		}
	}
	else {
		const char *identifier;
		if (RNA_property_enum_identifier(BPy_GetContext(), ptr, prop, val, &identifier)) {
			ret = PyUnicode_FromString(identifier);
		} else {
			EnumPropertyItem *item;
			int free= FALSE;

			/* don't throw error here, can't trust blender 100% to give the
			 * right values, python code should not generate error for that */
			RNA_property_enum_items(BPy_GetContext(), ptr, prop, &item, NULL, &free);
			if(item && item->identifier) {
				ret= PyUnicode_FromString(item->identifier);
			}
			else {
				char *ptr_name= RNA_struct_name_get_alloc(ptr, NULL, FALSE);

				/* prefer not fail silently incase of api errors, maybe disable it later */
				printf("RNA Warning: Current value \"%d\" matches no enum in '%s', '%s', '%s'\n", val, RNA_struct_identifier(ptr->type), ptr_name, RNA_property_identifier(prop));

#if 0           // gives python decoding errors while generating docs :(
				char error_str[256];
				snprintf(error_str, sizeof(error_str), "RNA Warning: Current value \"%d\" matches no enum in '%s', '%s', '%s'", val, RNA_struct_identifier(ptr->type), ptr_name, RNA_property_identifier(prop));
				PyErr_Warn(PyExc_RuntimeWarning, error_str);
#endif

				if(ptr_name)
					MEM_freeN(ptr_name);

				ret = PyUnicode_FromString( "" );
			}

			if(free)
				MEM_freeN(item);

			/*PyErr_Format(PyExc_AttributeError, "RNA Error: Current value \"%d\" matches no enum", val);
			ret = NULL;*/
		}
	}

	return ret;
}

PyObject * pyrna_prop_to_py(PointerRNA *ptr, PropertyRNA *prop)
{
	PyObject *ret;
	int type = RNA_property_type(prop);

	if (RNA_property_array_check(ptr, prop)) {
		return pyrna_py_from_array(ptr, prop);
	}
	
	/* see if we can coorce into a python type - PropertyType */
	switch (type) {
	case PROP_BOOLEAN:
		ret = PyBool_FromLong( RNA_property_boolean_get(ptr, prop) );
		break;
	case PROP_INT:
		ret = PyLong_FromSsize_t( (Py_ssize_t)RNA_property_int_get(ptr, prop) );
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
		ret= pyrna_enum_to_py(ptr, prop, RNA_property_enum_get(ptr, prop));
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
		PyErr_Format(PyExc_TypeError, "RNA Error: unknown type \"%d\" (pyrna_prop_to_py)", type);
		ret = NULL;
		break;
	}
	
	return ret;
}

/* This function is used by operators and converting dicts into collections.
 * Its takes keyword args and fills them with property values */
int pyrna_pydict_to_props(PointerRNA *ptr, PyObject *kw, int all_args, const char *error_prefix)
{
	int error_val = 0;
	int totkw;
	const char *arg_name= NULL;
	PyObject *item;

	totkw = kw ? PyDict_Size(kw):0;

	RNA_STRUCT_BEGIN(ptr, prop) {
		arg_name= RNA_property_identifier(prop);

		if (strcmp(arg_name, "rna_type")==0) continue;

		if (kw==NULL) {
			PyErr_Format( PyExc_TypeError, "%.200s: no keywords, expected \"%.200s\"", error_prefix, arg_name ? arg_name : "<UNKNOWN>");
			error_val= -1;
			break;
		}

		item= PyDict_GetItemString(kw, arg_name); /* wont set an error */

		if (item == NULL) {
			if(all_args) {
				PyErr_Format( PyExc_TypeError, "%.200s: keyword \"%.200s\" missing", error_prefix, arg_name ? arg_name : "<UNKNOWN>");
				error_val = -1; /* pyrna_py_to_prop sets the error */
				break;
			}
		} else {
			if (pyrna_py_to_prop(ptr, prop, NULL, NULL, item, error_prefix)) {
				error_val= -1;
				break;
			}
			totkw--;
		}
	}
	RNA_STRUCT_END;

	if (error_val==0 && totkw > 0) { /* some keywords were given that were not used :/ */
		PyObject *key, *value;
		Py_ssize_t pos = 0;

		while (PyDict_Next(kw, &pos, &key, &value)) {
			arg_name= _PyUnicode_AsString(key);
			if (RNA_struct_find_property(ptr, arg_name) == NULL) break;
			arg_name= NULL;
		}

		PyErr_Format( PyExc_TypeError, "%.200s: keyword \"%.200s\" unrecognized", error_prefix, arg_name ? arg_name : "<UNKNOWN>");
		error_val = -1;
	}

	return error_val;
}

static PyObject * pyrna_func_call(PyObject *self, PyObject *args, PyObject *kw);

static PyObject *pyrna_func_to_py(BPy_DummyPointerRNA *pyrna, FunctionRNA *func)
{
	static PyMethodDef func_meth = {"<generic rna function>", (PyCFunction)pyrna_func_call, METH_VARARGS|METH_KEYWORDS, "python rna function"};
	PyObject *self;
	PyObject *ret;
	
	if(func==NULL) {
		PyErr_Format( PyExc_RuntimeError, "%.200s: type attempted to get NULL function", RNA_struct_identifier(pyrna->ptr.type));
		return NULL;
	}

	self= PyTuple_New(2);
	
	PyTuple_SET_ITEM(self, 0, (PyObject *)pyrna);
	Py_INCREF(pyrna);

	PyTuple_SET_ITEM(self, 1, PyCObject_FromVoidPtr((void *)func, NULL));
	
	ret= PyCFunction_New(&func_meth, self);
	Py_DECREF(self);
	
	return ret;
}



int pyrna_py_to_prop(PointerRNA *ptr, PropertyRNA *prop, ParameterList *parms, void *data, PyObject *value, const char *error_prefix)
{
	/* XXX hard limits should be checked here */
	int type = RNA_property_type(prop);
	

	if (RNA_property_array_check(ptr, prop)) {

		/* char error_str[512]; */
		int ok= 1;

#ifdef USE_MATHUTILS
		if(MatrixObject_Check(value)) {
			MatrixObject *mat = (MatrixObject*)value;
			if(!BaseMath_ReadCallback(mat))
				return -1;
		} else /* continue... */
#endif
		if (!PySequence_Check(value)) {
			PyErr_Format(PyExc_TypeError, "%.200s RNA array assignment expected a sequence instead of %.200s instance.", error_prefix, Py_TYPE(value)->tp_name);
			return -1;
		}
		/* done getting the length */
		ok= pyrna_py_to_array(ptr, prop, parms, data, value, error_prefix);

		if (!ok) {
			/* PyErr_Format(PyExc_AttributeError, "%.200s %s", error_prefix, error_str); */
			return -1;
		}
	}
	else {
		/* Normal Property (not an array) */
		
		/* see if we can coorce into a python type - PropertyType */
		switch (type) {
		case PROP_BOOLEAN:
		{
			int param;
			/* prefer not to have an exception here
			 * however so many poll functions return None or a valid Object.
			 * its a hassle to convert these into a bool before returning, */
			if(RNA_property_flag(prop) & PROP_OUTPUT)
				param = PyObject_IsTrue( value );
			else
				param = PyLong_AsSsize_t( value );
			
			if( param < 0 || param > 1) {
				PyErr_Format(PyExc_TypeError, "%.200s expected True/False or 0/1", error_prefix);
				return -1;
			} else {
				if(data)	*((int*)data)= param;
				else		RNA_property_boolean_set(ptr, prop, param);
			}
			break;
		}
		case PROP_INT:
		{
			int param = PyLong_AsSsize_t(value);
			if (param==-1 && PyErr_Occurred()) {
				PyErr_Format(PyExc_TypeError, "%.200s expected an int type", error_prefix);
				return -1;
			} else {
				if(data)	*((int*)data)= param;
				else		RNA_property_int_set(ptr, prop, param);
			}
			break;
		}
		case PROP_FLOAT:
		{
			float param = PyFloat_AsDouble(value);
			if (PyErr_Occurred()) {
				PyErr_Format(PyExc_TypeError, "%.200s expected a float type", error_prefix);
				return -1;
			} else {
				if(data)	*((float*)data)= param;
				else		RNA_property_float_set(ptr, prop, param);
			}
			break;
		}
		case PROP_STRING:
		{
			char *param = _PyUnicode_AsString(value);
			
			if (param==NULL) {
				PyErr_Format(PyExc_TypeError, "%.200s expected a string type", error_prefix);
				return -1;
			} else {
				if(data)	*((char**)data)= param;
				else		RNA_property_string_set(ptr, prop, param);
			}
			break;
		}
		case PROP_ENUM:
		{
			int val= 0, tmpval;

			if (PyUnicode_Check(value)) {
				if (!pyrna_string_to_enum(value, ptr, prop, &val, error_prefix))
					return -1;
			}
			else if (PyAnySet_Check(value)) {
				if(RNA_property_flag(prop) & PROP_ENUM_FLAG) {
					/* set of enum items, concatenate all values with OR */

					/* set looping */
					Py_ssize_t pos = 0;
					PyObject *key;
					long hash;

					while (_PySet_NextEntry(value, &pos, &key, &hash)) {
						if (!pyrna_string_to_enum(key, ptr, prop, &tmpval, error_prefix))
							return -1;

						val |= tmpval;
					}
				}
				else {
					PyErr_Format(PyExc_TypeError, "%.200s, %.200s.%.200s is not a bitflag enum type", error_prefix, RNA_struct_identifier(ptr->type), RNA_property_identifier(prop));
					return -1;
				}
			}
			else {
				char *enum_str= pyrna_enum_as_string(ptr, prop);
				PyErr_Format(PyExc_TypeError, "%.200s expected a string enum or a set of strings in (%.200s)", error_prefix, enum_str);
				MEM_freeN(enum_str);
				return -1;
			}

			if(data)	*((int*)data)= val;
			else		RNA_property_enum_set(ptr, prop, val);
			
			break;
		}
		case PROP_POINTER:
		{
			StructRNA *ptype= RNA_property_pointer_type(ptr, prop);
			int flag = RNA_property_flag(prop);

			/* if property is an OperatorProperties pointer and value is a map, forward back to pyrna_pydict_to_props */
			if (RNA_struct_is_a(ptype, &RNA_OperatorProperties) && PyDict_Check(value)) {
				PointerRNA opptr = RNA_property_pointer_get(ptr, prop);
				return pyrna_pydict_to_props(&opptr, value, 0, error_prefix);
			}

			if(!BPy_StructRNA_Check(value) && value != Py_None) {
				PyErr_Format(PyExc_TypeError, "%.200s expected a %.200s type", error_prefix, RNA_struct_identifier(ptype));
				return -1;
			} else if((flag & PROP_NEVER_NULL) && value == Py_None) {
				PyErr_Format(PyExc_TypeError, "%.200s does not support a 'None' assignment %.200s type", error_prefix, RNA_struct_identifier(ptype));
				return -1;
			} else {
				BPy_StructRNA *param= (BPy_StructRNA*)value;
				int raise_error= FALSE;
				if(data) {

					if(flag & PROP_RNAPTR) {
						if(value == Py_None)
							memset(data, 0, sizeof(PointerRNA));
						else
							*((PointerRNA*)data)= param->ptr;
					}
					else if(value == Py_None) {
						*((void**)data)= NULL;
					}
					else if(RNA_struct_is_a(param->ptr.type, ptype)) {
						*((void**)data)= param->ptr.data;
					}
					else {
						raise_error= TRUE;
					}
				}
				else {
					/* data==NULL, assign to RNA */
					if(value == Py_None) {
						PointerRNA valueptr;
						memset(&valueptr, 0, sizeof(valueptr));
						RNA_property_pointer_set(ptr, prop, valueptr);
					}
					else if(RNA_struct_is_a(param->ptr.type, ptype)) {
						RNA_property_pointer_set(ptr, prop, param->ptr);
					}
					else {
						PointerRNA tmp;
						RNA_pointer_create(NULL, ptype, NULL, &tmp);
						PyErr_Format(PyExc_TypeError, "%.200s expected a %.200s type", error_prefix, RNA_struct_identifier(tmp.type));
						return -1;
					}
				}
				
				if(raise_error) {
					PointerRNA tmp;
					RNA_pointer_create(NULL, ptype, NULL, &tmp);
					PyErr_Format(PyExc_TypeError, "%.200s expected a %.200s type", error_prefix, RNA_struct_identifier(tmp.type));
					return -1;
				}
			}
			break;
		}
		case PROP_COLLECTION:
		{
			int seq_len, i;
			PyObject *item;
			PointerRNA itemptr;
			ListBase *lb;
			CollectionPointerLink *link;

			lb= (data)? (ListBase*)data: NULL;
			
			/* convert a sequence of dict's into a collection */
			if(!PySequence_Check(value)) {
				PyErr_Format(PyExc_TypeError, "%.200s expected a sequence of dicts for an RNA collection", error_prefix);
				return -1;
			}
			
			seq_len = PySequence_Length(value);
			for(i=0; i<seq_len; i++) {
				item= PySequence_GetItem(value, i);
				if(item==NULL || PyDict_Check(item)==0) {
					PyErr_Format(PyExc_TypeError, "%.200s expected a sequence of dicts for an RNA collection", error_prefix);
					Py_XDECREF(item);
					return -1;
				}

				if(lb) {
					link= MEM_callocN(sizeof(CollectionPointerLink), "PyCollectionPointerLink");
					link->ptr= itemptr;
					BLI_addtail(lb, link);
				}
				else
					RNA_property_collection_add(ptr, prop, &itemptr);

				if(pyrna_pydict_to_props(&itemptr, item, 1, "Converting a python list to an RNA collection")==-1) {
					Py_DECREF(item);
					return -1;
				}
				Py_DECREF(item);
			}
			
			break;
		}
		default:
			PyErr_Format(PyExc_AttributeError, "%.200s unknown property type (pyrna_py_to_prop)", error_prefix);
			return -1;
			break;
		}
	}

	/* Run rna property functions */
	RNA_property_update(BPy_GetContext(), ptr, prop);

	return 0;
}

static PyObject * pyrna_prop_to_py_index(BPy_PropertyRNA *self, int index)
{
	return pyrna_py_from_array_index(self, &self->ptr, self->prop, index);
}

static int pyrna_py_to_prop_index(BPy_PropertyRNA *self, int index, PyObject *value)
{
	int ret = 0;
	int totdim;
	PointerRNA *ptr= &self->ptr;
	PropertyRNA *prop= self->prop;
	int type = RNA_property_type(prop);

	totdim= RNA_property_array_dimension(ptr, prop, NULL);

	if (totdim > 1) {
		/* char error_str[512]; */
		if (!pyrna_py_to_array_index(&self->ptr, self->prop, self->arraydim, self->arrayoffset, index, value, "")) {
			/* PyErr_SetString(PyExc_AttributeError, error_str); */
			ret= -1;
		}
	}
	else {
		/* see if we can coorce into a python type - PropertyType */
		switch (type) {
		case PROP_BOOLEAN:
			{
				int param = PyLong_AsSsize_t( value );
		
				if( param < 0 || param > 1) {
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
				if (param==-1 && PyErr_Occurred()) {
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
	}
	
	return ret;
}

//---------------sequence-------------------------------------------
static int pyrna_prop_array_length(BPy_PropertyRNA *self)
{
	if (RNA_property_array_dimension(&self->ptr, self->prop, NULL) > 1)
		return RNA_property_multi_array_length(&self->ptr, self->prop, self->arraydim);
	else
		return RNA_property_array_length(&self->ptr, self->prop);
}

static Py_ssize_t pyrna_prop_len( BPy_PropertyRNA *self )
{
	Py_ssize_t len;
	
	if (RNA_property_type(self->prop) == PROP_COLLECTION) {
		len = RNA_property_collection_length(&self->ptr, self->prop);
	} else if (RNA_property_array_check(&self->ptr, self->prop)) {
		len = pyrna_prop_array_length(self);
	} else {
		PyErr_SetString(PyExc_AttributeError, "len() only available for collection and array RNA types");
		len = -1; /* error value */
	}
	
	return len;
}

/* internal use only */
static PyObject *prop_subscript_collection_int(BPy_PropertyRNA *self, int keynum)
{
	PointerRNA newptr;

	if(keynum < 0) keynum += RNA_property_collection_length(&self->ptr, self->prop);

	if(RNA_property_collection_lookup_int(&self->ptr, self->prop, keynum, &newptr))
		return pyrna_struct_CreatePyObject(&newptr);

	PyErr_Format(PyExc_IndexError, "index %d out of range", keynum);
	return NULL;
}

static PyObject *prop_subscript_array_int(BPy_PropertyRNA *self, int keynum)
{
	int len= pyrna_prop_array_length(self);

	if(keynum < 0) keynum += len;

	if(keynum >= 0 && keynum < len)
		return pyrna_prop_to_py_index(self, keynum);

	PyErr_Format(PyExc_IndexError, "index %d out of range", keynum);
	return NULL;
}

static PyObject *prop_subscript_collection_str(BPy_PropertyRNA *self, char *keyname)
{
	PointerRNA newptr;
	if(RNA_property_collection_lookup_string(&self->ptr, self->prop, keyname, &newptr))
		return pyrna_struct_CreatePyObject(&newptr);

	PyErr_Format(PyExc_KeyError, "key \"%.200s\" not found", keyname);
	return NULL;
}
/* static PyObject *prop_subscript_array_str(BPy_PropertyRNA *self, char *keyname) */

static PyObject *prop_subscript_collection_slice(PointerRNA *ptr, PropertyRNA *prop, int start, int stop, int length)
{
	PointerRNA newptr;
	PyObject *list = PyList_New(stop - start);
	int count;

	start = MIN2(start,stop); /* values are clamped from  */

	for(count = start; count < stop; count++) {
		if(RNA_property_collection_lookup_int(ptr, prop, count - start, &newptr)) {
			PyList_SET_ITEM(list, count - start, pyrna_struct_CreatePyObject(&newptr));
		}
		else {
			Py_DECREF(list);

			PyErr_SetString(PyExc_RuntimeError, "error getting an rna struct from a collection");
			return NULL;
		}
	}

	return list;
}

/* TODO - dimensions
 * note: could also use pyrna_prop_to_py_index(self, count) in a loop but its a lot slower
 * since at the moment it reads (and even allocates) the entire array for each index.
 */
static PyObject *prop_subscript_array_slice(BPy_PropertyRNA *self, PointerRNA *ptr, PropertyRNA *prop, int start, int stop, int length)
{
	int count, totdim;

	PyObject *list = PyList_New(stop - start);

	totdim = RNA_property_array_dimension(ptr, prop, NULL);

	if (totdim > 1) {
		for (count = start; count < stop; count++)
			PyList_SET_ITEM(list, count - start, pyrna_prop_to_py_index(self, count));
	}
	else {
		switch (RNA_property_type(prop)) {
		case PROP_FLOAT:
			{
				float values_stack[PYRNA_STACK_ARRAY];
				float *values;
				if(length > PYRNA_STACK_ARRAY)	{	values= PyMem_MALLOC(sizeof(float) * length); }
				else							{	values= values_stack; }
				RNA_property_float_get_array(ptr, prop, values);
			
				for(count=start; count<stop; count++)
					PyList_SET_ITEM(list, count-start, PyFloat_FromDouble(values[count]));

				if(values != values_stack) {
					PyMem_FREE(values);
				}
				break;
			}
		case PROP_BOOLEAN:
			{
				int values_stack[PYRNA_STACK_ARRAY];
				int *values;
				if(length > PYRNA_STACK_ARRAY)	{	values= PyMem_MALLOC(sizeof(int) * length); }
				else							{	values= values_stack; }

				RNA_property_boolean_get_array(ptr, prop, values);
				for(count=start; count<stop; count++)
					PyList_SET_ITEM(list, count-start, PyBool_FromLong(values[count]));

				if(values != values_stack) {
					PyMem_FREE(values);
				}
				break;
			}
		case PROP_INT:
			{
				int values_stack[PYRNA_STACK_ARRAY];
				int *values;
				if(length > PYRNA_STACK_ARRAY)	{	values= PyMem_MALLOC(sizeof(int) * length); }
				else							{	values= values_stack; }

				RNA_property_int_get_array(ptr, prop, values);
				for(count=start; count<stop; count++)
					PyList_SET_ITEM(list, count-start, PyLong_FromSsize_t(values[count]));

				if(values != values_stack) {
					PyMem_FREE(values);
				}
				break;
			}
		default:
			/* probably will never happen */
			PyErr_SetString(PyExc_TypeError, "not an array type");
			Py_DECREF(list);
			list= NULL;
		}
	}
	return list;
}

static PyObject *prop_subscript_collection(BPy_PropertyRNA *self, PyObject *key)
{
	if (PyUnicode_Check(key)) {
		return prop_subscript_collection_str(self, _PyUnicode_AsString(key));
	}
	else if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;

		return prop_subscript_collection_int(self, i);
	}
	else if (PySlice_Check(key)) {
		int len= RNA_property_collection_length(&self->ptr, self->prop);
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((PySliceObject*)key, len, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyList_New(0);
		}
		else if (step == 1) {
			return prop_subscript_collection_slice(&self->ptr, self->prop, start, stop, len);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with rna");
			return NULL;
		}
	}
	else {
		PyErr_Format(PyExc_TypeError, "invalid rna key, key must be a string or an int instead of %.200s instance.", Py_TYPE(key)->tp_name);
		return NULL;
	}
}

static PyObject *prop_subscript_array(BPy_PropertyRNA *self, PyObject *key)
{
	/*if (PyUnicode_Check(key)) {
		return prop_subscript_array_str(self, _PyUnicode_AsString(key));
	} else*/
	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return NULL;
		return prop_subscript_array_int(self, PyLong_AsSsize_t(key));
	}
	else if (PySlice_Check(key)) {
		Py_ssize_t start, stop, step, slicelength;
		int len = pyrna_prop_array_length(self);

		if (PySlice_GetIndicesEx((PySliceObject*)key, len, &start, &stop, &step, &slicelength) < 0)
			return NULL;

		if (slicelength <= 0) {
			return PyList_New(0);
		}
		else if (step == 1) {
			return prop_subscript_array_slice(self, &self->ptr, self->prop, start, stop, len);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with rna");
			return NULL;
		}
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "invalid key, key must be an int");
		return NULL;
	}
}

static PyObject *pyrna_prop_subscript( BPy_PropertyRNA *self, PyObject *key )
{
	if (RNA_property_type(self->prop) == PROP_COLLECTION) {
		return prop_subscript_collection(self, key);
	} else if (RNA_property_array_check(&self->ptr, self->prop)) {
		return prop_subscript_array(self, key);
	} 

	PyErr_SetString(PyExc_TypeError, "rna type is not an array or a collection");
	return NULL;
}

/* could call (pyrna_py_to_prop_index(self, i, value) in a loop but it is slow */
static int prop_subscript_ass_array_slice(PointerRNA *ptr, PropertyRNA *prop, int start, int stop, int length, PyObject *value_orig)
{
	PyObject *value;
	int count;
	void *values_alloc= NULL;
	int ret= 0;

	if(value_orig == NULL) {
		PyErr_SetString(PyExc_TypeError, "invalid slice assignment, deleting with list types is not supported by StructRNA.");
		return -1;
	}

	if(!(value=PySequence_Fast(value_orig, "invalid slice assignment, type is not a sequence"))) {
		return -1;
	}

	if(PySequence_Fast_GET_SIZE(value) != stop-start) {
		Py_DECREF(value);
		PyErr_SetString(PyExc_TypeError, "invalid slice assignment, resizing StructRNA arrays isn't supported.");
		return -1;
	}

	switch (RNA_property_type(prop)) {
		case PROP_FLOAT:
		{
			float values_stack[PYRNA_STACK_ARRAY];
			float *values;
			if(length > PYRNA_STACK_ARRAY)	{	values= values_alloc= PyMem_MALLOC(sizeof(float) * length); }
			else							{	values= values_stack; }
			if(start != 0 || stop != length) /* partial assignment? - need to get the array */
				RNA_property_float_get_array(ptr, prop, values);
			
			for(count=start; count<stop; count++)
				values[count] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(value, count-start));

			if(PyErr_Occurred())	ret= -1;
			else					RNA_property_float_set_array(ptr, prop, values);
			break;
		}
		case PROP_BOOLEAN:
		{
			int values_stack[PYRNA_STACK_ARRAY];
			int *values;
			if(length > PYRNA_STACK_ARRAY)	{	values= values_alloc= PyMem_MALLOC(sizeof(int) * length); }
			else							{	values= values_stack; }

			if(start != 0 || stop != length) /* partial assignment? - need to get the array */
				RNA_property_boolean_get_array(ptr, prop, values);
	
			for(count=start; count<stop; count++)
				values[count] = PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, count-start));

			if(PyErr_Occurred())	ret= -1;
			else					RNA_property_boolean_set_array(ptr, prop, values);
			break;
		}
		case PROP_INT:
		{
			int values_stack[PYRNA_STACK_ARRAY];
			int *values;
			if(length > PYRNA_STACK_ARRAY)	{	values= values_alloc= PyMem_MALLOC(sizeof(int) * length); }
			else							{	values= values_stack; }

			if(start != 0 || stop != length) /* partial assignment? - need to get the array */
				RNA_property_int_get_array(ptr, prop, values);

			for(count=start; count<stop; count++)
				values[count] = PyLong_AsSsize_t(PySequence_Fast_GET_ITEM(value, count-start));

			if(PyErr_Occurred())	ret= -1;
			else					RNA_property_int_set_array(ptr, prop, values);
			break;
		}
		default:
			PyErr_SetString(PyExc_TypeError, "not an array type");
			ret= -1;
	}

	Py_DECREF(value);
	
	if(values_alloc) {
		PyMem_FREE(values_alloc);
	}
	
	return ret;

}

static int prop_subscript_ass_array_int(BPy_PropertyRNA *self, int keynum, PyObject *value)
{
	int len= pyrna_prop_array_length(self);

	if(keynum < 0) keynum += len;

	if(keynum >= 0 && keynum < len)
		return pyrna_py_to_prop_index(self, keynum, value);

	PyErr_SetString(PyExc_IndexError, "out of range");
	return -1;
}

static int pyrna_prop_ass_subscript( BPy_PropertyRNA *self, PyObject *key, PyObject *value )
{
	/* char *keyname = NULL; */ /* not supported yet */
	
	if (!RNA_property_editable(&self->ptr, self->prop)) {
		PyErr_Format( PyExc_AttributeError, "PropertyRNA - attribute \"%.200s\" from \"%.200s\" is read-only", RNA_property_identifier(self->prop), RNA_struct_identifier(self->ptr.type) );
		return -1;
	}
	
	/* maybe one day we can support this... */
	if (RNA_property_type(self->prop) == PROP_COLLECTION) {
		PyErr_Format( PyExc_AttributeError, "PropertyRNA - attribute \"%.200s\" from \"%.200s\" is a collection, assignment not supported", RNA_property_identifier(self->prop), RNA_struct_identifier(self->ptr.type) );
		return -1;
	}

	if (PyIndex_Check(key)) {
		Py_ssize_t i = PyNumber_AsSsize_t(key, PyExc_IndexError);
		if (i == -1 && PyErr_Occurred())
			return -1;

		return prop_subscript_ass_array_int(self, i, value);
	}
	else if (PySlice_Check(key)) {
		int len= RNA_property_array_length(&self->ptr, self->prop);
		Py_ssize_t start, stop, step, slicelength;

		if (PySlice_GetIndicesEx((PySliceObject*)key, len, &start, &stop, &step, &slicelength) < 0)
			return -1;

		if (slicelength <= 0) {
			return 0;
		}
		else if (step == 1) {
			return prop_subscript_ass_array_slice(&self->ptr, self->prop, start, stop, len, value);
		}
		else {
			PyErr_SetString(PyExc_TypeError, "slice steps not supported with rna");
			return -1;
		}
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "invalid key, key must be an int");
		return -1;
	}
}


static PyMappingMethods pyrna_prop_as_mapping = {
	( lenfunc ) pyrna_prop_len,	/* mp_length */
	( binaryfunc ) pyrna_prop_subscript,	/* mp_subscript */
	( objobjargproc ) pyrna_prop_ass_subscript,	/* mp_ass_subscript */
};

static int pyrna_prop_contains(BPy_PropertyRNA *self, PyObject *value)
{
	PointerRNA newptr; /* not used, just so RNA_property_collection_lookup_string runs */

	if (RNA_property_type(self->prop) == PROP_COLLECTION) {
		/* key in dict style check */
		char *keyname = _PyUnicode_AsString(value);

		if(keyname==NULL) {
			PyErr_SetString(PyExc_TypeError, "PropertyRNA - key in prop, key must be a string type");
			return -1;
		}


		if (RNA_property_collection_lookup_string(&self->ptr, self->prop, keyname, &newptr))
			return 1;
	}
	else if (RNA_property_array_check(&self->ptr, self->prop)) {
		/* value in list style check */
		return pyrna_array_contains_py(&self->ptr, self->prop, value);
	}
	else {
		PyErr_SetString(PyExc_TypeError, "PropertyRNA - type is not an array or a collection");
		return -1;
	}

	return 0;
}

static int pyrna_struct_contains(BPy_StructRNA *self, PyObject *value)
{
	IDProperty *group;
	char *name = _PyUnicode_AsString(value);

	if (!name) {
		PyErr_SetString( PyExc_TypeError, "expected a string");
		return -1;
	}

	if(RNA_struct_idproperties_check(self->ptr.type)==0) {
		PyErr_SetString( PyExc_TypeError, "this type doesnt support IDProperties");
		return -1;
	}

	group= RNA_struct_idproperties(&self->ptr, 0);
	
	if(!group)
		return 0;
	
	return IDP_GetPropertyFromGroup(group, name) ? 1:0;
}

static PyObject *pyrna_prop_item(BPy_PropertyRNA *self, Py_ssize_t index)
{
	/* reuse subscript functions */
	if (RNA_property_type(self->prop) == PROP_COLLECTION) {
		return prop_subscript_collection_int(self, index);
	} else if (RNA_property_array_check(&self->ptr, self->prop)) {
		return prop_subscript_array_int(self, index);
	}

	PyErr_SetString(PyExc_TypeError, "rna type is not an array or a collection");
	return NULL;
}

static PySequenceMethods pyrna_prop_as_sequence = {
	NULL,		/* Cant set the len otherwise it can evaluate as false */
	NULL,		/* sq_concat */
	NULL,		/* sq_repeat */
	(ssizeargfunc)pyrna_prop_item, /* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,		/* sq_slice */
	NULL,		/* sq_ass_item */
	NULL,		/* sq_ass_slice */
	(objobjproc)pyrna_prop_contains,	/* sq_contains */
};

static PySequenceMethods pyrna_struct_as_sequence = {
	NULL,		/* Cant set the len otherwise it can evaluate as false */
	NULL,		/* sq_concat */
	NULL,		/* sq_repeat */
	NULL,		/* sq_item */ /* Only set this so PySequence_Check() returns True */
	NULL,		/* sq_slice */
	NULL,		/* sq_ass_item */
	NULL,		/* sq_ass_slice */
	(objobjproc)pyrna_struct_contains,	/* sq_contains */
};

static PyObject *pyrna_struct_subscript( BPy_StructRNA *self, PyObject *key )
{
	/* mostly copied from BPy_IDGroup_Map_GetItem */
	IDProperty *group, *idprop;
	char *name= _PyUnicode_AsString(key);

	if(RNA_struct_idproperties_check(self->ptr.type)==0) {
		PyErr_SetString( PyExc_TypeError, "this type doesn't support IDProperties");
		return NULL;
	}

	if(name==NULL) {
		PyErr_SetString( PyExc_TypeError, "only strings are allowed as keys of ID properties");
		return NULL;
	}

	group= RNA_struct_idproperties(&self->ptr, 0);

	if(group==NULL) {
		PyErr_Format( PyExc_KeyError, "key \"%s\" not found", name);
		return NULL;
	}

	idprop= IDP_GetPropertyFromGroup(group, name);

	if(idprop==NULL) {
		PyErr_Format( PyExc_KeyError, "key \"%s\" not found", name);
		return NULL;
	}

	return BPy_IDGroup_WrapData(self->ptr.id.data, idprop);
}

static int pyrna_struct_ass_subscript( BPy_StructRNA *self, PyObject *key, PyObject *value )
{
	IDProperty *group= RNA_struct_idproperties(&self->ptr, 1);

	if(group==NULL) {
		PyErr_SetString(PyExc_TypeError, "id properties not supported for this type");
		return -1;
	}

	return BPy_Wrap_SetMapItem(group, key, value);
}

static PyMappingMethods pyrna_struct_as_mapping = {
	( lenfunc ) NULL,	/* mp_length */
	( binaryfunc ) pyrna_struct_subscript,	/* mp_subscript */
	( objobjargproc ) pyrna_struct_ass_subscript,	/* mp_ass_subscript */
};

static PyObject *pyrna_struct_keys(BPy_PropertyRNA *self)
{
	IDProperty *group;

	if(RNA_struct_idproperties_check(self->ptr.type)==0) {
		PyErr_SetString( PyExc_TypeError, "this type doesnt support IDProperties");
		return NULL;
	}

	group= RNA_struct_idproperties(&self->ptr, 0);

	if(group==NULL)
		return PyList_New(0);

	return BPy_Wrap_GetKeys(group);
}

static PyObject *pyrna_struct_items(BPy_PropertyRNA *self)
{
	IDProperty *group;

	if(RNA_struct_idproperties_check(self->ptr.type)==0) {
		PyErr_SetString( PyExc_TypeError, "this type doesnt support IDProperties");
		return NULL;
	}

	group= RNA_struct_idproperties(&self->ptr, 0);

	if(group==NULL)
		return PyList_New(0);

	return BPy_Wrap_GetItems(self->ptr.id.data, group);
}


static PyObject *pyrna_struct_values(BPy_PropertyRNA *self)
{
	IDProperty *group;

	if(RNA_struct_idproperties_check(self->ptr.type)==0) {
		PyErr_SetString( PyExc_TypeError, "this type doesnt support IDProperties");
		return NULL;
	}

	group= RNA_struct_idproperties(&self->ptr, 0);

	if(group==NULL)
		return PyList_New(0);

	return BPy_Wrap_GetValues(self->ptr.id.data, group);
}

static PyObject *pyrna_struct_keyframe_insert(BPy_StructRNA *self, PyObject *args)
{
	char *path, *path_full;
	int index= -1; /* default to all */
	float cfra = CTX_data_scene(BPy_GetContext())->r.cfra;
	PropertyRNA *prop;
	PyObject *result;

	if (!PyArg_ParseTuple(args, "s|if:keyframe_insert", &path, &index, &cfra))
		return NULL;

	if (self->ptr.data==NULL) {
		PyErr_Format( PyExc_TypeError, "keyframe_insert, this struct has no data, cant be animated", path);
		return NULL;
	}

	prop = RNA_struct_find_property(&self->ptr, path);

	if (prop==NULL) {
		PyErr_Format( PyExc_TypeError, "keyframe_insert, property \"%s\" not found", path);
		return NULL;
	}

	if (!RNA_property_animateable(&self->ptr, prop)) {
		PyErr_Format( PyExc_TypeError, "keyframe_insert, property \"%s\" not animatable", path);
		return NULL;
	}

	path_full= RNA_path_from_ID_to_property(&self->ptr, prop);

	if (path_full==NULL) {
		PyErr_Format( PyExc_TypeError, "keyframe_insert, could not make path to \"%s\"", path);
		return NULL;
	}

	result= PyBool_FromLong( insert_keyframe((ID *)self->ptr.id.data, NULL, NULL, path_full, index, cfra, 0));
	MEM_freeN(path_full);

	return result;
}


static PyObject *pyrna_struct_driver_add(BPy_StructRNA *self, PyObject *args)
{
	char *path, *path_full;
	int index= -1; /* default to all */
	PropertyRNA *prop;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "s|i:driver_add", &path, &index))
		return NULL;

	if (self->ptr.data==NULL) {
		PyErr_Format( PyExc_TypeError, "driver_add, this struct has no data, cant be animated", path);
		return NULL;
	}

	prop = RNA_struct_find_property(&self->ptr, path);

	if (prop==NULL) {
		PyErr_Format( PyExc_TypeError, "driver_add, property \"%s\" not found", path);
		return NULL;
	}

	if (!RNA_property_animateable(&self->ptr, prop)) {
		PyErr_Format( PyExc_TypeError, "driver_add, property \"%s\" not animatable", path);
		return NULL;
	}

	path_full= RNA_path_from_ID_to_property(&self->ptr, prop);

	if (path_full==NULL) {
		PyErr_Format( PyExc_TypeError, "driver_add, could not make path to \"%s\"", path);
		return NULL;
	}

	if(ANIM_add_driver((ID *)self->ptr.id.data, path_full, index, 0, DRIVER_TYPE_PYTHON)) {
		ID *id= self->ptr.id.data;
		AnimData *adt= BKE_animdata_from_id(id);
		FCurve *fcu;

		PointerRNA tptr;
		PyObject *item;

		if(index == -1) { /* all, use a list */
			int i= 0;
			ret= PyList_New(0);
			while((fcu= list_find_fcurve(&adt->drivers, path_full, i++))) {
				RNA_pointer_create(id, &RNA_FCurve, fcu, &tptr);
				item= pyrna_struct_CreatePyObject(&tptr);
				PyList_Append(ret, item);
				Py_DECREF(item);
			}
		}
		else {
			fcu= list_find_fcurve(&adt->drivers, path_full, index);
			RNA_pointer_create(id, &RNA_FCurve, fcu, &tptr);
			ret= pyrna_struct_CreatePyObject(&tptr);
		}
	}
	else {
		ret= Py_None;
		Py_INCREF(ret);
	}

	MEM_freeN(path_full);

	return ret;
}

static PyObject *pyrna_struct_is_property_set(BPy_StructRNA *self, PyObject *args)
{
	char *name;

	if (!PyArg_ParseTuple(args, "s:is_property_set", &name))
		return NULL;

	return PyBool_FromLong(RNA_property_is_set(&self->ptr, name));
}

static PyObject *pyrna_struct_is_property_hidden(BPy_StructRNA *self, PyObject *args)
{
	PropertyRNA *prop;
	char *name;
	int hidden;

	if (!PyArg_ParseTuple(args, "s:is_property_hidden", &name))
		return NULL;
	
	prop= RNA_struct_find_property(&self->ptr, name);
	hidden= (prop)? (RNA_property_flag(prop) & PROP_HIDDEN): 1;

	return PyBool_FromLong(hidden);
}

static PyObject *pyrna_struct_path_resolve(BPy_StructRNA *self, PyObject *value)
{
	char *path= _PyUnicode_AsString(value);
	PointerRNA r_ptr;
	PropertyRNA *r_prop;

	if(path==NULL) {
		PyErr_SetString( PyExc_TypeError, "items() is only valid for collection types" );
		return NULL;
	}

	if (RNA_path_resolve(&self->ptr, path, &r_ptr, &r_prop))
		return pyrna_prop_CreatePyObject(&r_ptr, r_prop);

	Py_RETURN_NONE;
}

static PyObject *pyrna_struct_path_to_id(BPy_StructRNA *self, PyObject *args)
{
	char *name= NULL;
	char *path;
	PropertyRNA *prop;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "|s:path_to_id", &name))
		return NULL;

	if(name) {
		prop= RNA_struct_find_property(&self->ptr, name);
		if(prop==NULL) {
			PyErr_Format(PyExc_TypeError, "path_to_id(\"%.200s\") not found", name);
			return NULL;
		}

		path= RNA_path_from_ID_to_property(&self->ptr, prop);
	}
	else {
		path= RNA_path_from_ID_to_struct(&self->ptr);
	}

	if(path==NULL) {
		if(name)	PyErr_Format(PyExc_TypeError, "%.200s.path_to_id(\"%s\") found but does not support path creation", RNA_struct_identifier(self->ptr.type), name);
		else		PyErr_Format(PyExc_TypeError, "%.200s.path_to_id() does not support path creation for this type", name);
		return NULL;
	}

	ret= PyUnicode_FromString(path);
	MEM_freeN(path);

	return ret;
}

static PyObject *pyrna_struct_recast_type(BPy_StructRNA *self, PyObject *args)
{
	PointerRNA r_ptr;
	RNA_pointer_recast(&self->ptr, &r_ptr);
	return pyrna_struct_CreatePyObject(&r_ptr);
}

static PyObject *pyrna_prop_path_to_id(BPy_PropertyRNA *self)
{
	char *path;
	PropertyRNA *prop = self->prop;
	PyObject *ret;

	path= RNA_path_from_ID_to_property(&self->ptr, self->prop);

	if(path==NULL) {
		PyErr_Format(PyExc_TypeError, "%.200s.%.200s.path_to_id() does not support path creation for this type", RNA_struct_identifier(self->ptr.type), RNA_property_identifier(prop));
		return NULL;
	}

	ret= PyUnicode_FromString(path);
	MEM_freeN(path);

	return ret;
}

static void pyrna_dir_members_py(PyObject *list, PyObject *self)
{
	PyObject *dict;
	PyObject **dict_ptr;
	PyObject *list_tmp;

	dict_ptr= _PyObject_GetDictPtr((PyObject *)self);

	if(dict_ptr && (dict=*dict_ptr)) {
		list_tmp = PyDict_Keys(dict);
		PyList_SetSlice(list, INT_MAX, INT_MAX, list_tmp);
		Py_DECREF(list_tmp);
	}

	dict= ((PyTypeObject *)Py_TYPE(self))->tp_dict;
	if(dict) {
		list_tmp = PyDict_Keys(dict);
		PyList_SetSlice(list, INT_MAX, INT_MAX, list_tmp);
		Py_DECREF(list_tmp);
	}
}

static void pyrna_dir_members_rna(PyObject *list, PointerRNA *ptr)
{
	PyObject *pystring;
	const char *idname;

	/* for looping over attrs and funcs */
	PointerRNA tptr;
	PropertyRNA *iterprop;

	{
		RNA_pointer_create(NULL, &RNA_Struct, ptr->type, &tptr);
		iterprop= RNA_struct_find_property(&tptr, "functions");

		RNA_PROP_BEGIN(&tptr, itemptr, iterprop) {
			idname= RNA_function_identifier(itemptr.data);

			pystring = PyUnicode_FromString(idname);
			PyList_Append(list, pystring);
			Py_DECREF(pystring);
		}
		RNA_PROP_END;
	}

	{
		/*
		 * Collect RNA attributes
		 */
		char name[256], *nameptr;

		iterprop= RNA_struct_iterator_property(ptr->type);

		RNA_PROP_BEGIN(ptr, itemptr, iterprop) {
			nameptr= RNA_struct_name_get_alloc(&itemptr, name, sizeof(name));

			if(nameptr) {
				pystring = PyUnicode_FromString(nameptr);
				PyList_Append(list, pystring);
				Py_DECREF(pystring);

				if(name != nameptr)
					MEM_freeN(nameptr);
			}
		}
		RNA_PROP_END;
	}
}


static PyObject *pyrna_struct_dir(BPy_StructRNA *self)
{
	PyObject *ret;
	PyObject *pystring;

	/* Include this incase this instance is a subtype of a python class
	 * In these instances we may want to return a function or variable provided by the subtype
	 * */
	ret = PyList_New(0);

	if (!BPy_StructRNA_CheckExact(self))
		pyrna_dir_members_py(ret, (PyObject *)self);

	pyrna_dir_members_rna(ret, &self->ptr);

	if(self->ptr.type == &RNA_Context) {
		ListBase lb = CTX_data_dir_get(self->ptr.data);
		LinkData *link;

		for(link=lb.first; link; link=link->next) {
			pystring = PyUnicode_FromString(link->data);
			PyList_Append(ret, pystring);
			Py_DECREF(pystring);
		}

		BLI_freelistN(&lb);
	}
	return ret;
}

//---------------getattr--------------------------------------------
static PyObject *pyrna_struct_getattro( BPy_StructRNA *self, PyObject *pyname )
{
	char *name = _PyUnicode_AsString(pyname);
	PyObject *ret;
	PropertyRNA *prop;
	FunctionRNA *func;
	
	if(name[0]=='_') { // rna can't start with a "_", so for __dict__ and similar we can skip using rna lookups
		/* annoying exception, maybe we need to have different types for this... */
		if((strcmp(name, "__getitem__")==0 || strcmp(name, "__setitem__")==0) && !RNA_struct_idproperties_check(self->ptr.type)) {
			PyErr_SetString(PyExc_AttributeError, "StructRNA - no __getitem__ support for this type");
			ret = NULL;
		}
		else {
			ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
		}
	}
	else if ((prop = RNA_struct_find_property(&self->ptr, name))) {
  		ret = pyrna_prop_to_py(&self->ptr, prop);
  	}
	/* RNA function only if callback is declared (no optional functions) */
	else if ((func = RNA_struct_find_function(&self->ptr, name)) && RNA_function_defined(func)) {
		ret = pyrna_func_to_py((BPy_DummyPointerRNA *)self, func);
	}
	else if (self->ptr.type == &RNA_Context) {
		PointerRNA newptr;
		ListBase newlb;
		int done;

		done= CTX_data_get(self->ptr.data, name, &newptr, &newlb);

		if(done==1) { /* found */
			if (newptr.data) {
				ret = pyrna_struct_CreatePyObject(&newptr);
			}
			else if (newlb.first) {
				CollectionPointerLink *link;
				PyObject *linkptr;

				ret = PyList_New(0);

				for(link=newlb.first; link; link=link->next) {
					linkptr= pyrna_struct_CreatePyObject(&link->ptr);
					PyList_Append(ret, linkptr);
					Py_DECREF(linkptr);
				}
			}
			else {
				ret = Py_None;
				Py_INCREF(ret);
			}
		}
		else if (done==-1) { /* found but not set */
			ret = Py_None;
			Py_INCREF(ret);
		}
        else { /* not found in the context */
        	/* lookup the subclass. raise an error if its not found */
        	ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
        }

		BLI_freelistN(&newlb);
	}
	else {
#if 0
		PyErr_Format( PyExc_AttributeError, "StructRNA - Attribute \"%.200s\" not found", name);
		ret = NULL;
#endif
		/* Include this incase this instance is a subtype of a python class
		 * In these instances we may want to return a function or variable provided by the subtype
		 *
		 * Also needed to return methods when its not a subtype
		 * */

		/* The error raised here will be displayed */
		ret = PyObject_GenericGetAttr((PyObject *)self, pyname);
	}
	
	return ret;
}

#if 0
static int pyrna_struct_pydict_contains(PyObject *self, PyObject *pyname)
{
	 PyObject *dict= *(_PyObject_GetDictPtr((PyObject *)self));
	 if (dict==NULL) /* unlikely */
		 return 0;

	return PyDict_Contains(dict, pyname);
}
#endif

//--------------- setattr-------------------------------------------
static int pyrna_struct_setattro( BPy_StructRNA *self, PyObject *pyname, PyObject *value )
{
	char *name = _PyUnicode_AsString(pyname);
	PropertyRNA *prop = RNA_struct_find_property(&self->ptr, name);
	
	if (prop==NULL) {
		return PyObject_GenericSetAttr((PyObject *)self, pyname, value);
#if 0
		// XXX - This currently allows anything to be assigned to an rna prop, need to see how this should be used
		// but for now it makes porting scripts confusing since it fails silently.
		// edit: allowing this for setting classes internal attributes.
		// edit: allow this for any attribute that alredy exists as a python attr
		if (	(name[0]=='_' /* || pyrna_struct_pydict_contains(self, pyname) */ ) &&
				!BPy_StructRNA_CheckExact(self) &&

			return 0;
		} else
		{
			PyErr_Format( PyExc_AttributeError, "StructRNA - Attribute \"%.200s\" not found", name);
			return -1;
		}
#endif
	}		
	
	if (!RNA_property_editable(&self->ptr, prop)) {
		PyErr_Format( PyExc_AttributeError, "StructRNA - Attribute \"%.200s\" from \"%.200s\" is read-only", RNA_property_identifier(prop), RNA_struct_identifier(self->ptr.type) );
		return -1;
	}
		
	/* pyrna_py_to_prop sets its own exceptions */
	return pyrna_py_to_prop(&self->ptr, prop, NULL, NULL, value, "StructRNA - item.attr = val:");
}

static PyObject *pyrna_prop_dir(BPy_PropertyRNA *self)
{
	PyObject *ret;
	PointerRNA r_ptr;

	/* Include this incase this instance is a subtype of a python class
	 * In these instances we may want to return a function or variable provided by the subtype
	 * */
	ret = PyList_New(0);

	if (!BPy_PropertyRNA_CheckExact(self))
		pyrna_dir_members_py(ret, (PyObject *)self);

	if(RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr))
		pyrna_dir_members_rna(ret, &r_ptr);

	return ret;
}


static PyObject *pyrna_prop_getattro( BPy_PropertyRNA *self, PyObject *pyname )
{
	char *name = _PyUnicode_AsString(pyname);

	if(name[0] != '_') {
		if (RNA_property_type(self->prop) == PROP_COLLECTION) {
			PyObject *ret;
			PropertyRNA *prop;
			FunctionRNA *func;

			PointerRNA r_ptr;
			if(RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
				if ((prop = RNA_struct_find_property(&r_ptr, name))) {
					ret = pyrna_prop_to_py(&r_ptr, prop);

					return ret;
				}
				else if ((func = RNA_struct_find_function(&r_ptr, name))) {
					PyObject *self_collection= pyrna_struct_CreatePyObject(&r_ptr);
					ret = pyrna_func_to_py((BPy_DummyPointerRNA *)self_collection, func);
					Py_DECREF(self_collection);

					return ret;
				}
			}
		}
	}
	else {
		/* annoying exception, maybe we need to have different types for this... */
		if((strcmp(name, "__getitem__")==0 || strcmp(name, "__setitem__")==0) &&
				(RNA_property_type(self->prop) != PROP_COLLECTION) &&
				RNA_property_array_check(&self->ptr, self->prop)==0
		) {
			PyErr_SetString(PyExc_AttributeError, "PropertyRNA - no __getitem__ support for this type");
			return NULL;
		}
	}

	/* The error raised here will be displayed */
	return PyObject_GenericGetAttr((PyObject *)self, pyname);
}

//--------------- setattr-------------------------------------------
static int pyrna_prop_setattro( BPy_PropertyRNA *self, PyObject *pyname, PyObject *value )
{
	char *name = _PyUnicode_AsString(pyname);
	PropertyRNA *prop;

	if (RNA_property_type(self->prop) == PROP_COLLECTION) {
		PointerRNA r_ptr;
		if(RNA_property_collection_type_get(&self->ptr, self->prop, &r_ptr)) {
			if ((prop = RNA_struct_find_property(&r_ptr, name))) {
				/* pyrna_py_to_prop sets its own exceptions */
				return pyrna_py_to_prop(&r_ptr, prop, NULL, NULL, value, "BPy_PropertyRNA - Attribute (setattr):");
			}
		}
	}

	PyErr_Format( PyExc_AttributeError, "BPy_PropertyRNA - Attribute \"%.200s\" not found", name);
	return -1;
}

/* odd case, we need to be able return a python method from a tp_getset */
static PyObject *pyrna_prop_add(BPy_PropertyRNA *self)
{
	PointerRNA r_ptr;

	RNA_property_collection_add(&self->ptr, self->prop, &r_ptr);
	if(!r_ptr.data) {
		PyErr_SetString( PyExc_TypeError, "add() not supported for this collection");
		return NULL;
	}
	else {
		return pyrna_struct_CreatePyObject(&r_ptr);
	}
}

static PyObject *pyrna_prop_remove(BPy_PropertyRNA *self, PyObject *value)
{
	PyObject *ret;
	int key= PyLong_AsSsize_t(value);

	if (key==-1 && PyErr_Occurred()) {
		PyErr_SetString( PyExc_TypeError, "remove() expected one int argument");
		return NULL;
	}

	if(!RNA_property_collection_remove(&self->ptr, self->prop, key)) {
		PyErr_SetString( PyExc_TypeError, "remove() not supported for this collection");
		return NULL;
	}

	ret = Py_None;
	Py_INCREF(ret);

	return ret;
}

static PyObject *pyrna_struct_get_id_data(BPy_StructRNA *self)
{
	if(self->ptr.id.data) {
		PointerRNA id_ptr;
		RNA_id_pointer_create((ID *)self->ptr.id.data, &id_ptr);
		return pyrna_struct_CreatePyObject(&id_ptr);
	}

	Py_RETURN_NONE;
}

/*****************************************************************************/
/* Python attributes get/set structure:                                      */
/*****************************************************************************/
#if 0
static PyGetSetDef pyrna_prop_getseters[] = {
	{"active", (getter)pyrna_prop_get_active, (setter)pyrna_prop_set_active, "", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};
#endif

static PyGetSetDef pyrna_struct_getseters[] = {
	{"id_data", (getter)pyrna_struct_get_id_data, (setter)NULL, "The ID data this datablock is from, (not available for all data)", NULL},
	{NULL,NULL,NULL,NULL,NULL}  /* Sentinel */
};

static PyObject *pyrna_prop_keys(BPy_PropertyRNA *self)
{
	PyObject *ret;
	if (RNA_property_type(self->prop) != PROP_COLLECTION) {
		PyErr_SetString( PyExc_TypeError, "keys() is only valid for collection types" );
		ret = NULL;
	} else {
		PyObject *item;
		char name[256], *nameptr;

		ret = PyList_New(0);
		
		RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
			nameptr= RNA_struct_name_get_alloc(&itemptr, name, sizeof(name));

			if(nameptr) {
				/* add to python list */
				item = PyUnicode_FromString( nameptr );
				PyList_Append(ret, item);
				Py_DECREF(item);
				/* done */
				
				if(name != nameptr)
					MEM_freeN(nameptr);
			}
		}
		RNA_PROP_END;
	}
	
	return ret;
}

static PyObject *pyrna_prop_items(BPy_PropertyRNA *self)
{
	PyObject *ret;
	if (RNA_property_type(self->prop) != PROP_COLLECTION) {
		PyErr_SetString( PyExc_TypeError, "items() is only valid for collection types" );
		ret = NULL;
	} else {
		PyObject *item;
		char name[256], *nameptr;
		int i= 0;

		ret = PyList_New(0);
		
		RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
			if(itemptr.data) {
				/* add to python list */
				item= PyTuple_New(2);
				nameptr= RNA_struct_name_get_alloc(&itemptr, name, sizeof(name));
				if(nameptr) {
					PyTuple_SET_ITEM(item, 0, PyUnicode_FromString( nameptr ));
					if(name != nameptr)
						MEM_freeN(nameptr);
				}
				else {
					PyTuple_SET_ITEM(item, 0, PyLong_FromSsize_t(i)); /* a bit strange but better then returning an empty list */
				}
				PyTuple_SET_ITEM(item, 1, pyrna_struct_CreatePyObject(&itemptr));
				
				PyList_Append(ret, item);
				Py_DECREF(item);
				
				i++;
			}
		}
		RNA_PROP_END;
	}
	
	return ret;
}


static PyObject *pyrna_prop_values(BPy_PropertyRNA *self)
{
	PyObject *ret;
	
	if (RNA_property_type(self->prop) != PROP_COLLECTION) {
		PyErr_SetString( PyExc_TypeError, "values() is only valid for collection types" );
		ret = NULL;
	} else {
		PyObject *item;
		ret = PyList_New(0);
		
		RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
			item = pyrna_struct_CreatePyObject(&itemptr);
			PyList_Append(ret, item);
			Py_DECREF(item);
		}
		RNA_PROP_END;
	}
	
	return ret;
}

static PyObject *pyrna_struct_get(BPy_StructRNA *self, PyObject *args)
{
	IDProperty *group, *idprop;

	char *key;
	PyObject* def = Py_None;

	if (!PyArg_ParseTuple(args, "s|O:get", &key, &def))
		return NULL;

	/* mostly copied from BPy_IDGroup_Map_GetItem */
	if(RNA_struct_idproperties_check(self->ptr.type)==0) {
		PyErr_SetString( PyExc_TypeError, "this type doesn't support IDProperties");
		return NULL;
	}

	group= RNA_struct_idproperties(&self->ptr, 0);
	if(group) {
		idprop= IDP_GetPropertyFromGroup(group, key);

		if(idprop)
			return BPy_IDGroup_WrapData(self->ptr.id.data, idprop);
	}

	Py_INCREF(def);
	return def;
}

static PyObject *pyrna_prop_get(BPy_PropertyRNA *self, PyObject *args)
{
	PointerRNA newptr;
	
	char *key;
	PyObject* def = Py_None;

	if (!PyArg_ParseTuple(args, "s|O:get", &key, &def))
		return NULL;
	
	if(RNA_property_collection_lookup_string(&self->ptr, self->prop, key, &newptr))
		return pyrna_struct_CreatePyObject(&newptr);
	
	Py_INCREF(def);
	return def;
}

static void foreach_attr_type(	BPy_PropertyRNA *self, char *attr,
									/* values to assign */
									RawPropertyType *raw_type, int *attr_tot, int *attr_signed )
{
	PropertyRNA *prop;
	*raw_type= PROP_RAW_UNSET;
	*attr_tot= 0;
	*attr_signed= FALSE;

	/* note: this is fail with zero length lists, so dont let this get caled in that case */
	RNA_PROP_BEGIN(&self->ptr, itemptr, self->prop) {
		prop = RNA_struct_find_property(&itemptr, attr);
		*raw_type= RNA_property_raw_type(prop);
		*attr_tot = RNA_property_array_length(&itemptr, prop);
		*attr_signed= (RNA_property_subtype(prop)==PROP_UNSIGNED) ? FALSE:TRUE;
		break;
	}
	RNA_PROP_END;
}

/* pyrna_prop_foreach_get/set both use this */
static int foreach_parse_args(
		BPy_PropertyRNA *self, PyObject *args,

		/*values to assign */
		char **attr, PyObject **seq, int *tot, int *size, RawPropertyType *raw_type, int *attr_tot, int *attr_signed)
{
#if 0
	int array_tot;
	int target_tot;
#endif

	*size= *attr_tot= *attr_signed= FALSE;
	*raw_type= PROP_RAW_UNSET;

	if(!PyArg_ParseTuple(args, "sO", attr, seq) || (!PySequence_Check(*seq) && PyObject_CheckBuffer(*seq))) {
		PyErr_SetString( PyExc_TypeError, "foreach_get(attr, sequence) expects a string and a sequence" );
		return -1;
	}

	*tot= PySequence_Length(*seq); // TODO - buffer may not be a sequence! array.array() is tho.

	if(*tot>0) {
		foreach_attr_type(self, *attr, raw_type, attr_tot, attr_signed);
		*size= RNA_raw_type_sizeof(*raw_type);

#if 0	// works fine but not strictly needed, we could allow RNA_property_collection_raw_* to do the checks
		if((*attr_tot) < 1)
			*attr_tot= 1;

		if (RNA_property_type(self->prop) == PROP_COLLECTION)
			array_tot = RNA_property_collection_length(&self->ptr, self->prop);
		else
			array_tot = RNA_property_array_length(&self->ptr, self->prop);


		target_tot= array_tot * (*attr_tot);

		/* rna_access.c - rna_raw_access(...) uses this same method */
		if(target_tot != (*tot)) {
			PyErr_Format( PyExc_TypeError, "foreach_get(attr, sequence) sequence length mismatch given %d, needed %d", *tot, target_tot);
			return -1;
		}
#endif
	}

	/* check 'attr_tot' otherwise we dont know if any values were set
	 * this isnt ideal because it means running on an empty list may fail silently when its not compatible. */
	if (*size == 0 && *attr_tot != 0) {
		PyErr_SetString( PyExc_AttributeError, "attribute does not support foreach method" );
		return -1;
	}
	return 0;
}

static int foreach_compat_buffer(RawPropertyType raw_type, int attr_signed, const char *format)
{
	char f = format ? *format:'B'; /* B is assumed when not set */

	switch(raw_type) {
	case PROP_RAW_CHAR:
		if (attr_signed)	return (f=='b') ? 1:0;
		else				return (f=='B') ? 1:0;
	case PROP_RAW_SHORT:
		if (attr_signed)	return (f=='h') ? 1:0;
		else				return (f=='H') ? 1:0;
	case PROP_RAW_INT:
		if (attr_signed)	return (f=='i') ? 1:0;
		else				return (f=='I') ? 1:0;
	case PROP_RAW_FLOAT:
		return (f=='f') ? 1:0;
	case PROP_RAW_DOUBLE:
		return (f=='d') ? 1:0;
	case PROP_RAW_UNSET:
		return 0;
	}

	return 0;
}

static PyObject *foreach_getset(BPy_PropertyRNA *self, PyObject *args, int set)
{
	PyObject *item = NULL;
	int i=0, ok=0, buffer_is_compat;
	void *array= NULL;

	/* get/set both take the same args currently */
	char *attr;
	PyObject *seq;
	int tot, size, attr_tot, attr_signed;
	RawPropertyType raw_type;

	if(foreach_parse_args(self, args,    &attr, &seq, &tot, &size, &raw_type, &attr_tot, &attr_signed) < 0)
		return NULL;

	if(tot==0)
		Py_RETURN_NONE;



	if(set) { /* get the array from python */
		buffer_is_compat = FALSE;
		if(PyObject_CheckBuffer(seq)) {
			Py_buffer buf;
			PyObject_GetBuffer(seq, &buf, PyBUF_SIMPLE | PyBUF_FORMAT);

			/* check if the buffer matches */

			buffer_is_compat = foreach_compat_buffer(raw_type, attr_signed, buf.format);

			if(buffer_is_compat) {
				ok = RNA_property_collection_raw_set(NULL, &self->ptr, self->prop, attr, buf.buf, raw_type, tot);
			}

			PyBuffer_Release(&buf);
		}

		/* could not use the buffer, fallback to sequence */
		if(!buffer_is_compat) {
			array= PyMem_Malloc(size * tot);

			for( ; i<tot; i++) {
				item= PySequence_GetItem(seq, i);
				switch(raw_type) {
				case PROP_RAW_CHAR:
					((char *)array)[i]= (char)PyLong_AsSsize_t(item);
					break;
				case PROP_RAW_SHORT:
					((short *)array)[i]= (short)PyLong_AsSsize_t(item);
					break;
				case PROP_RAW_INT:
					((int *)array)[i]= (int)PyLong_AsSsize_t(item);
					break;
				case PROP_RAW_FLOAT:
					((float *)array)[i]= (float)PyFloat_AsDouble(item);
					break;
				case PROP_RAW_DOUBLE:
					((double *)array)[i]= (double)PyFloat_AsDouble(item);
					break;
				case PROP_RAW_UNSET:
					/* should never happen */
					break;
				}

				Py_DECREF(item);
			}

			ok = RNA_property_collection_raw_set(NULL, &self->ptr, self->prop, attr, array, raw_type, tot);
		}
	}
	else {
		buffer_is_compat = FALSE;
		if(PyObject_CheckBuffer(seq)) {
			Py_buffer buf;
			PyObject_GetBuffer(seq, &buf, PyBUF_SIMPLE | PyBUF_FORMAT);

			/* check if the buffer matches, TODO - signed/unsigned types */

			buffer_is_compat = foreach_compat_buffer(raw_type, attr_signed, buf.format);

			if(buffer_is_compat) {
				ok = RNA_property_collection_raw_get(NULL, &self->ptr, self->prop, attr, buf.buf, raw_type, tot);
			}

			PyBuffer_Release(&buf);
		}

		/* could not use the buffer, fallback to sequence */
		if(!buffer_is_compat) {
			array= PyMem_Malloc(size * tot);

			ok = RNA_property_collection_raw_get(NULL, &self->ptr, self->prop, attr, array, raw_type, tot);

			if(!ok) i= tot; /* skip the loop */

			for( ; i<tot; i++) {

				switch(raw_type) {
				case PROP_RAW_CHAR:
					item= PyLong_FromSsize_t(  (Py_ssize_t) ((char *)array)[i]  );
					break;
				case PROP_RAW_SHORT:
					item= PyLong_FromSsize_t(  (Py_ssize_t) ((short *)array)[i]  );
					break;
				case PROP_RAW_INT:
					item= PyLong_FromSsize_t(  (Py_ssize_t) ((int *)array)[i]  );
					break;
				case PROP_RAW_FLOAT:
					item= PyFloat_FromDouble(  (double) ((float *)array)[i]  );
					break;
				case PROP_RAW_DOUBLE:
					item= PyFloat_FromDouble(  (double) ((double *)array)[i]  );
					break;
				case PROP_RAW_UNSET:
					/* should never happen */
					break;
				}

				PySequence_SetItem(seq, i, item);
				Py_DECREF(item);
			}
		}
	}

	if(array)
		PyMem_Free(array);

	if(PyErr_Occurred()) {
		/* Maybe we could make our own error */
		PyErr_Print();
		PyErr_SetString(PyExc_SystemError, "could not access the py sequence");
		return NULL;
	}
	if (!ok) {
		PyErr_SetString(PyExc_SystemError, "internal error setting the array");
		return NULL;
	}

	Py_RETURN_NONE;
}

static PyObject *pyrna_prop_foreach_get(BPy_PropertyRNA *self, PyObject *args)
{
	return foreach_getset(self, args, 0);
}

static  PyObject *pyrna_prop_foreach_set(BPy_PropertyRNA *self, PyObject *args)
{
	return foreach_getset(self, args, 1);
}

/* A bit of a kludge, make a list out of a collection or array,
 * then return the lists iter function, not especially fast but convenient for now */
PyObject *pyrna_prop_iter(BPy_PropertyRNA *self)
{
	/* Try get values from a collection */
	PyObject *ret;
	PyObject *iter;
	
	if(RNA_property_array_check(&self->ptr, self->prop)) {
		int len= pyrna_prop_array_length(self);
		ret = prop_subscript_array_slice(self, &self->ptr, self->prop, 0, len, len);
	}
	else if ((ret = pyrna_prop_values(self))) {
		/* do nothing */
	}
	else {
		PyErr_SetString( PyExc_TypeError, "this BPy_PropertyRNA object is not iterable" );
		return NULL;
	}
	
	
	/* we know this is a list so no need to PyIter_Check */
	iter = PyObject_GetIter(ret);
	Py_DECREF(ret);
	return iter;
}

static struct PyMethodDef pyrna_struct_methods[] = {

	/* only for PointerRNA's with ID'props */
	{"keys", (PyCFunction)pyrna_struct_keys, METH_NOARGS, NULL},
	{"values", (PyCFunction)pyrna_struct_values, METH_NOARGS, NULL},
	{"items", (PyCFunction)pyrna_struct_items, METH_NOARGS, NULL},

	{"get", (PyCFunction)pyrna_struct_get, METH_VARARGS, NULL},

	/* maybe this become and ID function */
	{"keyframe_insert", (PyCFunction)pyrna_struct_keyframe_insert, METH_VARARGS, NULL},
	{"driver_add", (PyCFunction)pyrna_struct_driver_add, METH_VARARGS, NULL},
	{"is_property_set", (PyCFunction)pyrna_struct_is_property_set, METH_VARARGS, NULL},
	{"is_property_hidden", (PyCFunction)pyrna_struct_is_property_hidden, METH_VARARGS, NULL},
	{"path_resolve", (PyCFunction)pyrna_struct_path_resolve, METH_O, NULL},
	{"path_to_id", (PyCFunction)pyrna_struct_path_to_id, METH_VARARGS, NULL},
	{"recast_type", (PyCFunction)pyrna_struct_recast_type, METH_NOARGS, NULL},
	{"__dir__", (PyCFunction)pyrna_struct_dir, METH_NOARGS, NULL},
	{NULL, NULL, 0, NULL}
};

static struct PyMethodDef pyrna_prop_methods[] = {
	{"keys", (PyCFunction)pyrna_prop_keys, METH_NOARGS, NULL},
	{"items", (PyCFunction)pyrna_prop_items, METH_NOARGS,NULL},
	{"values", (PyCFunction)pyrna_prop_values, METH_NOARGS, NULL},
	
	{"get", (PyCFunction)pyrna_prop_get, METH_VARARGS, NULL},

	/* moved into a getset */
	{"add", (PyCFunction)pyrna_prop_add, METH_NOARGS, NULL},
	{"remove", (PyCFunction)pyrna_prop_remove, METH_O, NULL},

	/* almost the same as the srna function */
	{"path_to_id", (PyCFunction)pyrna_prop_path_to_id, METH_NOARGS, NULL},

	/* array accessor function */
	{"foreach_get", (PyCFunction)pyrna_prop_foreach_get, METH_VARARGS, NULL},
	{"foreach_set", (PyCFunction)pyrna_prop_foreach_set, METH_VARARGS, NULL},
	{"__dir__", (PyCFunction)pyrna_prop_dir, METH_NOARGS, NULL},
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

PyObject *pyrna_param_to_py(PointerRNA *ptr, ParameterList *parms, PropertyRNA *prop, void *data)
{
	PyObject *ret;
	int type = RNA_property_type(prop);
	int flag = RNA_property_flag(prop);
	int a;

	if(RNA_property_array_check(ptr, prop)) {
		int len;

		if (flag & PROP_DYNAMIC) {
			len= RNA_parameter_length_get_data(parms, prop, data);

			data= *((void **)data);
		}
		else
			len= RNA_property_array_length(ptr, prop);

		/* resolve the array from a new pytype */

		/* kazanbas: TODO make multidim sequences here */

		switch (type) {
		case PROP_BOOLEAN:
			ret = PyTuple_New(len);
			for(a=0; a<len; a++)
				PyTuple_SET_ITEM(ret, a, PyBool_FromLong( ((int*)data)[a] ));
			break;
		case PROP_INT:
			ret = PyTuple_New(len);
			for(a=0; a<len; a++)
				PyTuple_SET_ITEM(ret, a, PyLong_FromSsize_t( (Py_ssize_t)((int*)data)[a] ));
			break;
		case PROP_FLOAT:
			switch(RNA_property_subtype(prop)) {
				case PROP_ALL_VECTOR_SUBTYPES:
					ret= newVectorObject(data, len, Py_NEW, NULL);
					break;
				case PROP_MATRIX:
					if(len==16) {
						ret= newMatrixObject(data, 4, 4, Py_NEW, NULL);
						break;
					}
					else if (len==9) {
						ret= newMatrixObject(data, 3, 3, Py_NEW, NULL);
						break;
					}
					/* pass through */
				default:
					ret = PyTuple_New(len);
					for(a=0; a<len; a++)
						PyTuple_SET_ITEM(ret, a, PyFloat_FromDouble( ((float*)data)[a] ));

			}
			break;
		default:
			PyErr_Format(PyExc_TypeError, "RNA Error: unknown array type \"%d\" (pyrna_param_to_py)", type);
			ret = NULL;
			break;
		}
	}
	else {
		/* see if we can coorce into a python type - PropertyType */
		switch (type) {
		case PROP_BOOLEAN:
			ret = PyBool_FromLong( *(int*)data );
			break;
		case PROP_INT:
			ret = PyLong_FromSsize_t( (Py_ssize_t)*(int*)data );
			break;
		case PROP_FLOAT:
			ret = PyFloat_FromDouble( *(float*)data );
			break;
		case PROP_STRING:
		{
			if(flag & PROP_THICK_WRAP)
				ret = PyUnicode_FromString( (char*)data );
			else
				ret = PyUnicode_FromString( *(char**)data );
			break;
		}
		case PROP_ENUM:
		{
			ret= pyrna_enum_to_py(ptr, prop, *(int*)data);
			break;
		}
		case PROP_POINTER:
		{
			PointerRNA newptr;
			StructRNA *type= RNA_property_pointer_type(ptr, prop);

			if(flag & PROP_RNAPTR) {
				/* in this case we get the full ptr */
				newptr= *(PointerRNA*)data;
			}
			else {
				if(RNA_struct_is_ID(type)) {
					RNA_id_pointer_create(*(void**)data, &newptr);
				} else {
					/* note: this is taken from the function's ID pointer
					 * and will break if a function returns a pointer from
					 * another ID block, watch this! - it should at least be
					 * easy to debug since they are all ID's */
					RNA_pointer_create(ptr->id.data, type, *(void**)data, &newptr);
				}
			}

			if (newptr.data) {
				ret = pyrna_struct_CreatePyObject(&newptr);
			} else {
				ret = Py_None;
				Py_INCREF(ret);
			}
			break;
		}
		case PROP_COLLECTION:
		{
			ListBase *lb= (ListBase*)data;
			CollectionPointerLink *link;
			PyObject *linkptr;

			ret = PyList_New(0);

			for(link=lb->first; link; link=link->next) {
				linkptr= pyrna_struct_CreatePyObject(&link->ptr);
				PyList_Append(ret, linkptr);
				Py_DECREF(linkptr);
			}

			break;
		}
		default:
			PyErr_Format(PyExc_TypeError, "RNA Error: unknown type \"%d\" (pyrna_param_to_py)", type);
			ret = NULL;
			break;
		}
	}

	return ret;
}

static PyObject * pyrna_func_call(PyObject *self, PyObject *args, PyObject *kw)
{
	/* Note, both BPy_StructRNA and BPy_PropertyRNA can be used here */
	PointerRNA *self_ptr= &(((BPy_DummyPointerRNA *)PyTuple_GET_ITEM(self, 0))->ptr);
	FunctionRNA *self_func=  PyCObject_AsVoidPtr(PyTuple_GET_ITEM(self, 1));

	PointerRNA funcptr;
	ParameterList parms;
	ParameterIterator iter;
	PropertyRNA *parm;
	PyObject *ret, *item;
	int i, args_len, parms_len, ret_len, flag, err= 0, kw_tot= 0, kw_arg;
	const char *parm_id;

	PropertyRNA *pret_single= NULL;
	void *retdata_single= NULL;

	/* Should never happen but it does in rare cases */
	if(self_ptr==NULL) {
		PyErr_SetString(PyExc_RuntimeError, "rna functions internal rna pointer is NULL, this is a bug. aborting");
		return NULL;
	}
	
	if(self_func==NULL) {
		PyErr_Format(PyExc_RuntimeError, "%.200s.<unknown>(): rna function internal function is NULL, this is a bug. aborting", RNA_struct_identifier(self_ptr->type));
		return NULL;
	}
	
	/* include the ID pointer for pyrna_param_to_py() so we can include the
	 * ID pointer on return values, this only works when returned values have
	 * the same ID as the functions. */
	RNA_pointer_create(self_ptr->id.data, &RNA_Function, self_func, &funcptr);

	args_len= PyTuple_GET_SIZE(args);

	RNA_parameter_list_create(&parms, self_ptr, self_func);
	RNA_parameter_list_begin(&parms, &iter);
	parms_len= RNA_parameter_list_size(&parms);
	ret_len= 0;

	if(args_len + (kw ? PyDict_Size(kw):0) > parms_len) {
		RNA_parameter_list_end(&iter);
		PyErr_Format(PyExc_TypeError, "%.200s.%.200s(): takes at most %d arguments, got %d", RNA_struct_identifier(self_ptr->type), RNA_function_identifier(self_func), parms_len, args_len);
		err= -1;
	}

	/* parse function parameters */
	for (i= 0; iter.valid && err==0; RNA_parameter_list_next(&iter)) {
		parm= iter.parm;
		flag= RNA_property_flag(parm);

		/* only useful for single argument returns, we'll need another list loop for multiple */
		if (flag & PROP_OUTPUT) {
			ret_len++;
			if (pret_single==NULL) {
				pret_single= parm;
				retdata_single= iter.data;
			}

			continue;
		}

		parm_id= RNA_property_identifier(parm);
		item= NULL;

		if ((i < args_len) && (flag & PROP_REQUIRED)) {
			item= PyTuple_GET_ITEM(args, i);
			i++;

			kw_arg= FALSE;
		}
		else if (kw != NULL) {
			item= PyDict_GetItemString(kw, parm_id);  /* borrow ref */
			if(item)
				kw_tot++; /* make sure invalid keywords are not given */

			kw_arg= TRUE;
		}

		if (item==NULL) {
			if(flag & PROP_REQUIRED) {
				PyErr_Format(PyExc_TypeError, "%.200s.%.200s(): required parameter \"%.200s\" not specified", RNA_struct_identifier(self_ptr->type), RNA_function_identifier(self_func), parm_id);
				err= -1;
				break;
			}
			else /* PyDict_GetItemString wont raise an error */
				continue;
		}

		err= pyrna_py_to_prop(&funcptr, parm, &parms, iter.data, item, "");

		if(err!=0) {
			/* the error generated isnt that useful, so generate it again with a useful prefix
			 * could also write a function to prepend to error messages */
			char error_prefix[512];
			PyErr_Clear(); /* re-raise */

			if(kw_arg==TRUE)
				snprintf(error_prefix, sizeof(error_prefix), "%s.%s(): error with keyword argument \"%s\" - ", RNA_struct_identifier(self_ptr->type), RNA_function_identifier(self_func), parm_id);
			else
				snprintf(error_prefix, sizeof(error_prefix), "%s.%s(): error with argument %d, \"%s\" - ", RNA_struct_identifier(self_ptr->type), RNA_function_identifier(self_func), i, parm_id);

			pyrna_py_to_prop(&funcptr, parm, &parms, iter.data, item, error_prefix);

			break;
		}
	}
	
	RNA_parameter_list_end(&iter);


	/* Check if we gave args that dont exist in the function
	 * printing the error is slow but it should only happen when developing.
	 * the if below is quick, checking if it passed less keyword args then we gave.
	 * (Dont overwrite the error if we have one, otherwise can skip important messages and confuse with args)
	 */
	if(err == 0 && kw && (PyDict_Size(kw) > kw_tot)) {
		PyObject *key, *value;
		Py_ssize_t pos = 0;

		DynStr *bad_args= BLI_dynstr_new();
		DynStr *good_args= BLI_dynstr_new();

		char *arg_name, *bad_args_str, *good_args_str;
		int found= FALSE, first= TRUE;

		while (PyDict_Next(kw, &pos, &key, &value)) {

			arg_name= _PyUnicode_AsString(key);
			found= FALSE;

			if(arg_name==NULL) { /* unlikely the argname is not a string but ignore if it is*/
				PyErr_Clear();
			}
			else {
				/* Search for arg_name */
				RNA_parameter_list_begin(&parms, &iter);
				for(; iter.valid; RNA_parameter_list_next(&iter)) {
					parm= iter.parm;
					if (strcmp(arg_name, RNA_property_identifier(parm))==0) {
						found= TRUE;
						break;
					}
				}

				RNA_parameter_list_end(&iter);

				if(found==FALSE) {
					BLI_dynstr_appendf(bad_args, first ? "%s" : ", %s", arg_name);
					first= FALSE;
				}
			}
		}

		/* list good args */
		first= TRUE;

		RNA_parameter_list_begin(&parms, &iter);
		for(; iter.valid; RNA_parameter_list_next(&iter)) {
			parm= iter.parm;
			if(RNA_property_flag(parm) & PROP_OUTPUT)
				continue;

			BLI_dynstr_appendf(good_args, first ? "%s" : ", %s", RNA_property_identifier(parm));
			first= FALSE;
		}
		RNA_parameter_list_end(&iter);


		bad_args_str= BLI_dynstr_get_cstring(bad_args);
		good_args_str= BLI_dynstr_get_cstring(good_args);

		PyErr_Format(PyExc_TypeError, "%.200s.%.200s(): was called with invalid keyword arguments(s) (%s), expected (%s)", RNA_struct_identifier(self_ptr->type), RNA_function_identifier(self_func), bad_args_str, good_args_str);

		BLI_dynstr_free(bad_args);
		BLI_dynstr_free(good_args);
		MEM_freeN(bad_args_str);
		MEM_freeN(good_args_str);

		err= -1;
	}

	ret= NULL;
	if (err==0) {
		/* call function */
		ReportList reports;
		bContext *C= BPy_GetContext();

		BKE_reports_init(&reports, RPT_STORE);
		RNA_function_call(C, &reports, self_ptr, self_func, &parms);

		err= (BPy_reports_to_error(&reports))? -1: 0;
		BKE_reports_clear(&reports);

		/* return value */
		if(err==0) {
			if (ret_len > 0) {
				if (ret_len > 1) {
					ret= PyTuple_New(ret_len);
					i= 0; /* arg index */

					RNA_parameter_list_begin(&parms, &iter);

					for(; iter.valid; RNA_parameter_list_next(&iter)) {
						parm= iter.parm;
						flag= RNA_property_flag(parm);

						if (flag & PROP_OUTPUT)
							PyTuple_SET_ITEM(ret, i++, pyrna_param_to_py(&funcptr, &parms, parm, iter.data));
					}

					RNA_parameter_list_end(&iter);
				}
				else
					ret= pyrna_param_to_py(&funcptr, &parms, pret_single, retdata_single);

				/* possible there is an error in conversion */
				if(ret==NULL)
					err= -1;
			}
		}
	}

	/* cleanup */
	RNA_parameter_list_end(&iter);
	RNA_parameter_list_free(&parms);

	if (ret)
		return ret;

	if (err==-1)
		return NULL;

	Py_RETURN_NONE;
}

/*-----------------------BPy_StructRNA method def------------------------------*/
PyTypeObject pyrna_struct_Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
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
	&pyrna_struct_as_sequence,	/* PySequenceMethods *tp_as_sequence; */
	&pyrna_struct_as_mapping,	/* PyMappingMethods *tp_as_mapping; */

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
	pyrna_struct_methods,			/* struct PyMethodDef *tp_methods; */
	NULL,                       /* struct PyMemberDef *tp_members; */
	pyrna_struct_getseters,		/* struct PyGetSetDef *tp_getset; */
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
	PyVarObject_HEAD_INIT(NULL, 0)
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
	&pyrna_prop_as_sequence,	/* PySequenceMethods *tp_as_sequence; */
	&pyrna_prop_as_mapping,		/* PyMappingMethods *tp_as_mapping; */

	/* More standard operations (here for binary compatibility) */

	NULL,						/* hashfunc tp_hash; */
	NULL,                       /* ternaryfunc tp_call; */
	NULL,                       /* reprfunc tp_str; */

	/* will only use these if this is a subtype of a py class */
	( getattrofunc ) pyrna_prop_getattro,	/* getattrofunc tp_getattro; */
	( setattrofunc ) pyrna_prop_setattro,	/* setattrofunc tp_setattro; */

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
	NULL /*pyrna_prop_getseters*/,      	/* struct PyGetSetDef *tp_getset; */
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

static struct PyMethodDef pyrna_struct_subtype_methods[] = {
	{"BoolProperty", (PyCFunction)BPy_BoolProperty, METH_VARARGS|METH_KEYWORDS, ""},
	{"IntProperty", (PyCFunction)BPy_IntProperty, METH_VARARGS|METH_KEYWORDS, ""},
	{"FloatProperty", (PyCFunction)BPy_FloatProperty, METH_VARARGS|METH_KEYWORDS, ""},
	{"FloatVectorProperty", (PyCFunction)BPy_FloatVectorProperty, METH_VARARGS|METH_KEYWORDS, ""},
	{"StringProperty", (PyCFunction)BPy_StringProperty, METH_VARARGS|METH_KEYWORDS, ""},
	{"EnumProperty", (PyCFunction)BPy_EnumProperty, METH_VARARGS|METH_KEYWORDS, ""},
	{"PointerProperty", (PyCFunction)BPy_PointerProperty, METH_VARARGS|METH_KEYWORDS, ""},
	{"CollectionProperty", (PyCFunction)BPy_CollectionProperty, METH_VARARGS|METH_KEYWORDS, ""},

//	{"__get_rna", (PyCFunction)BPy_GetStructRNA, METH_NOARGS, ""},
	{NULL, NULL, 0, NULL}
};

static void pyrna_subtype_set_rna(PyObject *newclass, StructRNA *srna)
{
	PointerRNA ptr;
	PyObject *item;
	
	Py_INCREF(newclass);

	if (RNA_struct_py_type_get(srna))
		PyObSpit("RNA WAS SET - ", RNA_struct_py_type_get(srna));
	
	Py_XDECREF(((PyObject *)RNA_struct_py_type_get(srna)));
	
	RNA_struct_py_type_set(srna, (void *)newclass); /* Store for later use */

	/* Not 100% needed but useful,
	 * having an instance within a type looks wrong however this instance IS an rna type */

	/* python deals with the curcular ref */
	RNA_pointer_create(NULL, &RNA_Struct, srna, &ptr);
	item = pyrna_struct_CreatePyObject(&ptr);

	//item = PyCObject_FromVoidPtr(srna, NULL);
	PyDict_SetItemString(((PyTypeObject *)newclass)->tp_dict, "bl_rna", item);
	Py_DECREF(item);
	/* done with rna instance */

	/* attach functions into the class
	 * so you can do... bpy.types.Scene.SomeFunction()
	 */
	{
		PyMethodDef *ml;

		for(ml= pyrna_struct_subtype_methods; ml->ml_name; ml++){
			PyObject_SetAttrString(newclass, ml->ml_name, PyCFunction_New(ml, newclass));
		}
	}
}

/*
static StructRNA *srna_from_self(PyObject *self);
PyObject *BPy_GetStructRNA(PyObject *self)
{
	StructRNA *srna= pyrna_struct_as_srna(self);
	PointerRNA ptr;
	PyObject *ret;

	RNA_pointer_create(NULL, &RNA_Struct, srna, &ptr);
	ret= pyrna_struct_CreatePyObject(&ptr);

	if(ret) {
		return ret;
	}
	else {
		Py_RETURN_NONE;
	}
}
*/

static PyObject* pyrna_srna_Subtype(StructRNA *srna);

/* return a borrowed reference */
static PyObject* pyrna_srna_PyBase(StructRNA *srna) //, PyObject *bpy_types_dict)
{
	/* Assume RNA_struct_py_type_get(srna) was already checked */
	StructRNA *base;

	PyObject *py_base= NULL;

	/* get the base type */
	base= RNA_struct_base(srna);

	if(base && base != srna) {
		/*/printf("debug subtype %s %p\n", RNA_struct_identifier(srna), srna); */
		py_base= pyrna_srna_Subtype(base); //, bpy_types_dict);
		Py_DECREF(py_base); /* srna owns, this is only to pass as an arg */
	}

	if(py_base==NULL) {
		py_base= (PyObject *)&pyrna_struct_Type;
	}

	return py_base;
}

/* check if we have a native python subclass, use it when it exists
 * return a borrowed reference */
static PyObject* pyrna_srna_ExternalType(StructRNA *srna)
{
	PyObject *bpy_types_dict= NULL;
	const char *idname= RNA_struct_identifier(srna);
	PyObject *newclass;

	if(bpy_types_dict==NULL) {
		PyObject *bpy_types= PyImport_ImportModuleLevel("bpy_types", NULL, NULL, NULL, 0);

		if(bpy_types==NULL) {
			PyErr_Print();
			PyErr_Clear();
			fprintf(stderr, "pyrna_srna_ExternalType: failed to find 'bpy_types' module\n");
			return NULL;
		}

		bpy_types_dict = PyModule_GetDict(bpy_types); // borrow
		Py_DECREF(bpy_types); // fairly safe to assume the dict is kept
	}

	newclass= PyDict_GetItemString(bpy_types_dict, idname);

	/* sanity check, could skip this unless in debug mode */
	if(newclass) {
		PyObject *base_compare= pyrna_srna_PyBase(srna);
		//PyObject *slots= PyObject_GetAttrString(newclass, "__slots__"); // cant do this because it gets superclasses values!
		//PyObject *bases= PyObject_GetAttrString(newclass, "__bases__"); // can do this but faster not to.
		PyObject *bases= ((PyTypeObject *)newclass)->tp_bases;
		PyObject *slots = PyDict_GetItemString(((PyTypeObject *)newclass)->tp_dict, "__slots__");

		if(slots==NULL) {
			fprintf(stderr, "pyrna_srna_ExternalType: expected class '%s' to have __slots__ defined\n\nSee bpy_types.py\n", idname);
			newclass= NULL;
		}
		else if(PyTuple_GET_SIZE(bases)) {
			PyObject *base= PyTuple_GET_ITEM(bases, 0);

			if(base_compare != base) {
				fprintf(stderr, "pyrna_srna_ExternalType: incorrect subclassing of SRNA '%s'\nSee bpy_types.py\n", idname);
				PyObSpit("Expected! ", base_compare);
				newclass= NULL;
			}
			else {
				if(G.f & G_DEBUG)
					fprintf(stderr, "SRNA Subclassed: '%s'\n", idname);
			}
		}
	}

	return newclass;
}

static PyObject* pyrna_srna_Subtype(StructRNA *srna)
{
	PyObject *newclass = NULL;

	if (srna == NULL) {
		newclass= NULL; /* Nothing to do */
	} else if ((newclass= RNA_struct_py_type_get(srna))) {
		Py_INCREF(newclass);
	} else if ((newclass= pyrna_srna_ExternalType(srna))) {
		pyrna_subtype_set_rna(newclass, srna);
		Py_INCREF(newclass);
	} else {
		/* subclass equivelents
		- class myClass(myBase):
			some='value' # or ...
		- myClass = type(name='myClass', bases=(myBase,), dict={'__module__':'bpy.types'})
		*/

		/* Assume RNA_struct_py_type_get(srna) was alredy checked */
		PyObject *py_base= pyrna_srna_PyBase(srna);

		const char *idname= RNA_struct_identifier(srna);
		const char *descr= RNA_struct_ui_description(srna);

		if(!descr) descr= "(no docs)";
		
		/* always use O not N when calling, N causes refcount errors */
		newclass = PyObject_CallFunction(	(PyObject*)&PyType_Type, "s(O){sssss()}", idname, py_base, "__module__","bpy.types", "__doc__",descr, "__slots__");
		/* newclass will now have 2 ref's, ???, probably 1 is internal since decrefing here segfaults */

		/* PyObSpit("new class ref", newclass); */

		if (newclass) {
			/* srna owns one, and the other is owned by the caller */
			pyrna_subtype_set_rna(newclass, srna);

			Py_DECREF(newclass); /* let srna own */
		}
		else {
			/* this should not happen */
			PyErr_Print();
			PyErr_Clear();
		}
	}
	
	return newclass;
}

/* use for subtyping so we know which srna is used for a PointerRNA */
static StructRNA *srna_from_ptr(PointerRNA *ptr)
{
	if(ptr->type == &RNA_Struct) {
		return ptr->data;
	}
	else {
		return ptr->type;
	}
}

/* always returns a new ref, be sure to decref when done */
static PyObject* pyrna_struct_Subtype(PointerRNA *ptr)
{
	return pyrna_srna_Subtype(srna_from_ptr(ptr));
}

/*-----------------------CreatePyObject---------------------------------*/
PyObject *pyrna_struct_CreatePyObject( PointerRNA *ptr )
{
	BPy_StructRNA *pyrna= NULL;
	
	if (ptr->data==NULL && ptr->type==NULL) { /* Operator RNA has NULL data */
		Py_RETURN_NONE;
	}
	else {
		PyTypeObject *tp = (PyTypeObject *)pyrna_struct_Subtype(ptr);
		
		if (tp) {
			pyrna = (BPy_StructRNA *) tp->tp_alloc(tp, 0);
			Py_DECREF(tp); /* srna owns, cant hold a ref */
		}
		else {
			fprintf(stderr, "Could not make type\n");
			pyrna = ( BPy_StructRNA * ) PyObject_NEW( BPy_StructRNA, &pyrna_struct_Type );
		}
	}

	if( !pyrna ) {
		PyErr_SetString( PyExc_MemoryError, "couldn't create BPy_StructRNA object" );
		return NULL;
	}
	
	pyrna->ptr= *ptr;
	pyrna->freeptr= FALSE;
	
	// PyObSpit("NewStructRNA: ", (PyObject *)pyrna);
	
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

	pyrna->arraydim= 0;
	pyrna->arrayoffset= 0;
		
	return ( PyObject * ) pyrna;
}

void BPY_rna_init(void)
{
#ifdef USE_MATHUTILS // register mathutils callbacks, ok to run more then once.
	mathutils_rna_array_cb_index= Mathutils_RegisterCallback(&mathutils_rna_array_cb);
	mathutils_rna_matrix_cb_index= Mathutils_RegisterCallback(&mathutils_rna_matrix_cb);
#endif

	if( PyType_Ready( &pyrna_struct_Type ) < 0 )
		return;

	if( PyType_Ready( &pyrna_prop_Type ) < 0 )
		return;
}

/* bpy.data from python */
static PointerRNA *rna_module_ptr= NULL;
PyObject *BPY_rna_module(void)
{
	BPy_StructRNA *pyrna;
	PointerRNA ptr;

	/* for now, return the base RNA type rather then a real module */
	RNA_main_pointer_create(G.main, &ptr);
	pyrna= (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);
	
	rna_module_ptr= &pyrna->ptr;
	return (PyObject *)pyrna;
}

void BPY_update_rna_module(void)
{
	RNA_main_pointer_create(G.main, rna_module_ptr);
}

#if 0
/* This is a way we can access docstrings for RNA types
 * without having the datatypes in blender */
PyObject *BPY_rna_doc( void )
{
	PointerRNA ptr;
	
	/* for now, return the base RNA type rather then a real module */
	RNA_blender_rna_pointer_create(&ptr);
	
	return pyrna_struct_CreatePyObject(&ptr);
}
#endif


/* pyrna_basetype_* - BPy_BaseTypeRNA is just a BPy_PropertyRNA struct with a differnt type
 * the self->ptr and self->prop are always set to the "structs" collection */
//---------------getattr--------------------------------------------
static PyObject *pyrna_basetype_getattro( BPy_BaseTypeRNA *self, PyObject *pyname )
{
	PointerRNA newptr;
	PyObject *ret;
	char *name= _PyUnicode_AsString(pyname);
	
	if(strcmp(name, "register")==0) {
		/* this is called so often, make an exception and save a full lookup on all types */
		ret= PyObject_GenericGetAttr((PyObject *)self, pyname);
	}
	else if (RNA_property_collection_lookup_string(&self->ptr, self->prop, name, &newptr)) {
		ret= pyrna_struct_Subtype(&newptr);
		if (ret==NULL) {
			PyErr_Format(PyExc_SystemError, "bpy.types.%.200s subtype could not be generated, this is a bug!", _PyUnicode_AsString(pyname));
		}
	}
	else {
#if 0
		PyErr_Format(PyExc_AttributeError, "bpy.types.%.200s RNA_Struct does not exist", _PyUnicode_AsString(pyname));
		return NULL;
#endif
		/* The error raised here will be displayed */
		ret= PyObject_GenericGetAttr((PyObject *)self, pyname);
	}

	return ret;
}

static PyObject *pyrna_basetype_dir(BPy_BaseTypeRNA *self);
static struct PyMethodDef pyrna_basetype_methods[] = {
	{"__dir__", (PyCFunction)pyrna_basetype_dir, METH_NOARGS, ""},
	{"register", (PyCFunction)pyrna_basetype_register, METH_O, ""},
	{"unregister", (PyCFunction)pyrna_basetype_unregister, METH_O, ""},
	{NULL, NULL, 0, NULL}
};

static PyObject *pyrna_basetype_dir(BPy_BaseTypeRNA *self)
{
	PyObject *list, *name;
	PyMethodDef *meth;
	
	list= pyrna_prop_keys(self); /* like calling structs.keys(), avoids looping here */

	for(meth=pyrna_basetype_methods; meth->ml_name; meth++) {
		name = PyUnicode_FromString(meth->ml_name);
		PyList_Append(list, name);
		Py_DECREF(name);
	}
	
	return list;
}

PyTypeObject pyrna_basetype_Type = BLANK_PYTHON_TYPE;

PyObject *BPY_rna_types(void)
{
	BPy_BaseTypeRNA *self;

	if ((pyrna_basetype_Type.tp_flags & Py_TPFLAGS_READY)==0)  {
		pyrna_basetype_Type.tp_name = "RNA_Types";
		pyrna_basetype_Type.tp_basicsize = sizeof( BPy_BaseTypeRNA );
		pyrna_basetype_Type.tp_getattro = ( getattrofunc )pyrna_basetype_getattro;
		pyrna_basetype_Type.tp_flags = Py_TPFLAGS_DEFAULT;
		pyrna_basetype_Type.tp_methods = pyrna_basetype_methods;
		
		if( PyType_Ready( &pyrna_basetype_Type ) < 0 )
			return NULL;
	}
	
	self= (BPy_BaseTypeRNA *)PyObject_NEW( BPy_BaseTypeRNA, &pyrna_basetype_Type );
	
	/* avoid doing this lookup for every getattr */
	RNA_blender_rna_pointer_create(&self->ptr);
	self->prop = RNA_struct_find_property(&self->ptr, "structs");
	
	return (PyObject *)self;
}

StructRNA *pyrna_struct_as_srna(PyObject *self)
{
	BPy_StructRNA *py_srna = NULL;
	StructRNA *srna;
	
	/* ack, PyObject_GetAttrString wont look up this types tp_dict first :/ */
	if(PyType_Check(self)) {
		py_srna = (BPy_StructRNA *)PyDict_GetItemString(((PyTypeObject *)self)->tp_dict, "bl_rna");
		Py_XINCREF(py_srna);
	}
	
	if(py_srna==NULL)
		py_srna = (BPy_StructRNA*)PyObject_GetAttrString(self, "bl_rna");

	if(py_srna==NULL) {
	 	PyErr_SetString(PyExc_SystemError, "internal error, self had no bl_rna attribute, should never happen.");
		return NULL;
	}

	if(!BPy_StructRNA_Check(py_srna)) {
	 	PyErr_Format(PyExc_SystemError, "internal error, bl_rna was of type %.200s, instead of %.200s instance.", Py_TYPE(py_srna)->tp_name, pyrna_struct_Type.tp_name);
	 	Py_DECREF(py_srna);
		return NULL;
	}

	if(py_srna->ptr.type != &RNA_Struct) {
	 	PyErr_SetString(PyExc_SystemError, "internal error, bl_rna was not a RNA_Struct type of rna struct.");
	 	Py_DECREF(py_srna);
		return NULL;
	}

	srna= py_srna->ptr.data;
	Py_DECREF(py_srna);

	return srna;
}

/* Orphan functions, not sure where they should go */
/* get the srna for methods attached to types */
/* */
StructRNA *srna_from_self(PyObject *self)
{
	/* a bit sloppy but would cause a very confusing bug if
	 * an error happened to be set here */
	PyErr_Clear();

	if(self==NULL) {
		return NULL;
	}
	else if (PyCObject_Check(self)) {
		return PyCObject_AsVoidPtr(self);
	}
	else if (PyType_Check(self)==0) {
		return NULL;
	}
	/* These cases above not errors, they just mean the type was not compatible
	 * After this any errors will be raised in the script */

	return pyrna_struct_as_srna(self);
}

static int deferred_register_prop(StructRNA *srna, PyObject *item, PyObject *key, PyObject *dummy_args)
{
	/* We only care about results from C which
	 * are for sure types, save some time with error */
	if(PyTuple_CheckExact(item) && PyTuple_GET_SIZE(item)==2) {

		PyObject *py_func_ptr, *py_kw, *py_srna_cobject, *py_ret;
		PyObject *(*pyfunc)(PyObject *, PyObject *, PyObject *);

		if(PyArg_ParseTuple(item, "O!O!", &PyCObject_Type, &py_func_ptr, &PyDict_Type, &py_kw)) {

			if(*_PyUnicode_AsString(key)=='_') {
				PyErr_Format(PyExc_ValueError, "StructRNA \"%.200s\" registration error: %.200s could not register because the property starts with an '_'\n", RNA_struct_identifier(srna), _PyUnicode_AsString(key));
				Py_DECREF(dummy_args);
				return -1;
			}
			pyfunc = PyCObject_AsVoidPtr(py_func_ptr);
			py_srna_cobject = PyCObject_FromVoidPtr(srna, NULL);

			/* not 100% nice :/, modifies the dict passed, should be ok */
			PyDict_SetItemString(py_kw, "attr", key);

			py_ret = pyfunc(py_srna_cobject, dummy_args, py_kw);
			Py_DECREF(py_srna_cobject);

			if(py_ret) {
				Py_DECREF(py_ret);
			}
			else {
				PyErr_Print();
				PyErr_Clear();

				PyErr_Format(PyExc_ValueError, "StructRNA \"%.200s\" registration error: %.200s could not register\n", RNA_struct_identifier(srna), _PyUnicode_AsString(key));
				Py_DECREF(dummy_args);
				return -1;
			}
		}
		else {
			/* Since this is a class dict, ignore args that can't be passed */

			/* for testing only */
			/* PyObSpit("Why doesn't this work??", item);
			PyErr_Print(); */
			PyErr_Clear();
		}
	}

	return 0;
}

int pyrna_deferred_register_props(StructRNA *srna, PyObject *class_dict)
{
	PyObject *item, *key;
	PyObject *order;
	PyObject *dummy_args;
	Py_ssize_t pos = 0;
	int ret;

	dummy_args = PyTuple_New(0);

	order= PyDict_GetItemString(class_dict, "order");

	if(order==NULL)
		PyErr_Clear();

	if(order && PyList_Check(order)) {
		for(pos= 0; pos<PyList_GET_SIZE(order); pos++) {
			key= PyList_GET_ITEM(order, pos);
			item= PyDict_GetItem(class_dict, key);
			ret= deferred_register_prop(srna, item, key, dummy_args);
			if(ret==-1)
				break;
		}
	}
	else {
		while (PyDict_Next(class_dict, &pos, &key, &item)) {
			ret= deferred_register_prop(srna, item, key, dummy_args);

			if(ret==-1)
				break;
		}
	}

	Py_DECREF(dummy_args);

	return 0;
}

/*-------------------- Type Registration ------------------------*/

static int rna_function_arg_count(FunctionRNA *func)
{
	const ListBase *lb= RNA_function_defined_parameters(func);
	PropertyRNA *parm;
	Link *link;
	int count= 1;

	for(link=lb->first; link; link=link->next) {
		parm= (PropertyRNA*)link;
		if(!(RNA_property_flag(parm) & PROP_OUTPUT))
			count++;
	}
	
	return count;
}

static int bpy_class_validate(PointerRNA *dummyptr, void *py_data, int *have_function)
{
	const ListBase *lb;
	Link *link;
	FunctionRNA *func;
	PropertyRNA *prop;
	StructRNA *srna= dummyptr->type;
	const char *class_type= RNA_struct_identifier(srna);
	PyObject *py_class= (PyObject*)py_data;
	PyObject *base_class= RNA_struct_py_type_get(srna);
	PyObject *item, *fitem;
	PyObject *py_arg_count;
	int i, flag, arg_count, func_arg_count;
	const char *py_class_name = ((PyTypeObject *)py_class)->tp_name; // __name__


	if (base_class) {
		if (!PyObject_IsSubclass(py_class, base_class)) {
			PyErr_Format( PyExc_TypeError, "expected %.200s subclass of class \"%.200s\"", class_type, py_class_name);
			return -1;
		}
	}

	/* verify callback functions */
	lb= RNA_struct_defined_functions(srna);
	i= 0;
	for(link=lb->first; link; link=link->next) {
		func= (FunctionRNA*)link;
		flag= RNA_function_flag(func);

		if(!(flag & FUNC_REGISTER))
			continue;

		item = PyObject_GetAttrString(py_class, RNA_function_identifier(func));

		have_function[i]= (item != NULL);
		i++;

		if (item==NULL) {
			if ((flag & FUNC_REGISTER_OPTIONAL)==0) {
				PyErr_Format( PyExc_AttributeError, "expected %.200s, %.200s class to have an \"%.200s\" attribute", class_type, py_class_name, RNA_function_identifier(func));
				return -1;
			}

			PyErr_Clear();
		}
		else {
			Py_DECREF(item); /* no need to keep a ref, the class owns it */

			if (PyMethod_Check(item))
				fitem= PyMethod_Function(item); /* py 2.x */
			else
				fitem= item; /* py 3.x */

			if (PyFunction_Check(fitem)==0) {
				PyErr_Format( PyExc_TypeError, "expected %.200s, %.200s class \"%.200s\" attribute to be a function", class_type, py_class_name, RNA_function_identifier(func));
				return -1;
			}

			func_arg_count= rna_function_arg_count(func);

			if (func_arg_count >= 0) { /* -1 if we dont care*/
				py_arg_count = PyObject_GetAttrString(PyFunction_GET_CODE(fitem), "co_argcount");
				arg_count = PyLong_AsSsize_t(py_arg_count);
				Py_DECREF(py_arg_count);

				if (arg_count != func_arg_count) {
					PyErr_Format( PyExc_AttributeError, "expected %.200s, %.200s class \"%.200s\" function to have %d args", class_type, py_class_name, RNA_function_identifier(func), func_arg_count);
					return -1;
				}
			}
		}
	}

	/* verify properties */
	lb= RNA_struct_defined_properties(srna);
	for(link=lb->first; link; link=link->next) {
		const char *identifier;
		prop= (PropertyRNA*)link;
		flag= RNA_property_flag(prop);

		if(!(flag & PROP_REGISTER))
			continue;

		identifier= RNA_property_identifier(prop);
		item = PyObject_GetAttrString(py_class, identifier);

		if (item==NULL) {
			/* Sneaky workaround to use the class name as the bl_idname */

#define		BPY_REPLACEMENT_STRING(rna_attr, py_attr) \
			if(strcmp(identifier, rna_attr) == 0) { \
				item= PyObject_GetAttrString(py_class, py_attr); \
				if(item && item != Py_None) { \
					if(pyrna_py_to_prop(dummyptr, prop, NULL, NULL, item, "validating class error:") != 0) { \
						Py_DECREF(item); \
						return -1; \
					} \
				} \
				Py_XDECREF(item); \
			} \


			BPY_REPLACEMENT_STRING("bl_idname", "__name__");
			BPY_REPLACEMENT_STRING("bl_description", "__doc__");

#undef		BPY_REPLACEMENT_STRING

			if (item == NULL && (((flag & PROP_REGISTER_OPTIONAL) != PROP_REGISTER_OPTIONAL))) {
				PyErr_Format( PyExc_AttributeError, "expected %.200s, %.200s class to have an \"%.200s\" attribute", class_type, py_class_name, identifier);
				return -1;
			}

			PyErr_Clear();
		}
		else {
			Py_DECREF(item); /* no need to keep a ref, the class owns it */

			if(pyrna_py_to_prop(dummyptr, prop, NULL, NULL, item, "validating class error:") != 0)
				return -1;
		}
	}

	return 0;
}

extern void BPY_update_modules( void ); //XXX temp solution

/* TODO - multiple return values like with rna functions */
static int bpy_class_call(PointerRNA *ptr, FunctionRNA *func, ParameterList *parms)
{
	PyObject *args;
	PyObject *ret= NULL, *py_class, *py_class_instance, *item, *parmitem;
	PropertyRNA *parm;
	ParameterIterator iter;
	PointerRNA funcptr;
	int err= 0, i, flag, ret_len=0;

	PropertyRNA *pret_single= NULL;
	void *retdata_single= NULL;

	PyGILState_STATE gilstate;

	bContext *C= BPy_GetContext(); // XXX - NEEDS FIXING, QUITE BAD.
	bpy_context_set(C, &gilstate);

	py_class= RNA_struct_py_type_get(ptr->type);
	
	item = pyrna_struct_CreatePyObject(ptr);
	if(item == NULL) {
		py_class_instance = NULL;
	}
	else if(item == Py_None) { /* probably wont ever happen but possible */
		Py_DECREF(item);
		py_class_instance = NULL;
	}
	else {
		args = PyTuple_New(1);
		PyTuple_SET_ITEM(args, 0, item);
		py_class_instance = PyObject_Call(py_class, args, NULL);
		Py_DECREF(args);
	}

	if (py_class_instance) { /* Initializing the class worked, now run its invoke function */
		item= PyObject_GetAttrString(py_class, RNA_function_identifier(func));
//		flag= RNA_function_flag(func);

		if(item) {
			RNA_pointer_create(NULL, &RNA_Function, func, &funcptr);

			args = PyTuple_New(rna_function_arg_count(func)); /* first arg is included in 'item' */
			PyTuple_SET_ITEM(args, 0, py_class_instance);

			RNA_parameter_list_begin(parms, &iter);

			/* parse function parameters */
			for (i= 1; iter.valid; RNA_parameter_list_next(&iter)) {
				parm= iter.parm;
				flag= RNA_property_flag(parm);

				/* only useful for single argument returns, we'll need another list loop for multiple */
				if (flag & PROP_OUTPUT) {
					ret_len++;
					if (pret_single==NULL) {
						pret_single= parm;
						retdata_single= iter.data;
					}

					continue;
				}

				parmitem= pyrna_param_to_py(&funcptr, parms, parm, iter.data);
				PyTuple_SET_ITEM(args, i, parmitem);
				i++;
			}

			ret = PyObject_Call(item, args, NULL);

			RNA_parameter_list_end(&iter);
			Py_DECREF(item);
			Py_DECREF(args);
		}
		else {
			PyErr_Print();
			PyErr_Clear();
			PyErr_Format(PyExc_TypeError, "could not find function %.200s in %.200s to execute callback.", RNA_function_identifier(func), RNA_struct_identifier(ptr->type));
			err= -1;
		}
	}
	else {
		PyErr_Format(PyExc_RuntimeError, "could not create instance of %.200s to call callback function %.200s.", RNA_struct_identifier(ptr->type), RNA_function_identifier(func));
		err= -1;
	}

	if (ret == NULL) { /* covers py_class_instance failing too */
		err= -1;
	}
	else {
		if(ret_len==1) {
			err= pyrna_py_to_prop(&funcptr, pret_single, parms, retdata_single, ret, "calling class function:");
		}
		else if (ret_len > 1) {

			if(PyTuple_Check(ret)==0) {
				PyErr_Format(PyExc_RuntimeError, "expected class %.200s, function %.200s to return a tuple of size %d.", RNA_struct_identifier(ptr->type), RNA_function_identifier(func), ret_len);
				err= -1;
			}
			else if (PyTuple_GET_SIZE(ret) != ret_len) {
				PyErr_Format(PyExc_RuntimeError, "class %.200s, function %.200s to returned %d items, expected %d.", RNA_struct_identifier(ptr->type), RNA_function_identifier(func), PyTuple_GET_SIZE(ret), ret_len);
				err= -1;
			}
			else {

				RNA_parameter_list_begin(parms, &iter);

				/* parse function parameters */
				for (i= 0; iter.valid; RNA_parameter_list_next(&iter)) {
					parm= iter.parm;
					flag= RNA_property_flag(parm);

					/* only useful for single argument returns, we'll need another list loop for multiple */
					if (flag & PROP_OUTPUT) {
						err= pyrna_py_to_prop(&funcptr, parm, parms, iter.data, PyTuple_GET_ITEM(ret, i++), "calling class function:");
						if(err)
							break;
					}
				}

				RNA_parameter_list_end(&iter);
			}
		}
		Py_DECREF(ret);
	}

	if(err != 0) {
		PyErr_Print();
		PyErr_Clear();
	}

	bpy_context_clear(C, &gilstate);
	
	return err;
}

static void bpy_class_free(void *pyob_ptr)
{
	PyObject *self= (PyObject *)pyob_ptr;
	PyGILState_STATE gilstate;

	gilstate = PyGILState_Ensure();

	PyDict_Clear(((PyTypeObject*)self)->tp_dict);

	if(G.f&G_DEBUG) {
		if(self->ob_refcnt > 1) {
			PyObSpit("zombie class - ref should be 1", self);
		}
	}

	Py_DECREF((PyObject *)pyob_ptr);

	PyGILState_Release(gilstate);
}

void pyrna_alloc_types(void)
{
	PyGILState_STATE gilstate;

	PointerRNA ptr;
	PropertyRNA *prop;
	
	gilstate = PyGILState_Ensure();

	/* avoid doing this lookup for every getattr */
	RNA_blender_rna_pointer_create(&ptr);
	prop = RNA_struct_find_property(&ptr, "structs");

	RNA_PROP_BEGIN(&ptr, itemptr, prop) {
		Py_DECREF(pyrna_struct_Subtype(&itemptr));
	}
	RNA_PROP_END;

	PyGILState_Release(gilstate);
}


void pyrna_free_types(void)
{
	PointerRNA ptr;
	PropertyRNA *prop;

	/* avoid doing this lookup for every getattr */
	RNA_blender_rna_pointer_create(&ptr);
	prop = RNA_struct_find_property(&ptr, "structs");


	RNA_PROP_BEGIN(&ptr, itemptr, prop) {
		StructRNA *srna= srna_from_ptr(&itemptr);
		void *py_ptr= RNA_struct_py_type_get(srna);

		if(py_ptr) {
#if 0	// XXX - should be able to do this but makes python crash on exit
			bpy_class_free(py_ptr);
#endif
			RNA_struct_py_type_set(srna, NULL);
		}
	}
	RNA_PROP_END;

}

/* Note! MemLeak XXX
 *
 * There is currently a bug where moving registering a python class does
 * not properly manage refcounts from the python class, since the srna owns
 * the python class this should not be so tricky but changing the references as
 * youd expect when changing ownership crashes blender on exit so I had to comment out
 * the decref. This is not so bad because the leak only happens when re-registering (hold F8)
 * - Should still be fixed - Campbell
 * */

PyObject *pyrna_basetype_register(PyObject *self, PyObject *py_class)
{
	bContext *C= NULL;
	ReportList reports;
	StructRegisterFunc reg;
	StructRNA *srna;
	StructRNA *srna_new;
	PyObject *item;
	const char *identifier= "";

	srna= pyrna_struct_as_srna(py_class);
	if(srna==NULL)
		return NULL;
	
	/* check that we have a register callback for this type */
	reg= RNA_struct_register(srna);

	if(!reg) {
		PyErr_SetString(PyExc_AttributeError, "expected a Type subclassed from a registerable rna type (no register supported).");
		return NULL;
	}
	
	/* get the context, so register callback can do necessary refreshes */
	C= BPy_GetContext();

	/* call the register callback with reports & identifier */
	BKE_reports_init(&reports, RPT_STORE);

	item= PyObject_GetAttrString(py_class, "__name__");

	if(item) {
		identifier= _PyUnicode_AsString(item);
		Py_DECREF(item); /* no need to keep a ref, the class owns it */
	}

	srna_new= reg(C, &reports, py_class, identifier, bpy_class_validate, bpy_class_call, bpy_class_free);

	if(!srna_new) {
		BPy_reports_to_error(&reports);
		BKE_reports_clear(&reports);
		return NULL;
	}

	BKE_reports_clear(&reports);

	pyrna_subtype_set_rna(py_class, srna_new); /* takes a ref to py_class */

	/* old srna still references us, keep the check incase registering somehow can free it  */
	if(RNA_struct_py_type_get(srna)) {
		RNA_struct_py_type_set(srna, NULL);
		// Py_DECREF(py_class); // shuld be able to do this XXX since the old rna adds a new ref.
	}

	/* Can't use this because it returns a dict proxy
	 *
	 * item= PyObject_GetAttrString(py_class, "__dict__");
	 */
	item= ((PyTypeObject*)py_class)->tp_dict;
	if(item) {
		if(pyrna_deferred_register_props(srna_new, item)!=0) {
			return NULL;
		}
	}
	else {
		PyErr_Clear();
	}

	Py_RETURN_NONE;
}

PyObject *pyrna_basetype_unregister(PyObject *self, PyObject *py_class)
{
	bContext *C= NULL;
	StructUnregisterFunc unreg;
	StructRNA *srna;

	srna= pyrna_struct_as_srna(py_class);
	if(srna==NULL)
		return NULL;
	
	/* check that we have a unregister callback for this type */
	unreg= RNA_struct_unregister(srna);

	if(!unreg) {
		PyErr_SetString(PyExc_AttributeError, "expected a Type subclassed from a registerable rna type (no unregister supported).");
		return NULL;
	}
	
	/* get the context, so register callback can do necessary refreshes */
	C= BPy_GetContext();

	/* call unregister */
	unreg(C, srna); /* calls bpy_class_free, this decref's py_class */

	Py_RETURN_NONE;
}

