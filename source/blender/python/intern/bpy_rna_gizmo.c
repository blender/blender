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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_rna_gizmo.c
 *  \ingroup pythonintern
 *
 * .
 */

#include <Python.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"

#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"

#include "bpy_capi_utils.h"
#include "bpy_rna_gizmo.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"
#include "RNA_enum_types.h"

#include "bpy_rna.h"


/* -------------------------------------------------------------------- */
/** \name Gizmo Target Property Define API
 * \{ */

enum {
	BPY_GIZMO_FN_SLOT_GET = 0,
	BPY_GIZMO_FN_SLOT_SET,
	BPY_GIZMO_FN_SLOT_RANGE_GET,
};
#define BPY_GIZMO_FN_SLOT_LEN (BPY_GIZMO_FN_SLOT_RANGE_GET + 1)

struct BPyGizmoHandlerUserData {

	PyObject *fn_slots[BPY_GIZMO_FN_SLOT_LEN];
};

static void py_rna_gizmo_handler_get_cb(
        const wmGizmo *UNUSED(gz), wmGizmoProperty *gz_prop,
        void *value_p)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();

	struct BPyGizmoHandlerUserData *data = gz_prop->custom_func.user_data;
	PyObject *ret = PyObject_CallObject(data->fn_slots[BPY_GIZMO_FN_SLOT_GET], NULL);
	if (ret == NULL) {
		goto fail;
	}

	if (gz_prop->type->data_type == PROP_FLOAT) {
		float *value = value_p;
		if (gz_prop->type->array_length == 1) {
			if ((*value = PyFloat_AsDouble(ret)) == -1.0f && PyErr_Occurred()) {
				goto fail;
			}
		}
		else {
			if (PyC_AsArray(value, ret, gz_prop->type->array_length, &PyFloat_Type, false,
			                "Gizmo get callback: ") == -1)
			{
				goto fail;
			}
		}
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "internal error, unsupported type");
		goto fail;
	}

	Py_DECREF(ret);

	PyGILState_Release(gilstate);
	return;

fail:
	PyErr_Print();
	PyErr_Clear();

	PyGILState_Release(gilstate);
}

static void py_rna_gizmo_handler_set_cb(
        const wmGizmo *UNUSED(gz), wmGizmoProperty *gz_prop,
        const void *value_p)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();

	struct BPyGizmoHandlerUserData *data = gz_prop->custom_func.user_data;

	PyObject *args = PyTuple_New(1);

	if (gz_prop->type->data_type == PROP_FLOAT) {
		const float *value = value_p;
		PyObject *py_value;
		if (gz_prop->type->array_length == 1) {
			py_value = PyFloat_FromDouble(*value);
		}
		else {
			py_value =  PyC_Tuple_PackArray_F32(value, gz_prop->type->array_length);
		}
		if (py_value == NULL) {
			goto fail;
		}
		PyTuple_SET_ITEM(args, 0, py_value);
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "internal error, unsupported type");
		goto fail;
	}

	PyObject *ret = PyObject_CallObject(data->fn_slots[BPY_GIZMO_FN_SLOT_SET], args);
	if (ret == NULL) {
		goto fail;
	}
	Py_DECREF(ret);

	PyGILState_Release(gilstate);
	return;

fail:
	PyErr_Print();
	PyErr_Clear();

	Py_DECREF(args);

	PyGILState_Release(gilstate);
}

