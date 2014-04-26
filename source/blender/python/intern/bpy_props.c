/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_props.c
 *  \ingroup pythonintern
 *
 * This file defines 'bpy.props' module used so scripts can define their own
 * rna properties for use with python operators or adding new properties to
 * existing blender types.
 */


#include <Python.h>

#include "RNA_types.h"

#include "BLI_utildefines.h"

#include "bpy_props.h"
#include "bpy_rna.h"
#include "bpy_util.h"

#include "BKE_idprop.h"

#include "RNA_access.h"
#include "RNA_define.h" /* for defining our own rna */
#include "RNA_enum_types.h"

#include "MEM_guardedalloc.h"

#include "../generic/py_capi_utils.h"

/* initial definition of callback slots we'll probably have more than 1 */
#define BPY_DATA_CB_SLOT_SIZE 3

#define BPY_DATA_CB_SLOT_UPDATE 0
#define BPY_DATA_CB_SLOT_GET 1
#define BPY_DATA_CB_SLOT_SET 2

extern BPy_StructRNA *bpy_context_module;

static EnumPropertyItem property_flag_items[] = {
	{PROP_HIDDEN, "HIDDEN", 0, "Hidden", ""},
	{PROP_SKIP_SAVE, "SKIP_SAVE", 0, "Skip Save", ""},
	{PROP_ANIMATABLE, "ANIMATABLE", 0, "Animatable", ""},
	{PROP_LIB_EXCEPTION, "LIBRARY_EDITABLE", 0, "Library Editable", ""},
	{PROP_PROPORTIONAL, "PROPORTIONAL", 0, "Adjust values proportionally to eachother", ""},
	{0, NULL, 0, NULL, NULL}};

#define BPY_PROPDEF_OPTIONS_DOC \
"   :arg options: Enumerator in ['HIDDEN', 'SKIP_SAVE', 'ANIMATABLE', 'LIBRARY_EDITABLE', 'PROPORTIONAL'].\n" \
"   :type options: set\n" \

static EnumPropertyItem property_flag_enum_items[] = {
	{PROP_HIDDEN, "HIDDEN", 0, "Hidden", ""},
	{PROP_SKIP_SAVE, "SKIP_SAVE", 0, "Skip Save", ""},
	{PROP_ANIMATABLE, "ANIMATABLE", 0, "Animatable", ""},
	{PROP_LIB_EXCEPTION, "LIBRARY_EDITABLE", 0, "Library Editable", ""},
	{PROP_ENUM_FLAG, "ENUM_FLAG", 0, "Enum Flag", ""},
	{0, NULL, 0, NULL, NULL}};

#define BPY_PROPDEF_OPTIONS_ENUM_DOC \
"   :type default: string or set\n" \
"   :arg options: Enumerator in ['HIDDEN', 'SKIP_SAVE', 'ANIMATABLE', 'ENUM_FLAG', 'LIBRARY_EDITABLE'].\n" \

/* subtypes */
/* XXX Keep in sync with rna_rna.c's property_subtype_items ???
 *     Currently it is not...
 */
static EnumPropertyItem property_subtype_string_items[] = {
	{PROP_FILEPATH, "FILE_PATH", 0, "File Path", ""},
	{PROP_DIRPATH, "DIR_PATH", 0, "Directory Path", ""},
	{PROP_FILENAME, "FILE_NAME", 0, "Filename", ""},
	{PROP_BYTESTRING, "BYTE_STRING", 0, "Byte String", ""},
	{PROP_PASSWORD, "PASSWORD", 0, "Password", "A string that is displayed hidden ('********')"},

	{PROP_NONE, "NONE", 0, "None", ""},
	{0, NULL, 0, NULL, NULL}};

#define BPY_PROPDEF_SUBTYPE_STRING_DOC \
"   :arg subtype: Enumerator in ['FILE_PATH', 'DIR_PATH', 'FILE_NAME', 'BYTE_STRING', 'PASSWORD', 'NONE'].\n" \
"   :type subtype: string\n" \

static EnumPropertyItem property_subtype_number_items[] = {
	{PROP_PIXEL, "PIXEL", 0, "Pixel", ""},
	{PROP_UNSIGNED, "UNSIGNED", 0, "Unsigned", ""},
	{PROP_PERCENTAGE, "PERCENTAGE", 0, "Percentage", ""},
	{PROP_FACTOR, "FACTOR", 0, "Factor", ""},
	{PROP_ANGLE, "ANGLE", 0, "Angle", ""},
	{PROP_TIME, "TIME", 0, "Time", ""},
	{PROP_DISTANCE, "DISTANCE", 0, "Distance", ""},

	{PROP_NONE, "NONE", 0, "None", ""},
	{0, NULL, 0, NULL, NULL}};

#define BPY_PROPDEF_SUBTYPE_NUMBER_DOC \
"   :arg subtype: Enumerator in ['PIXEL', 'UNSIGNED', 'PERCENTAGE', 'FACTOR', 'ANGLE', 'TIME', 'DISTANCE', 'NONE'].\n" \
"   :type subtype: string\n" \

static EnumPropertyItem property_subtype_array_items[] = {
	{PROP_COLOR, "COLOR", 0, "Color", ""},
	{PROP_TRANSLATION, "TRANSLATION", 0, "Translation", ""},
	{PROP_DIRECTION, "DIRECTION", 0, "Direction", ""},
	{PROP_VELOCITY, "VELOCITY", 0, "Velocity", ""},
	{PROP_ACCELERATION, "ACCELERATION", 0, "Acceleration", ""},
	{PROP_MATRIX, "MATRIX", 0, "Matrix", ""},
	{PROP_EULER, "EULER", 0, "Euler", ""},
	{PROP_QUATERNION, "QUATERNION", 0, "Quaternion", ""},
	{PROP_AXISANGLE, "AXISANGLE", 0, "Axis Angle", ""},
	{PROP_XYZ, "XYZ", 0, "XYZ", ""},
	{PROP_COLOR_GAMMA, "COLOR_GAMMA", 0, "Color Gamma", ""},
	{PROP_LAYER, "LAYER", 0, "Layer", ""},

	{PROP_NONE, "NONE", 0, "None", ""},
	{0, NULL, 0, NULL, NULL}};

#define BPY_PROPDEF_SUBTYPE_ARRAY_DOC \
"   :arg subtype: Enumerator in ['COLOR', 'TRANSLATION', 'DIRECTION', " \
                                "'VELOCITY', 'ACCELERATION', 'MATRIX', 'EULER', 'QUATERNION', 'AXISANGLE', " \
                                "'XYZ', 'COLOR_GAMMA', 'LAYER', 'NONE'].\n" \
"   :type subtype: string\n"

/* PyObject's */
static PyObject *pymeth_BoolProperty = NULL;
static PyObject *pymeth_BoolVectorProperty = NULL;
static PyObject *pymeth_IntProperty = NULL;
static PyObject *pymeth_IntVectorProperty = NULL;
static PyObject *pymeth_FloatProperty = NULL;
static PyObject *pymeth_FloatVectorProperty = NULL;
static PyObject *pymeth_StringProperty = NULL;
static PyObject *pymeth_EnumProperty = NULL;
static PyObject *pymeth_PointerProperty = NULL;
static PyObject *pymeth_CollectionProperty = NULL;
static PyObject *pymeth_RemoveProperty = NULL;

static PyObject *pyrna_struct_as_instance(PointerRNA *ptr)
{
	PyObject *self = NULL;
	/* first get self */
	/* operators can store their own instance for later use */
	if (ptr->data) {
		void **instance = RNA_struct_instance(ptr);

		if (instance) {
			if (*instance) {
				self = *instance;
				Py_INCREF(self);
			}
		}
	}

	/* in most cases this will run */
	if (self == NULL) {
		self = pyrna_struct_CreatePyObject(ptr);
	}

	return self;
}

/* could be moved into bpy_utils */
static void printf_func_error(PyObject *py_func)
{
	/* since we return to C code we can't leave the error */
	PyCodeObject *f_code = (PyCodeObject *)PyFunction_GET_CODE(py_func);
	PyErr_Print();
	PyErr_Clear();

	/* use py style error */
	fprintf(stderr, "File \"%s\", line %d, in %s\n",
	        _PyUnicode_AsString(f_code->co_filename),
	        f_code->co_firstlineno,
	        _PyUnicode_AsString(((PyFunctionObject *)py_func)->func_name)
	        );
}

/* operators and classes use this so it can store the args given but defer
 * running it until the operator runs where these values are used to setup
 * the default args for that operator instance */
static PyObject *bpy_prop_deferred_return(PyObject *func, PyObject *kw)
{
	PyObject *ret = PyTuple_New(2);
	PyTuple_SET_ITEM(ret, 0, func);
	Py_INCREF(func);

	if (kw == NULL)
		kw = PyDict_New();
	else
		Py_INCREF(kw);

	PyTuple_SET_ITEM(ret, 1, kw);

	return ret;
}

