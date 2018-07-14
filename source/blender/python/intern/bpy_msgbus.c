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

/** \file blender/python/intern/bpy_msgbus.c
 *  \ingroup pythonintern
 * This file defines '_bpy_msgbus' module, exposed as 'bpy.msgbus'.
 */

#include <Python.h>

#include "../generic/python_utildefines.h"
#include "../generic/py_capi_utils.h"
#include "../mathutils/mathutils.h"

#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"
#include "WM_message.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "bpy_capi_utils.h"
#include "bpy_rna.h"
#include "bpy_intern_string.h"
#include "bpy_gizmo_wrap.h"  /* own include */


#include "bpy_msgbus.h"  /* own include */


/* -------------------------------------------------------------------- */
/** \name Internal Utils
 * \{ */

#define BPY_MSGBUS_RNA_MSGKEY_DOC \
"   :arg key: Represents the type of data being subscribed to\n" \
"\n" \
"      Arguments include\n" \
"      - :class:`bpy.types.Property` instance.\n" \
"      - :class:`bpy.types.Struct` type.\n" \
"      - (:class:`bpy.types.Struct`, str) type and property name.\n" \
"   :type key: Muliple\n"

/**
 * There are multiple ways we can get RNA from Python,
 * it's also possible to register a type instead of an instance.
 *
 * This function handles converting Python to RNA subscription information.
 *
 * \param py_sub: See #BPY_MSGBUS_RNA_MSGKEY_DOC for description.
 * \param msg_key_params: Message key with all members zeroed out.
 * \return -1 on failure, 0 on success.
 */