static void py_rna_gizmo_handler_range_get_cb(
        const wmGizmo *UNUSED(gz), wmGizmoProperty *gz_prop,
        void *value_p)
{
	struct BPyGizmoHandlerUserData *data = gz_prop->custom_func.user_data;

	PyGILState_STATE gilstate = PyGILState_Ensure();

	PyObject *ret = PyObject_CallObject(data->fn_slots[BPY_GIZMO_FN_SLOT_RANGE_GET], NULL);
	if (ret == NULL) {
		goto fail;
	}

	if (!PyTuple_Check(ret)) {
		PyErr_Format(PyExc_TypeError,
		             "Expected a tuple, not %.200s",
		             Py_TYPE(ret)->tp_name);
		goto fail;
	}

	if (PyTuple_GET_SIZE(ret) != 2) {
		PyErr_Format(PyExc_TypeError,
		             "Expected a tuple of size 2, not %d",
		             PyTuple_GET_SIZE(ret));
		goto fail;
	}

	if (gz_prop->type->data_type == PROP_FLOAT) {
		float range[2];
		for (int i = 0; i < 2; i++) {
			if (((range[i] = PyFloat_AsDouble(PyTuple_GET_ITEM(ret, i))) == -1.0f && PyErr_Occurred()) == 0) {
				/* pass */
			}
			else {
				goto fail;
			}
		}
		memcpy(value_p, range, sizeof(range));
	}
	else {
		PyErr_SetString(PyExc_AttributeError, "internal error, unsupported type");
		goto fail;
	}

	Py_DECREF(ret);
	PyGILState_Release(gilstate);
	return;

fail:
	Py_XDECREF(ret);

	PyErr_Print();
	PyErr_Clear();

	PyGILState_Release(gilstate);
}

static void py_rna_gizmo_handler_free_cb(
        const wmGizmo *UNUSED(gz), wmGizmoProperty *gz_prop)
{
	struct BPyGizmoHandlerUserData *data = gz_prop->custom_func.user_data;

	PyGILState_STATE gilstate = PyGILState_Ensure();
	for (int i = 0; i < BPY_GIZMO_FN_SLOT_LEN; i++) {
		Py_XDECREF(data->fn_slots[i]);
	}
	PyGILState_Release(gilstate);

	MEM_freeN(data);

}

PyDoc_STRVAR(bpy_gizmo_target_set_handler_doc,
".. method:: target_set_handler(target, get, set, range=None):\n"
"\n"
"   Assigns callbacks to a gizmos property.\n"
"\n"
"   :arg get: Function that returns the value for this property (single value or sequence).\n"
"   :type get: callable\n"
"   :arg set: Function that takes a single value argument and applies it.\n"
"   :type set: callable\n"
"   :arg range: Function that returns a (min, max) tuple for gizmos that use a range.\n"
"   :type range: callable\n"
);
static PyObject *bpy_gizmo_target_set_handler(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();

	struct {
		PyObject *self;
		char *target;
		PyObject *py_fn_slots[BPY_GIZMO_FN_SLOT_LEN];
	} params = {
		.self = NULL,
		.target = NULL,
		.py_fn_slots = {NULL},
	};

	/* Note: this is a counter-part to functions:
	 * 'Gizmo.target_set_prop & target_set_operator'
	 * (see: rna_wm_gizmo_api.c). conventions should match. */
	static const char * const _keywords[] = {"self", "target", "get", "set", "range", NULL};
	static _PyArg_Parser _parser = {"Os|$OOO:target_set_handler", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kw, &_parser,
	        &params.self,
	        &params.target,
	        &params.py_fn_slots[BPY_GIZMO_FN_SLOT_GET],
	        &params.py_fn_slots[BPY_GIZMO_FN_SLOT_SET],
	        &params.py_fn_slots[BPY_GIZMO_FN_SLOT_RANGE_GET]))
	{
		goto fail;
	}

	wmGizmo *gz = ((BPy_StructRNA *)params.self)->ptr.data;

	const wmGizmoPropertyType *gz_prop_type =
	        WM_gizmotype_target_property_find(gz->type, params.target);
	if (gz_prop_type == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "Gizmo target property '%s.%s' not found",
		             gz->type->idname, params.target);
		goto fail;
	}

	{
		const int slots_required = 2;
		const int slots_start = 2;
		for (int i = 0; i < BPY_GIZMO_FN_SLOT_LEN; i++) {
			if (params.py_fn_slots[i] == NULL) {
				if (i < slots_required) {
					PyErr_Format(PyExc_ValueError, "Argument '%s' not given", _keywords[slots_start + i]);
					goto fail;
				}
			}
			else if (!PyCallable_Check(params.py_fn_slots[i])) {
				PyErr_Format(PyExc_ValueError, "Argument '%s' not callable", _keywords[slots_start + i]);
				goto fail;
			}
		}
	}

	struct BPyGizmoHandlerUserData *data = MEM_callocN(sizeof(*data), __func__);

	for (int i = 0; i < BPY_GIZMO_FN_SLOT_LEN; i++) {
		data->fn_slots[i] = params.py_fn_slots[i];
		Py_XINCREF(params.py_fn_slots[i]);
	}

	WM_gizmo_target_property_def_func_ptr(
	        gz, gz_prop_type,
	        &(const struct wmGizmoPropertyFnParams) {
	            .value_get_fn = py_rna_gizmo_handler_get_cb,
	            .value_set_fn = py_rna_gizmo_handler_set_cb,
	            .range_get_fn = py_rna_gizmo_handler_range_get_cb,
	            .free_fn = py_rna_gizmo_handler_free_cb,
	            .user_data = data,
	        });

	PyGILState_Release(gilstate);

	Py_RETURN_NONE;