/* callbacks */
static void bpy_prop_update_cb(struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop)
{
	PyGILState_STATE gilstate;
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	const bool is_write_ok = pyrna_write_check();

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	bpy_context_set(C, &gilstate);

	py_func = py_data[BPY_DATA_CB_SLOT_UPDATE];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	PyTuple_SET_ITEM(args, 1, (PyObject *)bpy_context_module);
	Py_INCREF(bpy_context_module);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}

	bpy_context_clear(C, &gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static int bpy_prop_boolean_get_cb(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int value;

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
		value = false;
	}
	else {
		value = PyLong_AsLong(ret);

		if (value == -1 && PyErr_Occurred()) {
			printf_func_error(py_func);
			value = false;
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}

	return value;
}

static void bpy_prop_boolean_set_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, int value)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_SET];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	PyTuple_SET_ITEM(args, 1, PyBool_FromLong(value));

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}
	
	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static void bpy_prop_boolean_array_get_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, int *values)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int i, len = RNA_property_array_length(ptr, prop);

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);

		for (i = 0; i < len; ++i)
			values[i] = false;
	}
	else {
		if (PyC_AsArray(values, ret, len, &PyBool_Type, false, "BoolVectorProperty get") == -1) {
			printf_func_error(py_func);

			for (i = 0; i < len; ++i)
				values[i] = false;

			/* PyC_AsArray decrements refcount internally on error */
		}
		else {
			Py_DECREF(ret);
		}
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static void bpy_prop_boolean_array_set_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, const int *values)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyObject *py_values;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int len = RNA_property_array_length(ptr, prop);

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_SET];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	py_values = PyC_FromArray(values, len, &PyBool_Type, false, "BoolVectorProperty set");
	if (!py_values) {
		printf_func_error(py_func);
	}
	else
		PyTuple_SET_ITEM(args, 1, py_values);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static int bpy_prop_int_get_cb(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int value;

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
		value = 0.0f;
	}
	else {
		value = PyLong_AsLong(ret);

		if (value == -1 && PyErr_Occurred()) {
			printf_func_error(py_func);
			value = 0;
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}

	return value;
}

static void bpy_prop_int_set_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, int value)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_SET];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	PyTuple_SET_ITEM(args, 1, PyLong_FromLong(value));

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static void bpy_prop_int_array_get_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, int *values)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int i, len = RNA_property_array_length(ptr, prop);

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);

		for (i = 0; i < len; ++i)
			values[i] = 0;
	}
	else {
		if (PyC_AsArray(values, ret, len, &PyLong_Type, false, "IntVectorProperty get") == -1) {
			printf_func_error(py_func);

			for (i = 0; i < len; ++i)
				values[i] = 0;

			/* PyC_AsArray decrements refcount internally on error */
		}
		else {
			Py_DECREF(ret);
		}
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static void bpy_prop_int_array_set_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, const int *values)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyObject *py_values;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int len = RNA_property_array_length(ptr, prop);

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_SET];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	py_values = PyC_FromArray(values, len, &PyLong_Type, false, "IntVectorProperty set");
	if (!py_values) {
		printf_func_error(py_func);
	}
	else
		PyTuple_SET_ITEM(args, 1, py_values);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static float bpy_prop_float_get_cb(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	float value;

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
		value = 0.0f;
	}
	else {
		value = PyFloat_AsDouble(ret);

		if (value == -1.0f && PyErr_Occurred()) {
			printf_func_error(py_func);
			value = 0.0f;
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}

	return value;
}

static void bpy_prop_float_set_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, float value)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_SET];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	PyTuple_SET_ITEM(args, 1, PyFloat_FromDouble(value));

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static void bpy_prop_float_array_get_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, float *values)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int i, len = RNA_property_array_length(ptr, prop);

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);

		for (i = 0; i < len; ++i)
			values[i] = 0.0f;
	}
	else {
		if (PyC_AsArray(values, ret, len, &PyFloat_Type, false, "FloatVectorProperty get") == -1) {
			printf_func_error(py_func);

			for (i = 0; i < len; ++i)
				values[i] = 0.0f;

			/* PyC_AsArray decrements refcount internally on error */
		}
		else {
			Py_DECREF(ret);
		}
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static void bpy_prop_float_array_set_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, const float *values)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyObject *py_values;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int len = RNA_property_array_length(ptr, prop);

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_SET];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	py_values = PyC_FromArray(values, len, &PyFloat_Type, false, "FloatVectorProperty set");
	if (!py_values) {
		printf_func_error(py_func);
	}
	else
		PyTuple_SET_ITEM(args, 1, py_values);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static void bpy_prop_string_get_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, char *value)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
		value[0] = '\0';
	}
	else if (!PyUnicode_Check(ret)) {
		PyErr_Format(PyExc_TypeError,
		             "return value must be a string, not %.200s",
		             Py_TYPE(ret)->tp_name);
		printf_func_error(py_func);
		value[0] = '\0';
		Py_DECREF(ret);
	}
	else {
		Py_ssize_t length;
		const char *buffer = _PyUnicode_AsStringAndSize(ret, &length);
		memcpy(value, buffer, length + 1);
		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static int bpy_prop_string_length_cb(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int length;

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
		length = 0;
	}
	else if (!PyUnicode_Check(ret)) {
		PyErr_Format(PyExc_TypeError,
		             "return value must be a string, not %.200s",
		             Py_TYPE(ret)->tp_name);
		printf_func_error(py_func);
		length = 0;
		Py_DECREF(ret);
	}
	else {
		Py_ssize_t length_ssize_t = 0;
		_PyUnicode_AsStringAndSize(ret, &length_ssize_t);
		length = length_ssize_t;
		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}

	return length;
}

static void bpy_prop_string_set_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, const char *value)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	PyObject *py_value;

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_SET];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	py_value = PyUnicode_FromString(value);
	if (!py_value) {
		PyErr_SetString(PyExc_ValueError, "the return value must be a string");
		printf_func_error(py_func);
	}
	else
		PyTuple_SET_ITEM(args, 1, py_value);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

static int bpy_prop_enum_get_cb(struct PointerRNA *ptr, struct PropertyRNA *prop)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();
	int value;

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_GET];

	args = PyTuple_New(1);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
		value = RNA_property_enum_get_default(ptr, prop);
	}
	else {
		value = PyLong_AsLong(ret);

		if (value == -1 && PyErr_Occurred()) {
			printf_func_error(py_func);
			value = RNA_property_enum_get_default(ptr, prop);
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}

	return value;
}