static int py_msgbus_rna_key_from_py(
        PyObject *py_sub,
        wmMsgParams_RNA *msg_key_params,
        const char *error_prefix)
{

	/* Allow common case, object rotation, location - etc. */
	if (BaseMathObject_CheckExact(py_sub)) {
		BaseMathObject *py_sub_math = (BaseMathObject *)py_sub;
		if (py_sub_math->cb_user == NULL) {
			PyErr_Format(
			        PyExc_TypeError,
			        "%s: math argument has no owner",
			        error_prefix);
			return -1;
		}
		py_sub = py_sub_math->cb_user;
		/* Common case will use BPy_PropertyRNA_Check below. */
	}

	if (BPy_PropertyRNA_Check(py_sub)) {
		BPy_PropertyRNA *data_prop = (BPy_PropertyRNA *)py_sub;
		PYRNA_PROP_CHECK_INT(data_prop);
		msg_key_params->ptr = data_prop->ptr;
		msg_key_params->prop = data_prop->prop;
	}
	else if (BPy_StructRNA_Check(py_sub)) {
		/* note, this isn't typically used since we don't edit structs directly. */
		BPy_StructRNA *data_srna = (BPy_StructRNA *)py_sub;
		PYRNA_STRUCT_CHECK_INT(data_srna);
		msg_key_params->ptr = data_srna->ptr;
	}
	/* TODO - property / type, not instance. */
	else if (PyType_Check(py_sub)) {
		StructRNA *data_type = pyrna_struct_as_srna(py_sub, false, error_prefix);
		if (data_type == NULL) {
			return -1;
		}
		msg_key_params->ptr.type = data_type;
	}
	else if (PyTuple_CheckExact(py_sub)) {
		if (PyTuple_GET_SIZE(py_sub) == 2) {
			PyObject *data_type_py = PyTuple_GET_ITEM(py_sub, 0);
			PyObject *data_prop_py = PyTuple_GET_ITEM(py_sub, 1);
			StructRNA *data_type = pyrna_struct_as_srna(data_type_py, false, error_prefix);
			if (data_type == NULL) {
				return -1;
			}
			if (!PyUnicode_CheckExact(data_prop_py)) {
				PyErr_Format(
				        PyExc_TypeError,
				        "%s: expected property to be a string",
				        error_prefix);
				return -1;
			}
			PointerRNA data_type_ptr = { .type = data_type, };
			const char *data_prop_str = _PyUnicode_AsString(data_prop_py);
			PropertyRNA *data_prop = RNA_struct_find_property(&data_type_ptr, data_prop_str);

			if (data_prop == NULL) {
				PyErr_Format(
				        PyExc_TypeError,
				        "%s: struct %.200s does not contain property %.200s",
				        error_prefix,
				        RNA_struct_identifier(data_type),
				        data_prop_str);
				return -1;
			}

			msg_key_params->ptr.type = data_type;
			msg_key_params->prop = data_prop;
		}
		else {
			PyErr_Format(
			        PyExc_ValueError,
			        "%s: Expected a pair (type, property_id)",
			        error_prefix);
			return -1;
		}
	}
	return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Internal Callbacks
 * \{ */

#define BPY_MSGBUS_USER_DATA_LEN 2

/* Follow wmMsgNotifyFn spec */
static void bpy_msgbus_notify(
        bContext *C, wmMsgSubscribeKey *UNUSED(msg_key), wmMsgSubscribeValue *msg_val)
{
	PyGILState_STATE gilstate;
	bpy_context_set(C, &gilstate);

	PyObject *user_data = msg_val->user_data;
	BLI_assert(PyTuple_GET_SIZE(user_data) == BPY_MSGBUS_USER_DATA_LEN);

	PyObject *callback_args = PyTuple_GET_ITEM(user_data, 0);
	PyObject *callback_notify = PyTuple_GET_ITEM(user_data, 1);

	const bool is_write_ok = pyrna_write_check();
	if (!is_write_ok) {
		pyrna_write_set(true);
	}

	PyObject *ret = PyObject_CallObject(callback_notify, callback_args);

	if (ret == NULL) {
		PyC_Err_PrintWithFunc(callback_notify);
	}
	else {
		if (ret != Py_None) {
			PyErr_SetString(PyExc_ValueError, "the return value must be None");
			PyC_Err_PrintWithFunc(callback_notify);
		}
		Py_DECREF(ret);
	}

	bpy_context_clear(C, &gilstate);

	if (!is_write_ok) {
		pyrna_write_set(false);
	}
}

/* Follow wmMsgSubscribeValueFreeDataFn spec */
static void bpy_msgbus_subscribe_value_free_data(
        struct wmMsgSubscribeKey *UNUSED(msg_key), struct wmMsgSubscribeValue *msg_val)
{
	PyGILState_STATE gilstate = PyGILState_Ensure();
	Py_DECREF(msg_val->owner);
	Py_DECREF(msg_val->user_data);
	PyGILState_Release(gilstate);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public Message Bus API
 * \{ */

PyDoc_STRVAR(bpy_msgbus_subscribe_rna_doc,
".. function:: subscribe_rna(data, owner, args, notify)\n"
"\n"
BPY_MSGBUS_RNA_MSGKEY_DOC
"   :arg owner: Handle for this subscription (compared by identity).\n"
"   :type owner: Any type.\n"
"\n"
"   Returns a new vector int property definition.\n"
);
static PyObject *bpy_msgbus_subscribe_rna(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	const char *error_prefix = "subscribe_rna";
	PyObject *py_sub = NULL;
	PyObject *py_owner = NULL;
	PyObject *callback_args = NULL;
	PyObject *callback_notify = NULL;

	enum {
		IS_PERSISTENT = (1 << 0),
	};
	PyObject *py_options = NULL;
	EnumPropertyItem py_options_enum[] = {
		{IS_PERSISTENT, "PERSISTENT", 0, ""},
		{0, NULL, 0, NULL, NULL}
	};
	int options = 0;

	static const char *_keywords[] = {
		"key",
		"owner",
		"args",
		"notify",
		"options",
		NULL,
	};
	static _PyArg_Parser _parser = {"$OOO!OO!:subscribe_rna", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kw, &_parser,
	        &py_sub, &py_owner,
	        &PyTuple_Type, &callback_args,
	        &callback_notify,
	        &PySet_Type, &py_options))
	{
		return NULL;
	}

	if (py_options &&
	    (pyrna_set_to_enum_bitfield(py_options_enum, py_options, &options, error_prefix)) == -1)
	{
		return NULL;
	}

	/* Note: we may want to have a way to pass this in. */
	bContext *C = (bContext *)BPy_GetContext();
	struct wmMsgBus *mbus = CTX_wm_message_bus(C);
	wmMsgParams_RNA msg_key_params = {{{0}}};

	wmMsgSubscribeValue msg_val_params = {0};

	if (py_msgbus_rna_key_from_py(py_sub, &msg_key_params, error_prefix) == -1) {
		return NULL;
	}

	if (!PyFunction_Check(callback_notify)) {
		PyErr_Format(
		        PyExc_TypeError,
		        "notify expects a function, found %.200s",
		        Py_TYPE(callback_notify)->tp_name);
		return NULL;
	}

	if (options != 0) {
		if (options & IS_PERSISTENT) {
			msg_val_params.is_persistent = true;
		}
	}

	/* owner can be anything. */
	{
		msg_val_params.owner = py_owner;
		Py_INCREF(py_owner);
	}

	{
		PyObject *user_data = PyTuple_New(2);
		PyTuple_SET_ITEMS(
		        user_data,
		        Py_INCREF_RET(callback_args),
		        Py_INCREF_RET(callback_notify));
		msg_val_params.user_data = user_data;
	}

	msg_val_params.notify = bpy_msgbus_notify;
	msg_val_params.free_data = bpy_msgbus_subscribe_value_free_data;

	WM_msg_subscribe_rna_params(mbus, &msg_key_params, &msg_val_params, __func__);

	WM_msg_dump(mbus, __func__);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_msgbus_publish_rna_doc,
".. function:: publish_rna(data, owner, args, notify)\n"
"\n"
BPY_MSGBUS_RNA_MSGKEY_DOC
"\n"
"   Notify subscribers of changes to this property\n"
"   (this typically doesn't need to be called explicitly since changes will automatically publish updates).\n"
"   In some cases it may be useful to publish changes explicitly using more general keys.\n"
);
static PyObject *bpy_msgbus_publish_rna(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
	const char *error_prefix = "publish_rna";
	PyObject *py_sub = NULL;

	static const char *_keywords[] = {
		"key",
		NULL,
	};
	static _PyArg_Parser _parser = {"$O:publish_rna", _keywords, 0};
	if (!_PyArg_ParseTupleAndKeywordsFast(
	        args, kw, &_parser,
	        &py_sub))
	{
		return NULL;
	}

	/* Note: we may want to have a way to pass this in. */
	bContext *C = (bContext *)BPy_GetContext();
	struct wmMsgBus *mbus = CTX_wm_message_bus(C);
	wmMsgParams_RNA msg_key_params = {{{0}}};

	if (py_msgbus_rna_key_from_py(py_sub, &msg_key_params, error_prefix) == -1) {
		return NULL;
	}

	WM_msg_publish_rna_params(mbus, &msg_key_params);

	Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_msgbus_clear_by_owner_doc,
".. function:: clear_by_owner(owner)\n"
"\n"
"   Clear all subscribers using this owner.\n"
);
static PyObject *bpy_msgbus_clear_by_owner(PyObject *UNUSED(self), PyObject *py_owner)
{
	bContext *C = (bContext *)BPy_GetContext();
	struct wmMsgBus *mbus = CTX_wm_message_bus(C);
	WM_msgbus_clear_by_owner(mbus, py_owner);
	Py_RETURN_NONE;
}

static struct PyMethodDef BPy_msgbus_methods[] = {
	{"subscribe_rna", (PyCFunction)bpy_msgbus_subscribe_rna, METH_VARARGS | METH_KEYWORDS, bpy_msgbus_subscribe_rna_doc},
	{"publish_rna", (PyCFunction)bpy_msgbus_publish_rna, METH_VARARGS | METH_KEYWORDS, bpy_msgbus_publish_rna_doc},
	{"clear_by_owner", (PyCFunction)bpy_msgbus_clear_by_owner, METH_O, bpy_msgbus_clear_by_owner_doc},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef _bpy_msgbus_def = {
	PyModuleDef_HEAD_INIT,
	.m_name = "msgbus",
	.m_methods = BPy_msgbus_methods,
};


PyObject *BPY_msgbus_module(void)
{
	PyObject *submodule;

	submodule = PyModule_Create(&_bpy_msgbus_def);

	return submodule;
}

/** \} */