fail:
	PyGILState_Release(gilstate);
	return NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo Target Property Access API
 * \{ */

PyDoc_STRVAR(bpy_gizmo_target_get_value_doc,
".. method:: target_get_value(target):\n"
"\n"
"   Get the value of this target property.\n"
"\n"
"   :arg target: Target property name.\n"
"   :type target: string\n"
"   :return: The value of the target property.\n"
"   :rtype: Single value or array based on the target type\n"
);
static PyObject *bpy_gizmo_target_get_value(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	struct {
		PyObject *self;
		char *target;
	} params = {
		.self = NULL,
		.target = NULL,
	};

	static const char * const _keywords[] = {"self", "target", NULL};
	static _PyArg_Parser _parser = {"Os:target_get_value", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kw, &_parser,
	        &params.self,
	        &params.target))
	{
		goto fail;
	}

	wmGizmo *gz = ((BPy_StructRNA *)params.self)->ptr.data;

	wmGizmoProperty *gz_prop =
	        WM_gizmo_target_property_find(gz, params.target);
	if (gz_prop == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "Gizmo target property '%s.%s' not found",
		             gz->type->idname, params.target);
		goto fail;
	}

	const int array_len = WM_gizmo_target_property_array_length(gz, gz_prop);
	switch (gz_prop->type->data_type) {
		case PROP_FLOAT:
		{
			if (array_len != 0) {
				float *value = BLI_array_alloca(value, array_len);
				WM_gizmo_target_property_value_get_array(gz, gz_prop, value);
				return PyC_Tuple_PackArray_F32(value, array_len);
			}
			else {
				float value = WM_gizmo_target_property_value_get(gz, gz_prop);
				return PyFloat_FromDouble(value);
			}
			break;
		}
		default:
		{
			PyErr_SetString(PyExc_RuntimeError, "Not yet supported type");
			goto fail;
		}
	}

fail:
	return NULL;
}

PyDoc_STRVAR(bpy_gizmo_target_set_value_doc,
".. method:: target_set_value(target):\n"
"\n"
"   Set the value of this target property.\n"
"\n"
"   :arg target: Target property name.\n"
"   :type target: string\n"
);
static PyObject *bpy_gizmo_target_set_value(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	struct {
		PyObject *self;
		char *target;
		PyObject *value;
	} params = {
		.self = NULL,
		.target = NULL,
		.value = NULL,
	};

	static const char * const _keywords[] = {"self", "target", "value", NULL};
	static _PyArg_Parser _parser = {"OsO:target_set_value", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kw, &_parser,
	        &params.self,
	        &params.target,
	        &params.value))
	{
		goto fail;
	}

	wmGizmo *gz = ((BPy_StructRNA *)params.self)->ptr.data;

	wmGizmoProperty *gz_prop =
	        WM_gizmo_target_property_find(gz, params.target);
	if (gz_prop == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "Gizmo target property '%s.%s' not found",
		             gz->type->idname, params.target);
		goto fail;
	}

	const int array_len = WM_gizmo_target_property_array_length(gz, gz_prop);
	switch (gz_prop->type->data_type) {
		case PROP_FLOAT:
		{
			if (array_len != 0) {
				float *value = BLI_array_alloca(value, array_len);
				if (PyC_AsArray(value, params.value, gz_prop->type->array_length, &PyFloat_Type, false,
				                "Gizmo target property array") == -1)
				{
					goto fail;
				}
				WM_gizmo_target_property_value_set_array(BPy_GetContext(), gz, gz_prop, value);
			}
			else {
				float value;
				if ((value = PyFloat_AsDouble(params.value)) == -1.0f && PyErr_Occurred()) {
					goto fail;
				}
				WM_gizmo_target_property_value_set(BPy_GetContext(), gz, gz_prop, value);
			}
			Py_RETURN_NONE;
		}
		default:
		{
			PyErr_SetString(PyExc_RuntimeError, "Not yet supported type");
			goto fail;
		}
	}