static void bpy_prop_enum_set_cb(struct PointerRNA *ptr, struct PropertyRNA *prop, int value)
{
	PyObject **py_data = (PyObject **)RNA_property_py_data_get(prop);
	PyObject *py_func;
	PyObject *args;
	PyObject *self;
	PyObject *ret;
	PyGILState_STATE gilstate;
	bool use_gil;
	const bool is_write_ok = pyrna_write_check();

	BLI_assert(py_data != NULL);

	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	use_gil = true;  /* !PyC_IsInterpreterActive(); */

	if (use_gil)
		gilstate = PyGILState_Ensure();

	py_func = py_data[BPY_DATA_CB_SLOT_SET];

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	PyTuple_SET_ITEM(args, 1, PyLong_FromLong(value));

	ret = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (ret == NULL) {
		printf_func_error(py_func);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			printf_func_error(py_func);
		}

		Py_DECREF(ret);
	}

	if (use_gil)
		PyGILState_Release(gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

/* utility function we need for parsing int's in an if statement */
static int py_long_as_int(PyObject *py_long, int *r_int)
{
	if (PyLong_CheckExact(py_long)) {
		*r_int = (int)PyLong_AS_LONG(py_long);
		return 0;
	}
	else {
		return -1;
	}
}

#if 0
/* copies orig to buf, then sets orig to buf, returns copy length */
static size_t strswapbufcpy(char *buf, const char **orig)
{
	const char *src = *orig;
	char *dst = buf;
	size_t i = 0;
	*orig = buf;
	while ((*dst = *src)) { dst++; src++; i++; }
	return i + 1; /* include '\0' */
}
#endif

static int icon_id_from_name(const char *name)
{
	EnumPropertyItem *item;
	int id;

	if (name[0]) {
		for (item = icon_items, id = 0; item->identifier; item++, id++) {
			if (STREQ(item->name, name)) {
				return item->value;
			}
		}
	}
	
	return 0;
}

static EnumPropertyItem *enum_items_from_py(PyObject *seq_fast, PyObject *def, int *defvalue, const short is_enum_flag)
{
	EnumPropertyItem *items;
	PyObject *item;
	const Py_ssize_t seq_len = PySequence_Fast_GET_SIZE(seq_fast);
	Py_ssize_t totbuf = 0;
	int i;
	short def_used = 0;
	const char *def_cmp = NULL;

	if (is_enum_flag) {
		if (seq_len > RNA_ENUM_BITFLAG_SIZE) {
			PyErr_SetString(PyExc_TypeError,
			                "EnumProperty(...): maximum "
			                STRINGIFY(RNA_ENUM_BITFLAG_SIZE)
			                " members for a ENUM_FLAG type property");
			return NULL;
		}
		if (def && !PySet_Check(def)) {
			PyErr_Format(PyExc_TypeError,
			             "EnumProperty(...): default option must be a 'set' "
			             "type when ENUM_FLAG is enabled, not a '%.200s'",
			             Py_TYPE(def)->tp_name);
			return NULL;
		}
	}
	else {
		if (def) {
			def_cmp = _PyUnicode_AsString(def);
			if (def_cmp == NULL) {
				PyErr_Format(PyExc_TypeError,
				             "EnumProperty(...): default option must be a 'str' "
				             "type when ENUM_FLAG is disabled, not a '%.200s'",
				             Py_TYPE(def)->tp_name);
				return NULL;
			}
		}
	}

	/* blank value */
	*defvalue = 0;

	items = MEM_callocN(sizeof(EnumPropertyItem) * (seq_len + 1), "enum_items_from_py1");

	for (i = 0; i < seq_len; i++) {
		EnumPropertyItem tmp = {0, "", 0, "", ""};
		const char *tmp_icon = NULL;
		Py_ssize_t item_size;
		Py_ssize_t id_str_size;
		Py_ssize_t name_str_size;
		Py_ssize_t desc_str_size;

		item = PySequence_Fast_GET_ITEM(seq_fast, i);

		if ((PyTuple_CheckExact(item)) &&
		    (item_size = PyTuple_GET_SIZE(item)) &&
		    (item_size >= 3 && item_size <= 5) &&
		    (tmp.identifier =  _PyUnicode_AsStringAndSize(PyTuple_GET_ITEM(item, 0), &id_str_size)) &&
		    (tmp.name =        _PyUnicode_AsStringAndSize(PyTuple_GET_ITEM(item, 1), &name_str_size)) &&
		    (tmp.description = _PyUnicode_AsStringAndSize(PyTuple_GET_ITEM(item, 2), &desc_str_size)) &&
		    /* TODO, number isn't ensured to be unique from the script author */
		    (item_size != 4 || py_long_as_int(PyTuple_GET_ITEM(item, 3), &tmp.value) != -1) &&
		    (item_size != 5 || ((tmp_icon = _PyUnicode_AsString(PyTuple_GET_ITEM(item, 3))) &&
		                        py_long_as_int(PyTuple_GET_ITEM(item, 4), &tmp.value) != -1)))
		{
			if (is_enum_flag) {
				if (item_size < 4) {
					tmp.value = 1 << i;
				}

				if (def && PySet_Contains(def, PyTuple_GET_ITEM(item, 0))) {
					*defvalue |= tmp.value;
					def_used++;
				}
			}
			else {
				if (item_size < 4) {
					tmp.value = i;
				}

				if (def && def_used == 0 && STREQ(def_cmp, tmp.identifier)) {
					*defvalue = tmp.value;
					def_used++; /* only ever 1 */
				}
			}

			if (tmp_icon)
				tmp.icon = icon_id_from_name(tmp_icon);

			items[i] = tmp;

			/* calculate combine string length */
			totbuf += id_str_size + name_str_size + desc_str_size + 3; /* 3 is for '\0's */
		}
		else {
			MEM_freeN(items);
			PyErr_SetString(PyExc_TypeError,
			                "EnumProperty(...): expected a tuple containing "
			                "(identifier, name, description) and optionally an "
			                "icon name and unique number");
			return NULL;
		}

	}

	if (is_enum_flag) {
		/* strict check that all set members were used */
		if (def && def_used != PySet_GET_SIZE(def)) {
			MEM_freeN(items);

			PyErr_Format(PyExc_TypeError,
			             "EnumProperty(..., default={...}): set has %d unused member(s)",
			             PySet_GET_SIZE(def) - def_used);
			return NULL;
		}
	}
	else {
		if (def && def_used == 0) {
			MEM_freeN(items);

			PyErr_Format(PyExc_TypeError,
			             "EnumProperty(..., default=\'%s\'): not found in enum members",
			             def_cmp);
			return NULL;
		}
	}

	/* disabled duplicating strings because the array can still be freed and
	 * the strings from it referenced, for now we can't support dynamically
	 * created strings from python. */
#if 0
	/* this would all work perfectly _but_ the python strings may be freed
	 * immediately after use, so we need to duplicate them, ugh.
	 * annoying because it works most of the time without this. */
	{
		EnumPropertyItem *items_dup = MEM_mallocN((sizeof(EnumPropertyItem) * (seq_len + 1)) + (sizeof(char) * totbuf),
		                                          "enum_items_from_py2");
		EnumPropertyItem *items_ptr = items_dup;
		char *buf = ((char *)items_dup) + (sizeof(EnumPropertyItem) * (seq_len + 1));
		memcpy(items_dup, items, sizeof(EnumPropertyItem) * (seq_len + 1));
		for (i = 0; i < seq_len; i++, items_ptr++) {
			buf += strswapbufcpy(buf, &items_ptr->identifier);
			buf += strswapbufcpy(buf, &items_ptr->name);
			buf += strswapbufcpy(buf, &items_ptr->description);
		}
		MEM_freeN(items);
		items = items_dup;
	}
	/* end string duplication */
#endif

	return items;
}

static EnumPropertyItem *bpy_prop_enum_itemf_cb(struct bContext *C, PointerRNA *ptr, PropertyRNA *prop, bool *r_free)
{
	PyGILState_STATE gilstate;

	PyObject *py_func = RNA_property_enum_py_data_get(prop);
	PyObject *self = NULL;
	PyObject *args;
	PyObject *items; /* returned from the function call */

	EnumPropertyItem *eitems = NULL;
	int err = 0;

	bpy_context_set(C, &gilstate);

	args = PyTuple_New(2);
	self = pyrna_struct_as_instance(ptr);
	PyTuple_SET_ITEM(args, 0, self);

	/* now get the context */
	PyTuple_SET_ITEM(args, 1, (PyObject *)bpy_context_module);
	Py_INCREF(bpy_context_module);

	items = PyObject_CallObject(py_func, args);

	Py_DECREF(args);

	if (items == NULL) {
		err = -1;
	}
	else {
		PyObject *items_fast;
		int defvalue_dummy = 0;

		if (!(items_fast = PySequence_Fast(items, "EnumProperty(...): "
		                                   "return value from the callback was not a sequence")))
		{
			err = -1;
		}
		else {
			eitems = enum_items_from_py(items_fast, NULL, &defvalue_dummy,
			                            (RNA_property_flag(prop) & PROP_ENUM_FLAG) != 0);

			Py_DECREF(items_fast);

			if (!eitems) {
				err = -1;
			}
		}

		Py_DECREF(items);
	}

	if (err != -1) { /* worked */
		*r_free = true;
	}
	else {
		printf_func_error(py_func);

		eitems = DummyRNA_NULL_items;
	}


	bpy_context_clear(C, &gilstate);
	return eitems;
}

static int bpy_prop_callback_check(PyObject *py_func, const char *keyword, int argcount)
{
	if (py_func && py_func != Py_None) {
		if (!PyFunction_Check(py_func)) {
			PyErr_Format(PyExc_TypeError,
			             "%s keyword: expected a function type, not a %.200s",
			             keyword, Py_TYPE(py_func)->tp_name);
			return -1;
		}
		else {
			PyCodeObject *f_code = (PyCodeObject *)PyFunction_GET_CODE(py_func);
			if (f_code->co_argcount != argcount) {
				PyErr_Format(PyExc_TypeError,
				             "%s keyword: expected a function taking %d arguments, not %d",
				             keyword, argcount, f_code->co_argcount);
				return -1;
			}
		}
	}

	return 0;
}

static PyObject **bpy_prop_py_data_get(struct PropertyRNA *prop)
{
	PyObject **py_data = RNA_property_py_data_get(prop);
	if (!py_data) {
		py_data = MEM_callocN(sizeof(PyObject *) * BPY_DATA_CB_SLOT_SIZE, __func__);
		RNA_def_py_data(prop, py_data);
	}
	return py_data;
}

static void bpy_prop_callback_assign_update(struct PropertyRNA *prop, PyObject *update_cb)
{
	/* assume this is already checked for type and arg length */
	if (update_cb && update_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		RNA_def_property_update_runtime(prop, (void *)bpy_prop_update_cb);
		py_data[BPY_DATA_CB_SLOT_UPDATE] = update_cb;

		RNA_def_property_flag(prop, PROP_CONTEXT_PROPERTY_UPDATE);
	}
}

static void bpy_prop_callback_assign_boolean(struct PropertyRNA *prop, PyObject *get_cb, PyObject *set_cb)
{
	BooleanPropertyGetFunc rna_get_cb = NULL;
	BooleanPropertySetFunc rna_set_cb = NULL;
	
	if (get_cb && get_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_get_cb = bpy_prop_boolean_get_cb;
		py_data[BPY_DATA_CB_SLOT_GET] = get_cb;
	}

	if (set_cb && set_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_set_cb = bpy_prop_boolean_set_cb;
		py_data[BPY_DATA_CB_SLOT_SET] = set_cb;
	}

	RNA_def_property_boolean_funcs_runtime(prop, rna_get_cb, rna_set_cb);
}

static void bpy_prop_callback_assign_boolean_array(struct PropertyRNA *prop, PyObject *get_cb, PyObject *set_cb)
{
	BooleanArrayPropertyGetFunc rna_get_cb = NULL;
	BooleanArrayPropertySetFunc rna_set_cb = NULL;
	
	if (get_cb && get_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_get_cb = bpy_prop_boolean_array_get_cb;
		py_data[BPY_DATA_CB_SLOT_GET] = get_cb;
	}

	if (set_cb && set_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_set_cb = bpy_prop_boolean_array_set_cb;
		py_data[BPY_DATA_CB_SLOT_SET] = set_cb;
	}

	RNA_def_property_boolean_array_funcs_runtime(prop, rna_get_cb, rna_set_cb);
}

static void bpy_prop_callback_assign_int(struct PropertyRNA *prop, PyObject *get_cb, PyObject *set_cb)
{
	IntPropertyGetFunc rna_get_cb = NULL;
	IntPropertySetFunc rna_set_cb = NULL;
	
	if (get_cb && get_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_get_cb = bpy_prop_int_get_cb;
		py_data[BPY_DATA_CB_SLOT_GET] = get_cb;
	}

	if (set_cb && set_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_set_cb = bpy_prop_int_set_cb;
		py_data[BPY_DATA_CB_SLOT_SET] = set_cb;
	}

	RNA_def_property_int_funcs_runtime(prop, rna_get_cb, rna_set_cb, NULL);
}

static void bpy_prop_callback_assign_int_array(struct PropertyRNA *prop, PyObject *get_cb, PyObject *set_cb)
{
	IntArrayPropertyGetFunc rna_get_cb = NULL;
	IntArrayPropertySetFunc rna_set_cb = NULL;
	
	if (get_cb && get_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_get_cb = bpy_prop_int_array_get_cb;
		py_data[BPY_DATA_CB_SLOT_GET] = get_cb;
	}

	if (set_cb && set_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_set_cb = bpy_prop_int_array_set_cb;
		py_data[BPY_DATA_CB_SLOT_SET] = set_cb;
	}

	RNA_def_property_int_array_funcs_runtime(prop, rna_get_cb, rna_set_cb, NULL);
}

static void bpy_prop_callback_assign_float(struct PropertyRNA *prop, PyObject *get_cb, PyObject *set_cb)
{
	FloatPropertyGetFunc rna_get_cb = NULL;
	FloatPropertySetFunc rna_set_cb = NULL;
	
	if (get_cb && get_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_get_cb = bpy_prop_float_get_cb;
		py_data[BPY_DATA_CB_SLOT_GET] = get_cb;
	}

	if (set_cb && set_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_set_cb = bpy_prop_float_set_cb;
		py_data[BPY_DATA_CB_SLOT_SET] = set_cb;
	}

	RNA_def_property_float_funcs_runtime(prop, rna_get_cb, rna_set_cb, NULL);
}

static void bpy_prop_callback_assign_float_array(struct PropertyRNA *prop, PyObject *get_cb, PyObject *set_cb)
{
	FloatArrayPropertyGetFunc rna_get_cb = NULL;
	FloatArrayPropertySetFunc rna_set_cb = NULL;
	
	if (get_cb && get_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_get_cb = bpy_prop_float_array_get_cb;
		py_data[BPY_DATA_CB_SLOT_GET] = get_cb;
	}

	if (set_cb && set_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_set_cb = bpy_prop_float_array_set_cb;
		py_data[BPY_DATA_CB_SLOT_SET] = set_cb;
	}

	RNA_def_property_float_array_funcs_runtime(prop, rna_get_cb, rna_set_cb, NULL);
}

static void bpy_prop_callback_assign_string(struct PropertyRNA *prop, PyObject *get_cb, PyObject *set_cb)
{
	StringPropertyGetFunc rna_get_cb = NULL;
	StringPropertyLengthFunc rna_length_cb = NULL;
	StringPropertySetFunc rna_set_cb = NULL;
	
	if (get_cb && get_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_get_cb = bpy_prop_string_get_cb;
		rna_length_cb = bpy_prop_string_length_cb;
		py_data[BPY_DATA_CB_SLOT_GET] = get_cb;
	}

	if (set_cb && set_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_set_cb = bpy_prop_string_set_cb;
		py_data[BPY_DATA_CB_SLOT_SET] = set_cb;
	}

	RNA_def_property_string_funcs_runtime(prop, rna_get_cb, rna_length_cb, rna_set_cb);
}

static void bpy_prop_callback_assign_enum(struct PropertyRNA *prop, PyObject *get_cb, PyObject *set_cb, PyObject *itemf_cb)
{
	EnumPropertyGetFunc rna_get_cb = NULL;
	EnumPropertyItemFunc rna_itemf_cb = NULL;
	EnumPropertySetFunc rna_set_cb = NULL;
	
	if (get_cb && get_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_get_cb = bpy_prop_enum_get_cb;
		py_data[BPY_DATA_CB_SLOT_GET] = get_cb;
	}

	if (set_cb && set_cb != Py_None) {
		PyObject **py_data = bpy_prop_py_data_get(prop);

		rna_set_cb = bpy_prop_enum_set_cb;
		py_data[BPY_DATA_CB_SLOT_SET] = set_cb;
	}

	if (itemf_cb && itemf_cb != Py_None) {
		rna_itemf_cb = bpy_prop_enum_itemf_cb;
		RNA_def_property_enum_py_data(prop, (void *)itemf_cb);

		/* watch out!, if a user is tricky they can probably crash blender
		 * if they manage to free the callback, take care! */
		/* Py_INCREF(itemf_cb); */
	}

	RNA_def_property_enum_funcs_runtime(prop, rna_get_cb, rna_set_cb, rna_itemf_cb);
}

/* this define runs at the start of each function and deals with 
 * returning a deferred property (to be registered later) */
#define BPY_PROPDEF_HEAD(_func)                                               \
	if (PyTuple_GET_SIZE(args) == 1) {                                        \
		PyObject *ret;                                                        \
		self = PyTuple_GET_ITEM(args, 0);                                     \
		args = PyTuple_New(0);                                                \
		ret = BPy_##_func(self, args, kw);                                    \
		Py_DECREF(args);                                                      \
		return ret;                                                           \
	}                                                                         \
	else if (PyTuple_GET_SIZE(args) > 1) {                                    \
		PyErr_SetString(PyExc_ValueError, "all args must be keywords");       \
		return NULL;                                                          \
	}                                                                         \
	srna = srna_from_self(self, #_func"(...):");                              \
	if (srna == NULL) {                                                       \
		if (PyErr_Occurred())                                                 \
			return NULL;                                                      \
		return bpy_prop_deferred_return(pymeth_##_func, kw);                  \
	} (void)0

/* terse macros for error checks shared between all funcs cant use function
 * calls because of static strings passed to pyrna_set_to_enum_bitfield */
#define BPY_PROPDEF_CHECK(_func, _property_flag_items)                        \
	if (UNLIKELY(id_len >= MAX_IDPROP_NAME)) {                                \
		PyErr_Format(PyExc_TypeError,                                         \
		             #_func"(): '%.200s' too long, max length is %d",         \
		             id, MAX_IDPROP_NAME - 1);                                \
		return NULL;                                                          \
	}                                                                         \
	if (UNLIKELY(RNA_def_property_free_identifier(srna, id) == -1)) {         \
		PyErr_Format(PyExc_TypeError,                                         \
		             #_func"(): '%s' is defined as a non-dynamic type",       \
		             id);                                                     \
		return NULL;                                                          \
	}                                                                         \
	if (UNLIKELY(pyopts && pyrna_set_to_enum_bitfield(_property_flag_items,   \
	                                         pyopts,                          \
	                                         &opts,                           \
	                                         #_func"(options={ ...}):")))     \
	{                                                                         \
		return NULL;                                                          \
	} (void)0

#define BPY_PROPDEF_SUBTYPE_CHECK(_func, _property_flag_items, _subtype)      \
	BPY_PROPDEF_CHECK(_func, _property_flag_items);                           \
	if (UNLIKELY(pysubtype && RNA_enum_value_from_id(_subtype,                \
	                                        pysubtype,                        \
	                                        &subtype) == 0))                  \
	{                                                                         \
		const char *enum_str = BPy_enum_as_string(_subtype);                  \
		PyErr_Format(PyExc_TypeError,                                         \
		             #_func"(subtype='%s'): "                                 \
		             "subtype not found in (%s)",                             \
		             pysubtype, enum_str);                                    \
		MEM_freeN((void *)enum_str);                                          \
		return NULL;                                                          \
	} (void)0


#define BPY_PROPDEF_NAME_DOC \
"   :arg name: Name used in the user interface.\n" \
"   :type name: string\n" \


#define BPY_PROPDEF_DESC_DOC \
"   :arg description: Text used for the tooltip and api documentation.\n" \
"   :type description: string\n" \


#define BPY_PROPDEF_UNIT_DOC \
"   :arg unit: Enumerator in ['NONE', 'LENGTH', 'AREA', 'VOLUME', 'ROTATION', 'TIME', 'VELOCITY', 'ACCELERATION'].\n" \
"   :type unit: string\n"	\


#define BPY_PROPDEF_UPDATE_DOC \
"   :arg update: function to be called when this value is modified,\n" \
"      This function must take 2 values (self, context) and return None.\n" \
"      *Warning* there are no safety checks to avoid infinite recursion.\n" \
"   :type update: function\n" \

#if 0
static int bpy_struct_id_used(StructRNA *srna, char *identifier)
{
	PointerRNA ptr;
	RNA_pointer_create(NULL, srna, NULL, &ptr);
	return (RNA_struct_find_property(&ptr, identifier) != NULL);
}
#endif


/* Function that sets RNA, NOTE - self is NULL when called from python,
 * but being abused from C so we can pass the srna along.
 * This isn't incorrect since its a python object - but be careful */
PyDoc_STRVAR(BPy_BoolProperty_doc,
".. function:: BoolProperty(name=\"\", "
                           "description=\"\", "
                           "default=False, "
                           "options={'ANIMATABLE'}, "
                           "subtype='NONE', "
                           "update=None, "
                           "get=None, "
                           "set=None)\n"
"\n"
"   Returns a new boolean property definition.\n"
"\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
BPY_PROPDEF_OPTIONS_DOC
BPY_PROPDEF_SUBTYPE_NUMBER_DOC
BPY_PROPDEF_UPDATE_DOC
);
static PyObject *BPy_BoolProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(BoolProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default",
		                               "options", "subtype", "update", "get", "set", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		int id_len;
		int def = 0;
		PropertyRNA *prop;
		PyObject *pyopts = NULL;
		int opts = 0;
		const char *pysubtype = NULL;
		int subtype = PROP_NONE;
		PyObject *update_cb = NULL;
		PyObject *get_cb = NULL;
		PyObject *set_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#|ssiO!sOOO:BoolProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &name, &description, &def,
		                                 &PySet_Type, &pyopts, &pysubtype,
		                                 &update_cb, &get_cb, &set_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_SUBTYPE_CHECK(BoolProperty, property_flag_items, property_subtype_number_items);

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(get_cb, "get", 1) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(set_cb, "set", 2) == -1) {
			return NULL;
		}

		prop = RNA_def_property(srna, id, PROP_BOOLEAN, subtype);
		RNA_def_property_boolean_default(prop, def);
		RNA_def_property_ui_text(prop, name ? name : id, description);

		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		bpy_prop_callback_assign_boolean(prop, get_cb, set_cb);
		RNA_def_property_duplicate_pointers(srna, prop);
	}

	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_BoolVectorProperty_doc,
".. function:: BoolVectorProperty(name=\"\", "
                                 "description=\"\", "
                                 "default=(False, False, False), "
                                 "options={'ANIMATABLE'}, "
                                 "subtype='NONE', "
                                 "size=3, "
                                 "update=None, "
                                 "get=None, "
                                 "set=None)\n"
"\n"
"   Returns a new vector boolean property definition.\n"
"\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
"   :arg default: sequence of booleans the length of *size*.\n"
"   :type default: sequence\n"
BPY_PROPDEF_OPTIONS_DOC
BPY_PROPDEF_SUBTYPE_ARRAY_DOC
"   :arg size: Vector dimensions in [1, and " STRINGIFY(PYRNA_STACK_ARRAY) "].\n"
"   :type size: int\n"
BPY_PROPDEF_UPDATE_DOC
);
static PyObject *BPy_BoolVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(BoolVectorProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default",
		                               "options", "subtype", "size", "update", "get", "set", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		int id_len;
		int def[PYRNA_STACK_ARRAY] = {0};
		int size = 3;
		PropertyRNA *prop;
		PyObject *pydef = NULL;
		PyObject *pyopts = NULL;
		int opts = 0;
		const char *pysubtype = NULL;
		int subtype = PROP_NONE;
		PyObject *update_cb = NULL;
		PyObject *get_cb = NULL;
		PyObject *set_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#|ssOO!siOOO:BoolVectorProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &name, &description, &pydef,
		                                 &PySet_Type, &pyopts, &pysubtype, &size,
		                                 &update_cb, &get_cb, &set_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_SUBTYPE_CHECK(BoolVectorProperty, property_flag_items, property_subtype_array_items);

		if (size < 1 || size > PYRNA_STACK_ARRAY) {
			PyErr_Format(PyExc_TypeError,
			             "BoolVectorProperty(size=%d): size must be between 0 and "
			             STRINGIFY(PYRNA_STACK_ARRAY), size);
			return NULL;
		}

		if (pydef && PyC_AsArray(def, pydef, size, &PyBool_Type, false, "BoolVectorProperty(default=sequence)") == -1)
			return NULL;

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(get_cb, "get", 1) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(set_cb, "set", 2) == -1) {
			return NULL;
		}

		// prop = RNA_def_boolean_array(srna, id, size, pydef ? def:NULL, name ? name : id, description);
		prop = RNA_def_property(srna, id, PROP_BOOLEAN, subtype);
		RNA_def_property_array(prop, size);
		if (pydef) RNA_def_property_boolean_array_default(prop, def);
		RNA_def_property_ui_text(prop, name ? name : id, description);

		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		bpy_prop_callback_assign_boolean_array(prop, get_cb, set_cb);
		RNA_def_property_duplicate_pointers(srna, prop);
	}
	
	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_IntProperty_doc,
".. function:: IntProperty(name=\"\", "
                          "description=\"\", "
                          "default=0, "
                          "min=-2**31, max=2**31-1, "
                          "soft_min=-2**31, soft_max=2**31-1, "
                          "step=1, "
                          "options={'ANIMATABLE'}, "
                          "subtype='NONE', "
                          "update=None, "
                          "get=None, "
                          "set=None)\n"
"\n"
"   Returns a new int property definition.\n"
"\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
BPY_PROPDEF_OPTIONS_DOC
BPY_PROPDEF_SUBTYPE_NUMBER_DOC
BPY_PROPDEF_UPDATE_DOC
);
static PyObject *BPy_IntProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(IntProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default",
		                               "min", "max", "soft_min", "soft_max",
		                               "step", "options", "subtype", "update", "get", "set", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		int id_len;
		int min = INT_MIN, max = INT_MAX, soft_min = INT_MIN, soft_max = INT_MAX, step = 1, def = 0;
		PropertyRNA *prop;
		PyObject *pyopts = NULL;
		int opts = 0;
		const char *pysubtype = NULL;
		int subtype = PROP_NONE;
		PyObject *update_cb = NULL;
		PyObject *get_cb = NULL;
		PyObject *set_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#|ssiiiiiiO!sOOO:IntProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &name, &description, &def,
		                                 &min, &max, &soft_min, &soft_max,
		                                 &step, &PySet_Type, &pyopts, &pysubtype,
		                                 &update_cb, &get_cb, &set_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_SUBTYPE_CHECK(IntProperty, property_flag_items, property_subtype_number_items);

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(get_cb, "get", 1) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(set_cb, "set", 2) == -1) {
			return NULL;
		}

		prop = RNA_def_property(srna, id, PROP_INT, subtype);
		RNA_def_property_int_default(prop, def);
		RNA_def_property_ui_text(prop, name ? name : id, description);
		RNA_def_property_range(prop, min, max);
		RNA_def_property_ui_range(prop, MAX2(soft_min, min), MIN2(soft_max, max), step, 3);

		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		bpy_prop_callback_assign_int(prop, get_cb, set_cb);
		RNA_def_property_duplicate_pointers(srna, prop);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_IntVectorProperty_doc,
".. function:: IntVectorProperty(name=\"\", "
                                "description=\"\", "
                                "default=(0, 0, 0), min=-2**31, max=2**31-1, "
                                "soft_min=-2**31, "
                                "soft_max=2**31-1, "
                                "options={'ANIMATABLE'}, "
                                "subtype='NONE', "
                                "size=3, "
                                "update=None, "
                                "get=None, "
                                "set=None)\n"
"\n"
"   Returns a new vector int property definition.\n"
"\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
"   :arg default: sequence of ints the length of *size*.\n"
"   :type default: sequence\n"
BPY_PROPDEF_OPTIONS_DOC
BPY_PROPDEF_SUBTYPE_ARRAY_DOC
"   :arg size: Vector dimensions in [1, and " STRINGIFY(PYRNA_STACK_ARRAY) "].\n"
"   :type size: int\n"
BPY_PROPDEF_UPDATE_DOC
);
static PyObject *BPy_IntVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(IntVectorProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default",
		                               "min", "max", "soft_min", "soft_max",
		                               "step", "options", "subtype", "size", "update", "get", "set", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		int id_len;
		int min = INT_MIN, max = INT_MAX, soft_min = INT_MIN, soft_max = INT_MAX, step = 1;
		int def[PYRNA_STACK_ARRAY] = {0};
		int size = 3;
		PropertyRNA *prop;
		PyObject *pydef = NULL;
		PyObject *pyopts = NULL;
		int opts = 0;
		const char *pysubtype = NULL;
		int subtype = PROP_NONE;
		PyObject *update_cb = NULL;
		PyObject *get_cb = NULL;
		PyObject *set_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#|ssOiiiiiO!siOOO:IntVectorProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &name, &description, &pydef,
		                                 &min, &max, &soft_min, &soft_max,
		                                 &step, &PySet_Type, &pyopts,
		                                 &pysubtype, &size,
		                                 &update_cb, &get_cb, &set_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_SUBTYPE_CHECK(IntVectorProperty, property_flag_items, property_subtype_array_items);

		if (size < 1 || size > PYRNA_STACK_ARRAY) {
			PyErr_Format(PyExc_TypeError,
			             "IntVectorProperty(size=%d): size must be between 0 and "
			             STRINGIFY(PYRNA_STACK_ARRAY), size);
			return NULL;
		}

		if (pydef && PyC_AsArray(def, pydef, size, &PyLong_Type, false, "IntVectorProperty(default=sequence)") == -1)
			return NULL;

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(get_cb, "get", 1) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(set_cb, "set", 2) == -1) {
			return NULL;
		}

		prop = RNA_def_property(srna, id, PROP_INT, subtype);
		RNA_def_property_array(prop, size);
		if (pydef) RNA_def_property_int_array_default(prop, def);
		RNA_def_property_range(prop, min, max);
		RNA_def_property_ui_text(prop, name ? name : id, description);
		RNA_def_property_ui_range(prop, MAX2(soft_min, min), MIN2(soft_max, max), step, 3);

		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		bpy_prop_callback_assign_int_array(prop, get_cb, set_cb);
		RNA_def_property_duplicate_pointers(srna, prop);
	}
	Py_RETURN_NONE;
}


