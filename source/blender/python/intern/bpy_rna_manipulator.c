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

/** \file blender/python/intern/bpy_rna_manipulator.c
 *  \ingroup pythonintern
 *
 * .
 */

#include <Python.h>
#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BKE_main.h"

#include "WM_api.h"
#include "WM_types.h"

#include "bpy_util.h"
#include "bpy_rna_manipulator.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"
#include "RNA_enum_types.h"

#include "bpy_rna.h"

enum {
	BPY_MANIPULATOR_FN_SLOT_GET = 0,
	BPY_MANIPULATOR_FN_SLOT_SET,
	BPY_MANIPULATOR_FN_SLOT_RANGE_GET,
};
#define BPY_MANIPULATOR_FN_SLOT_LEN (BPY_MANIPULATOR_FN_SLOT_RANGE_GET + 1)

struct BPyManipulatorHandlerUserData {

	PyObject *fn_slots[BPY_MANIPULATOR_FN_SLOT_LEN];
};

static void py_rna_manipulator_handler_get_cb(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();

	struct BPyManipulatorHandlerUserData *data = mpr_prop->custom_func.user_data;
	PyObject *ret = PyObject_CallObject(data->fn_slots[BPY_MANIPULATOR_FN_SLOT_GET], NULL);
	if (ret == NULL) {
		goto fail;
	}

	if (mpr_prop->type->data_type == PROP_FLOAT) {
		float *value = value_p;
		if (mpr_prop->type->array_length == 1) {
			if (((*value = PyFloat_AsDouble(ret)) == -1.0f && PyErr_Occurred()) == 0) {
				goto fail;
			}
		}
		else {
			if (PyC_AsArray(value, ret, mpr_prop->type->array_length, &PyFloat_Type, false,
			                "Manipulator get callback: ") == -1)
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

static void py_rna_manipulator_handler_set_cb(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        const void *value_p)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();

	struct BPyManipulatorHandlerUserData *data = mpr_prop->custom_func.user_data;

	PyObject *args = PyTuple_New(1);

	if (mpr_prop->type->data_type == PROP_FLOAT) {
		const float *value = value_p;
		PyObject *py_value;
		if (mpr_prop->type->array_length == 1) {
			py_value = PyFloat_FromDouble(*value);
		}
		else {
			py_value =  PyC_FromArray((void *)value, mpr_prop->type->array_length, &PyFloat_Type, false,
			                          "Manipulator set callback: ");
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

	PyObject *ret = PyObject_CallObject(data->fn_slots[BPY_MANIPULATOR_FN_SLOT_SET], args);
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

static void py_rna_manipulator_handler_range_get_cb(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop,
        void *value_p)
{
	struct BPyManipulatorHandlerUserData *data = mpr_prop->custom_func.user_data;

	PyGILState_STATE gilstate = PyGILState_Ensure();

	PyObject *ret = PyObject_CallObject(data->fn_slots[BPY_MANIPULATOR_FN_SLOT_RANGE_GET], NULL);
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

	if (mpr_prop->type->data_type == PROP_FLOAT) {
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

static void py_rna_manipulator_handler_free_cb(
        const wmManipulator *UNUSED(mpr), wmManipulatorProperty *mpr_prop)
{
	struct BPyManipulatorHandlerUserData *data = mpr_prop->custom_func.user_data;

	PyGILState_STATE gilstate = PyGILState_Ensure();
	for (int i = 0; i < BPY_MANIPULATOR_FN_SLOT_LEN; i++) {
		Py_XDECREF(data->fn_slots[i]);
	}
	PyGILState_Release(gilstate);

	MEM_freeN(data);

}

PyDoc_STRVAR(bpy_manipulator_target_set_handler_doc,
".. method:: target_set_handler(target, get, set, range=None):\n"
"\n"
"   Assigns callbacks to a manipulators property.\n"
"\n"
"   :arg get: Function that returns the value for this property (single value or sequence).\n"
"   :type get: callable\n"
"   :arg set: Function that takes a single value argument and applies it.\n"
"   :type set: callable\n"
"   :arg range: Function that returns a (min, max) tuple for manipulators that use a range.\n"
"   :type range: callable\n"
);
static PyObject *bpy_manipulator_target_set_handler(PyObject *UNUSED(self), PyObject *args, PyObject *kwds)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();

	struct {
		PyObject *self;
		char *target;
		PyObject *py_fn_slots[BPY_MANIPULATOR_FN_SLOT_LEN];
	} params = {
		.self = NULL,
		.target = NULL,
		.py_fn_slots = {NULL},
	};

	/* Note: this is a counter-part to functions:
	 * 'Manipulator.target_set_prop & target_set_operator'
	 * (see: rna_wm_manipulator_api.c). conventions should match. */
	static const char * const _keywords[] = {"self", "target", "get", "set", "range", NULL};
#define KW_FMT "Os|$OOO:target_set_handler"
#if PY_VERSION_HEX >= 0x03070000
	static _PyArg_Parser _parser = {KW_FMT, _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kwds,
	        &_parser,
	        &params.self,
	        &params.target,
	        &params.py_fn_slots[BPY_MANIPULATOR_FN_SLOT_GET],
	        &params.py_fn_slots[BPY_MANIPULATOR_FN_SLOT_SET],
	        &params.py_fn_slots[BPY_MANIPULATOR_FN_SLOT_RANGE_GET]))
#else
	if (!PyArg_ParseTupleAndKeywords(
	        args, kwds,
	        KW_FMT, (char **)_keywords,
	        &params.self,
	        &params.target,
	        &params.py_fn_slots[BPY_MANIPULATOR_FN_SLOT_GET],
	        &params.py_fn_slots[BPY_MANIPULATOR_FN_SLOT_SET],
	        &params.py_fn_slots[BPY_MANIPULATOR_FN_SLOT_RANGE_GET]))
#endif
	{
		goto fail;
	}
#undef KW_FMT

	wmManipulator *mpr = ((BPy_StructRNA *)params.self)->ptr.data;

	const wmManipulatorPropertyType *mpr_prop_type =
	        WM_manipulatortype_target_property_find(mpr->type, params.target);
	if (mpr_prop_type == NULL) {
		PyErr_Format(PyExc_ValueError,
		             "Manipulator target property '%s.%s' not found",
		             mpr->type->idname, params.target);
		goto fail;
	}

	{
		const int slots_required = 2;
		const int slots_start = 2;
		for (int i = 0; i < BPY_MANIPULATOR_FN_SLOT_LEN; i++) {
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

	struct BPyManipulatorHandlerUserData *data = MEM_callocN(sizeof(*data), __func__);

	for (int i = 0; i < BPY_MANIPULATOR_FN_SLOT_LEN; i++) {
		data->fn_slots[i] = params.py_fn_slots[i];
		Py_XINCREF(params.py_fn_slots[i]);
	}

	WM_manipulator_target_property_def_func_ptr(
	        mpr, mpr_prop_type,
	        &(const struct wmManipulatorPropertyFnParams) {
	            .value_get_fn = py_rna_manipulator_handler_get_cb,
	            .value_set_fn = py_rna_manipulator_handler_set_cb,
	            .range_get_fn = py_rna_manipulator_handler_range_get_cb,
	            .free_fn = py_rna_manipulator_handler_free_cb,
	            .user_data = data,
	        });

	PyGILState_Release(gilstate);

	Py_RETURN_NONE;

fail:
	PyGILState_Release(gilstate);
	return NULL;
}

int BPY_rna_manipulator_module(PyObject *mod_par)
{
	static PyMethodDef method_def = {
	    "target_set_handler", (PyCFunction)bpy_manipulator_target_set_handler, METH_VARARGS | METH_KEYWORDS,
	    bpy_manipulator_target_set_handler_doc};

	PyObject *func = PyCFunction_New(&method_def, NULL);
	PyObject *func_inst = PyInstanceMethod_New(func);


	/* TODO, return a type that binds nearly to a method. */
	PyModule_AddObject(mod_par, "_rna_manipulator_target_set_handler", func_inst);

	return 0;
}