fail:
	return NULL;
}


PyDoc_STRVAR(bpy_gizmo_target_get_range_doc,
".. method:: target_get_range(target):\n"
"\n"
"   Get the range for this target property.\n"
"\n"
"   :arg target: Target property name.\n"
"   :Get the range for this target property"
"   :return: The range of this property (min, max).\n"
"   :rtype: tuple pair.\n"
);
static PyObject *bpy_gizmo_target_get_range(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	struct {
		PyObject *self;
		char *target;
	} params = {
		.self = NULL,
		.target = NULL,
	};

	static const char * const _keywords[] = {"self", "target", NULL};
	static _PyArg_Parser _parser = {"Os:target_get_range", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kw, &_parser,
	        &params.self,
	        &params.target))
	{
		goto fail;
	}

	wmGizmo *gz = ((BPy_StructRNA *)params.self)->ptr.data;

	wmGizmoProperty *gz_prop =
	        WM_gizmo_target_property_find(gz, params.target);
	if (gz_prop == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "Gizmo target property '%s.%s' not found",
		             gz->type->idname, params.target);
		goto fail;
	}

	switch (gz_prop->type->data_type) {
		case PROP_FLOAT:
		{
			float range[2];
			WM_gizmo_target_property_range_get(gz, gz_prop, range);
			return PyC_Tuple_PackArray_F32(range, 2);
		}
		default:
		{
			PyErr_SetString(PyExc_RuntimeError, "Not yet supported type");
			goto fail;
		}
	}

fail:
	return NULL;
}

/** \} */

int BPY_rna_gizmo_module(PyObject *mod_par)
{
	static PyMethodDef method_def_array[] = {
		/* Gizmo Target Property Define API */
		{"target_set_handler", (PyCFunction)bpy_gizmo_target_set_handler,
		 METH_VARARGS | METH_KEYWORDS, bpy_gizmo_target_set_handler_doc},
		/* Gizmo Target Property Access API */
		{"target_get_value", (PyCFunction)bpy_gizmo_target_get_value,
		 METH_VARARGS | METH_KEYWORDS, bpy_gizmo_target_get_value_doc},
		{"target_set_value", (PyCFunction)bpy_gizmo_target_set_value,
		 METH_VARARGS | METH_KEYWORDS, bpy_gizmo_target_set_value_doc},
		{"target_get_range", (PyCFunction)bpy_gizmo_target_get_range,
		 METH_VARARGS | METH_KEYWORDS, bpy_gizmo_target_get_range_doc},
		/* no sentinel needed. */
	};

	for (int i = 0; i < ARRAY_SIZE(method_def_array); i++) {
		PyMethodDef *m = &method_def_array[i];
		PyObject *func = PyCFunction_New(m, NULL);
		PyObject *func_inst = PyInstanceMethod_New(func);
		char name_prefix[128];
		PyOS_snprintf(name_prefix, sizeof(name_prefix), "_rna_gizmo_%s", m->ml_name);
		/* TODO, return a type that binds nearly to a method. */
		PyModule_AddObject(mod_par, name_prefix, func_inst);
	}

	return 0;
}