PyDoc_STRVAR(BPy_FloatProperty_doc,
".. function:: FloatProperty(name=\"\", "
                            "description=\"\", "
                            "default=0.0, "
                            "min=sys.float_info.min, max=sys.float_info.max, "
                            "soft_min=sys.float_info.min, soft_max=sys.float_info.max, "
                            "step=3, "
                            "precision=2, "
                            "options={'ANIMATABLE'}, "
                            "subtype='NONE', "
                            "unit='NONE', "
                            "update=None, "
                            "get=None, "
                            "set=None)\n"
"\n"
"   Returns a new float property definition.\n"
"\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
BPY_PROPDEF_OPTIONS_DOC
BPY_PROPDEF_SUBTYPE_NUMBER_DOC
BPY_PROPDEF_UNIT_DOC
BPY_PROPDEF_UPDATE_DOC
"   :arg precision: Number of digits of precision to display.\n"
"   :type precision: int\n"
);
static PyObject *BPy_FloatProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(FloatProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default",
		                               "min", "max", "soft_min", "soft_max",
		                               "step", "precision", "options", "subtype",
		                               "unit", "update", "get", "set", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		int id_len;
		float min = -FLT_MAX, max = FLT_MAX, soft_min = -FLT_MAX, soft_max = FLT_MAX, step = 3, def = 0.0f;
		int precision = 2;
		PropertyRNA *prop;
		PyObject *pyopts = NULL;
		int opts = 0;
		const char *pysubtype = NULL;
		int subtype = PROP_NONE;
		const char *pyunit = NULL;
		int unit = PROP_UNIT_NONE;
		PyObject *update_cb = NULL;
		PyObject *get_cb = NULL;
		PyObject *set_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#|ssffffffiO!ssOOO:FloatProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &name, &description, &def,
		                                 &min, &max, &soft_min, &soft_max,
		                                 &step, &precision, &PySet_Type,
		                                 &pyopts, &pysubtype, &pyunit,
		                                 &update_cb, &get_cb, &set_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_SUBTYPE_CHECK(FloatProperty, property_flag_items, property_subtype_number_items);

		if (pyunit && RNA_enum_value_from_id(property_unit_items, pyunit, &unit) == 0) {
			PyErr_Format(PyExc_TypeError, "FloatProperty(unit='%s'): invalid unit", pyunit);
			return NULL;
		}

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(get_cb, "get", 1) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(set_cb, "set", 2) == -1) {
			return NULL;
		}

		prop = RNA_def_property(srna, id, PROP_FLOAT, subtype | unit);
		RNA_def_property_float_default(prop, def);
		RNA_def_property_range(prop, min, max);
		RNA_def_property_ui_text(prop, name ? name : id, description);
		RNA_def_property_ui_range(prop, MAX2(soft_min, min), MIN2(soft_max, max), step, precision);

		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		bpy_prop_callback_assign_float(prop, get_cb, set_cb);
		RNA_def_property_duplicate_pointers(srna, prop);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_FloatVectorProperty_doc,
".. function:: FloatVectorProperty(name=\"\", "
                                  "description=\"\", "
                                  "default=(0.0, 0.0, 0.0), "
                                  "min=sys.float_info.min, max=sys.float_info.max, "
                                  "soft_min=sys.float_info.min, soft_max=sys.float_info.max, "
                                  "step=3, "
                                  "precision=2, "
                                  "options={'ANIMATABLE'}, "
                                  "subtype='NONE', "
                                  "size=3, "
                                  "update=None, "
                                  "get=None, "
                                  "set=None)\n"
"\n"
"   Returns a new vector float property definition.\n"
"\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
"   :arg default: sequence of floats the length of *size*.\n"
"   :type default: sequence\n"
BPY_PROPDEF_OPTIONS_DOC
BPY_PROPDEF_SUBTYPE_ARRAY_DOC
BPY_PROPDEF_UNIT_DOC
"   :arg size: Vector dimensions in [1, and " STRINGIFY(PYRNA_STACK_ARRAY) "].\n"
"   :type size: int\n"
"   :arg precision: Number of digits of precision to display.\n"
"   :type precision: int\n"
BPY_PROPDEF_UPDATE_DOC
);
static PyObject *BPy_FloatVectorProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(FloatVectorProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default",
		                               "min", "max", "soft_min", "soft_max",
		                               "step", "precision", "options", "subtype",
		                               "unit", "size", "update", "get", "set", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		int id_len;
		float min = -FLT_MAX, max = FLT_MAX, soft_min = -FLT_MAX, soft_max = FLT_MAX, step = 3;
		float def[PYRNA_STACK_ARRAY] = {0.0f};
		int precision = 2, size = 3;
		PropertyRNA *prop;
		PyObject *pydef = NULL;
		PyObject *pyopts = NULL;
		int opts = 0;
		const char *pysubtype = NULL;
		int subtype = PROP_NONE;
		const char *pyunit = NULL;
		int unit = PROP_UNIT_NONE;
		PyObject *update_cb = NULL;
		PyObject *get_cb = NULL;
		PyObject *set_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#|ssOfffffiO!ssiOOO:FloatVectorProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &name, &description, &pydef,
		                                 &min, &max, &soft_min, &soft_max,
		                                 &step, &precision, &PySet_Type,
		                                 &pyopts, &pysubtype, &pyunit, &size,
		                                 &update_cb, &get_cb, &set_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_SUBTYPE_CHECK(FloatVectorProperty, property_flag_items, property_subtype_array_items);

		if (pyunit && RNA_enum_value_from_id(property_unit_items, pyunit, &unit) == 0) {
			PyErr_Format(PyExc_TypeError, "FloatVectorProperty(unit='%s'): invalid unit", pyunit);
			return NULL;
		}

		if (size < 1 || size > PYRNA_STACK_ARRAY) {
			PyErr_Format(PyExc_TypeError,
			             "FloatVectorProperty(size=%d): size must be between 0 and "
			             STRINGIFY(PYRNA_STACK_ARRAY), size);
			return NULL;
		}

		if (pydef && PyC_AsArray(def, pydef, size, &PyFloat_Type, false, "FloatVectorProperty(default=sequence)") == -1)
			return NULL;

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(get_cb, "get", 1) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(set_cb, "set", 2) == -1) {
			return NULL;
		}

		prop = RNA_def_property(srna, id, PROP_FLOAT, subtype | unit);
		RNA_def_property_array(prop, size);
		if (pydef) RNA_def_property_float_array_default(prop, def);
		RNA_def_property_range(prop, min, max);
		RNA_def_property_ui_text(prop, name ? name : id, description);
		RNA_def_property_ui_range(prop, MAX2(soft_min, min), MIN2(soft_max, max), step, precision);

		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		bpy_prop_callback_assign_float_array(prop, get_cb, set_cb);
		RNA_def_property_duplicate_pointers(srna, prop);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_StringProperty_doc,
".. function:: StringProperty(name=\"\", "
                             "description=\"\", "
                             "default=\"\", "
                             "maxlen=0, "
                             "options={'ANIMATABLE'}, "
                             "subtype='NONE', "
                             "update=None, "
                             "get=None, "
                             "set=None)\n"
"\n"
"   Returns a new string property definition.\n"
"\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
"   :arg default: initializer string.\n"
"   :type default: string\n"
BPY_PROPDEF_OPTIONS_DOC
BPY_PROPDEF_SUBTYPE_STRING_DOC
BPY_PROPDEF_UPDATE_DOC
);
static PyObject *BPy_StringProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(StringProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "name", "description", "default",
		                               "maxlen", "options", "subtype", "update", "get", "set", NULL};
		const char *id = NULL, *name = NULL, *description = "", *def = "";
		int id_len;
		int maxlen = 0;
		PropertyRNA *prop;
		PyObject *pyopts = NULL;
		int opts = 0;
		const char *pysubtype = NULL;
		int subtype = PROP_NONE;
		PyObject *update_cb = NULL;
		PyObject *get_cb = NULL;
		PyObject *set_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#|sssiO!sOOO:StringProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &name, &description, &def,
		                                 &maxlen, &PySet_Type, &pyopts, &pysubtype,
		                                 &update_cb, &get_cb, &set_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_SUBTYPE_CHECK(StringProperty, property_flag_items, property_subtype_string_items);

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(get_cb, "get", 1) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(set_cb, "set", 2) == -1) {
			return NULL;
		}

		prop = RNA_def_property(srna, id, PROP_STRING, subtype);
		if (maxlen != 0) RNA_def_property_string_maxlength(prop, maxlen + 1);  /* +1 since it includes null terminator */
		if (def && def[0]) RNA_def_property_string_default(prop, def);
		RNA_def_property_ui_text(prop, name ? name : id, description);

		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		bpy_prop_callback_assign_string(prop, get_cb, set_cb);
		RNA_def_property_duplicate_pointers(srna, prop);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_EnumProperty_doc,
".. function:: EnumProperty(items, "
                           "name=\"\", "
                           "description=\"\", "
                           "default=\"\", "
                           "options={'ANIMATABLE'}, "
                           "update=None, "
                           "get=None, "
                           "set=None)\n"
"\n"
"   Returns a new enumerator property definition.\n"
"\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
"   :arg default: The default value for this enum, a string from the identifiers used in *items*.\n"
"      If the *ENUM_FLAG* option is used this must be a set of such string identifiers instead.\n"
BPY_PROPDEF_OPTIONS_ENUM_DOC
"   :type options: set\n"
"   :arg items: sequence of enum items formatted:\n"
"      [(identifier, name, description, icon, number), ...] where the identifier is used\n"
"      for python access and other values are used for the interface.\n"
"      The three first elements of the tuples are mandatory.\n"
"      The forth one is either the (unique!) number id of the item or, if followed by a fith element \n"
"      (which must be the numid), an icon string identifier.\n"
"      Note the item is optional.\n"
"      For dynamic values a callback can be passed which returns a list in\n"
"      the same format as the static list.\n"
"      This function must take 2 arguments (self, context)\n"
"      WARNING: There is a known bug with using a callback,\n"
"      Python must keep a reference to the strings returned or Blender will crash.\n"
"   :type items: sequence of string tuples or a function\n"
BPY_PROPDEF_UPDATE_DOC
);
static PyObject *BPy_EnumProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(EnumProperty);
	
	if (srna) {
		static const char *kwlist[] = {"attr", "items", "name", "description", "default",
		                               "options", "update", "get", "set", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		PyObject *def = NULL;
		int id_len;
		int defvalue = 0;
		PyObject *items, *items_fast;
		EnumPropertyItem *eitems;
		PropertyRNA *prop;
		PyObject *pyopts = NULL;
		int opts = 0;
		bool is_itemf = false;
		PyObject *update_cb = NULL;
		PyObject *get_cb = NULL;
		PyObject *set_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#O|ssOO!OOO:EnumProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &items, &name, &description,
		                                 &def, &PySet_Type, &pyopts,
		                                 &update_cb, &get_cb, &set_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_CHECK(EnumProperty, property_flag_enum_items);

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(get_cb, "get", 1) == -1) {
			return NULL;
		}
		if (bpy_prop_callback_check(set_cb, "set", 2) == -1) {
			return NULL;
		}

		/* items can be a list or a callable */
		if (PyFunction_Check(items)) { /* don't use PyCallable_Check because we need the function code for errors */
			PyCodeObject *f_code = (PyCodeObject *)PyFunction_GET_CODE(items);
			if (f_code->co_argcount != 2) {
				PyErr_Format(PyExc_ValueError,
				             "EnumProperty(...): expected 'items' function to take 2 arguments, not %d",
				             f_code->co_argcount);
				return NULL;
			}

			if (def) {
				/* note, using type error here is odd but python does this for invalid arguments */
				PyErr_SetString(PyExc_TypeError,
				                "EnumProperty(...): 'default' can't be set when 'items' is a function");
				return NULL;
			}

			is_itemf = true;
			eitems = DummyRNA_NULL_items;
		}
		else {
			if (!(items_fast = PySequence_Fast(items, "EnumProperty(...): "
			                                   "expected a sequence of tuples for the enum items or a function")))
			{
				return NULL;
			}

			eitems = enum_items_from_py(items_fast, def, &defvalue,
			                            (opts & PROP_ENUM_FLAG) != 0);

			if (!eitems) {
				Py_DECREF(items_fast);
				return NULL;
			}
		}

		if (opts & PROP_ENUM_FLAG)  prop = RNA_def_enum_flag(srna, id, eitems, defvalue, name ? name : id, description);
		else                        prop = RNA_def_enum(srna, id, eitems, defvalue, name ? name : id, description);

		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		bpy_prop_callback_assign_enum(prop, get_cb, set_cb, (is_itemf ? items : NULL));
		RNA_def_property_duplicate_pointers(srna, prop);

		if (is_itemf == false) {
			/* note: this must be postponed until after #RNA_def_property_duplicate_pointers
			 * otherwise if this is a generator it may free the strings before we copy them */
			Py_DECREF(items_fast);

			MEM_freeN(eitems);
		}
	}
	Py_RETURN_NONE;
}

static StructRNA *pointer_type_from_py(PyObject *value, const char *error_prefix)
{
	StructRNA *srna;

	srna = srna_from_self(value, "");
	if (!srna) {
		if (PyErr_Occurred()) {
			PyObject *msg = PyC_ExceptionBuffer();
			const char *msg_char = _PyUnicode_AsString(msg);
			PyErr_Format(PyExc_TypeError,
			             "%.200s expected an RNA type derived from PropertyGroup, failed with: %s",
			             error_prefix, msg_char);
			Py_DECREF(msg);
		}
		else {
			PyErr_Format(PyExc_TypeError,
			             "%.200s expected an RNA type derived from PropertyGroup, failed with type '%s'",
			             error_prefix, Py_TYPE(value)->tp_name);
		}
		return NULL;
	}

	if (!RNA_struct_is_a(srna, &RNA_PropertyGroup)) {
		PyErr_Format(PyExc_TypeError,
		             "%.200s expected an RNA type derived from PropertyGroup",
		             error_prefix);
		return NULL;
	}

	return srna;
}

PyDoc_STRVAR(BPy_PointerProperty_doc,
".. function:: PointerProperty(type=\"\", "
                              "description=\"\", "
                              "options={'ANIMATABLE'}, "
                              "update=None)\n"
"\n"
"   Returns a new pointer property definition.\n"
"\n"
"   :arg type: A subclass of :class:`bpy.types.PropertyGroup`.\n"
"   :type type: class\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
BPY_PROPDEF_OPTIONS_DOC
BPY_PROPDEF_UPDATE_DOC
);
static PyObject *BPy_PointerProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(PointerProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "type", "name", "description", "options", "update", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		int id_len;
		PropertyRNA *prop;
		StructRNA *ptype;
		PyObject *type = Py_None;
		PyObject *pyopts = NULL;
		int opts = 0;
		PyObject *update_cb = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#O|ssO!O:PointerProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &type, &name, &description,
		                                 &PySet_Type, &pyopts,
		                                 &update_cb))
		{
			return NULL;
		}

		BPY_PROPDEF_CHECK(PointerProperty, property_flag_items);

		ptype = pointer_type_from_py(type, "PointerProperty(...):");
		if (!ptype)
			return NULL;

		if (bpy_prop_callback_check(update_cb, "update", 2) == -1) {
			return NULL;
		}

		prop = RNA_def_pointer_runtime(srna, id, ptype, name ? name : id, description);
		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		bpy_prop_callback_assign_update(prop, update_cb);
		RNA_def_property_duplicate_pointers(srna, prop);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_CollectionProperty_doc,
".. function:: CollectionProperty(items, "
                                 "type=\"\", "
                                 "description=\"\", "
                                 "options={'ANIMATABLE'})\n"
"\n"
"   Returns a new collection property definition.\n"
"\n"
"   :arg type: A subclass of :class:`bpy.types.PropertyGroup`.\n"
"   :type type: class\n"
BPY_PROPDEF_NAME_DOC
BPY_PROPDEF_DESC_DOC
BPY_PROPDEF_OPTIONS_DOC
);
static PyObject *BPy_CollectionProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	BPY_PROPDEF_HEAD(CollectionProperty);

	if (srna) {
		static const char *kwlist[] = {"attr", "type", "name", "description", "options", NULL};
		const char *id = NULL, *name = NULL, *description = "";
		int id_len;
		PropertyRNA *prop;
		StructRNA *ptype;
		PyObject *type = Py_None;
		PyObject *pyopts = NULL;
		int opts = 0;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s#O|ssO!:CollectionProperty",
		                                 (char **)kwlist, &id, &id_len,
		                                 &type, &name, &description,
		                                 &PySet_Type, &pyopts))
		{
			return NULL;
		}

		BPY_PROPDEF_CHECK(CollectionProperty, property_flag_items);

		ptype = pointer_type_from_py(type, "CollectionProperty(...):");
		if (!ptype)
			return NULL;

		prop = RNA_def_collection_runtime(srna, id, ptype, name ? name : id, description);
		if (pyopts) {
			if (opts & PROP_HIDDEN) RNA_def_property_flag(prop, PROP_HIDDEN);
			if ((opts & PROP_ANIMATABLE) == 0) RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
			if (opts & PROP_SKIP_SAVE) RNA_def_property_flag(prop, PROP_SKIP_SAVE);
			if (opts & PROP_LIB_EXCEPTION) RNA_def_property_flag(prop, PROP_LIB_EXCEPTION);
		}
		RNA_def_property_duplicate_pointers(srna, prop);
	}
	Py_RETURN_NONE;
}

PyDoc_STRVAR(BPy_RemoveProperty_doc,
".. function:: RemoveProperty(cls, attr="")\n"
"\n"
"   Removes a dynamically defined property.\n"
"\n"
"   :arg cls: The class containing the property (must be a positional argument).\n"
"   :type cls: type\n"
"   :arg attr: Property name (must be passed as a keyword).\n"
"   :type attr: string\n"
"\n"
".. note:: Typically this function doesn't need to be accessed directly.\n"
"   Instead use ``del cls.attr``\n"
);
static PyObject *BPy_RemoveProperty(PyObject *self, PyObject *args, PyObject *kw)
{
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) == 1) {
		PyObject *ret;
		self = PyTuple_GET_ITEM(args, 0);
		args = PyTuple_New(0);
		ret = BPy_RemoveProperty(self, args, kw);
		Py_DECREF(args);
		return ret;
	}
	else if (PyTuple_GET_SIZE(args) > 1) {
		PyErr_SetString(PyExc_ValueError, "expected one positional arg, one keyword arg");
		return NULL;
	}

	srna = srna_from_self(self, "RemoveProperty(...):");
	if (srna == NULL && PyErr_Occurred()) {
		return NULL; /* self's type was compatible but error getting the srna */
	}
	else if (srna == NULL) {
		PyErr_SetString(PyExc_TypeError, "RemoveProperty(): struct rna not available for this type");
		return NULL;
	}
	else {
		static const char *kwlist[] = {"attr", NULL};
		
		const char *id = NULL;

		if (!PyArg_ParseTupleAndKeywords(args, kw,
		                                 "s:RemoveProperty",
		                                 (char **)kwlist, &id))
		{
			return NULL;
		}

		if (RNA_def_property_free_identifier(srna, id) != 1) {
			PyErr_Format(PyExc_TypeError, "RemoveProperty(): '%s' not a defined dynamic property", id);
			return NULL;
		}
	}
	Py_RETURN_NONE;
}

static struct PyMethodDef props_methods[] = {
	{"BoolProperty", (PyCFunction)BPy_BoolProperty, METH_VARARGS | METH_KEYWORDS, BPy_BoolProperty_doc},
	{"BoolVectorProperty", (PyCFunction)BPy_BoolVectorProperty, METH_VARARGS | METH_KEYWORDS, BPy_BoolVectorProperty_doc},
	{"IntProperty", (PyCFunction)BPy_IntProperty, METH_VARARGS | METH_KEYWORDS, BPy_IntProperty_doc},
	{"IntVectorProperty", (PyCFunction)BPy_IntVectorProperty, METH_VARARGS | METH_KEYWORDS, BPy_IntVectorProperty_doc},
	{"FloatProperty", (PyCFunction)BPy_FloatProperty, METH_VARARGS | METH_KEYWORDS, BPy_FloatProperty_doc},
	{"FloatVectorProperty", (PyCFunction)BPy_FloatVectorProperty, METH_VARARGS | METH_KEYWORDS, BPy_FloatVectorProperty_doc},
	{"StringProperty", (PyCFunction)BPy_StringProperty, METH_VARARGS | METH_KEYWORDS, BPy_StringProperty_doc},
	{"EnumProperty", (PyCFunction)BPy_EnumProperty, METH_VARARGS | METH_KEYWORDS, BPy_EnumProperty_doc},
	{"PointerProperty", (PyCFunction)BPy_PointerProperty, METH_VARARGS | METH_KEYWORDS, BPy_PointerProperty_doc},
	{"CollectionProperty", (PyCFunction)BPy_CollectionProperty, METH_VARARGS | METH_KEYWORDS, BPy_CollectionProperty_doc},

	{"RemoveProperty", (PyCFunction)BPy_RemoveProperty, METH_VARARGS | METH_KEYWORDS, BPy_RemoveProperty_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef props_module = {
	PyModuleDef_HEAD_INIT,
	"bpy.props",
	"This module defines properties to extend blenders internal data, the result of these functions"
	" is used to assign properties to classes registered with blender and can't be used directly.",
	-1, /* multiple "initialization" just copies the module dict. */
	props_methods,
	NULL, NULL, NULL, NULL
};

PyObject *BPY_rna_props(void)
{
	PyObject *submodule;
	PyObject *submodule_dict;
	
	submodule = PyModule_Create(&props_module);
	PyDict_SetItemString(PyImport_GetModuleDict(), props_module.m_name, submodule);

	/* INCREF since its its assumed that all these functions return the
	 * module with a new ref like PyDict_New, since they are passed to
	 * PyModule_AddObject which steals a ref */
	Py_INCREF(submodule);
	
	/* api needs the PyObjects internally */
	submodule_dict = PyModule_GetDict(submodule);

#define ASSIGN_STATIC(_name) pymeth_##_name = PyDict_GetItemString(submodule_dict, #_name)

	ASSIGN_STATIC(BoolProperty);
	ASSIGN_STATIC(BoolVectorProperty);
	ASSIGN_STATIC(IntProperty);
	ASSIGN_STATIC(IntVectorProperty);
	ASSIGN_STATIC(FloatProperty);
	ASSIGN_STATIC(FloatVectorProperty);
	ASSIGN_STATIC(StringProperty);
	ASSIGN_STATIC(EnumProperty);
	ASSIGN_STATIC(PointerProperty);
	ASSIGN_STATIC(CollectionProperty);
	ASSIGN_STATIC(RemoveProperty);
	
	return submodule;
}
