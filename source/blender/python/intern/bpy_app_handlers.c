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

/** \file blender/python/intern/bpy_app_handlers.c
 *  \ingroup pythonintern
 *
 * This file defines a 'PyStructSequence' accessed via 'bpy.app.handlers',
 * which exposes various lists that the script author can add callback
 * functions into (called via blenders generic BLI_cb api)
 */

#include <Python.h>
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"

#include "RNA_types.h"
#include "RNA_access.h"
#include "bpy_rna.h"
#include "bpy_app_handlers.h"

#include "../generic/python_utildefines.h"

#include "BPY_extern.h"

void bpy_app_generic_callback(struct Main *main, struct ID *id, void *arg);

static PyTypeObject BlenderAppCbType;

static PyStructSequence_Field app_cb_info_fields[] = {
	{(char *)"frame_change_pre",  (char *)"on frame change for playback and rendering (before)"},
	{(char *)"frame_change_post", (char *)"on frame change for playback and rendering (after)"},
	{(char *)"render_pre",        (char *)"on render (before)"},
	{(char *)"render_post",       (char *)"on render (after)"},
	{(char *)"render_write",      (char *)"on writing a render frame (directly after the frame is written)"},
	{(char *)"render_stats",      (char *)"on printing render statistics"},
	{(char *)"render_init",       (char *)"on initialization of a render job"},
	{(char *)"render_complete",   (char *)"on completion of render job"},
	{(char *)"render_cancel",     (char *)"on canceling a render job"},
	{(char *)"load_pre",          (char *)"on loading a new blend file (before)"},
	{(char *)"load_post",         (char *)"on loading a new blend file (after)"},
	{(char *)"save_pre",          (char *)"on saving a blend file (before)"},
	{(char *)"save_post",         (char *)"on saving a blend file (after)"},
	{(char *)"undo_pre",          (char *)"on loading an undo step (before)"},
	{(char *)"undo_post",         (char *)"on loading an undo step (after)"},
	{(char *)"redo_pre",          (char *)"on loading a redo step (before)"},
	{(char *)"redo_post",         (char *)"on loading a redo step (after)"},
	{(char *)"version_update",    (char *)"on ending the versioning code"},

	/* sets the permanent tag */
#   define APP_CB_OTHER_FIELDS 1
	{(char *)"persistent",        (char *)"Function decorator for callback functions not to be removed when loading new files"},

	{NULL}
};

static PyStructSequence_Desc app_cb_info_desc = {
	(char *)"bpy.app.handlers",     /* name */
	(char *)"This module contains callback lists",  /* doc */
	app_cb_info_fields,    /* fields */
	ARRAY_SIZE(app_cb_info_fields) - 1
};

#if 0
#  if (BLI_CB_EVT_TOT != ARRAY_SIZE(app_cb_info_fields))
#    error "Callbacks are out of sync"
#  endif
#endif

/* --------------------------------------------------------------------------*/
/* permanent tagging code */
#define PERMINENT_CB_ID "_bpy_persistent"

static PyObject *bpy_app_handlers_persistent_new(PyTypeObject *UNUSED(type), PyObject *args, PyObject *UNUSED(kwds))
{
	PyObject *value;

	if (!PyArg_ParseTuple(args, "O:bpy.app.handlers.persistent", &value))
		return NULL;

	if (PyFunction_Check(value)) {
		PyObject **dict_ptr = _PyObject_GetDictPtr(value);
		if (dict_ptr == NULL) {
			PyErr_SetString(PyExc_ValueError,
			                "bpy.app.handlers.persistent wasn't able to "
			                "get the dictionary from the function passed");
			return NULL;
		}
		else {
			/* set id */
			if (*dict_ptr == NULL) {
				*dict_ptr = PyDict_New();
			}

			PyDict_SetItemString(*dict_ptr, PERMINENT_CB_ID, Py_None);
		}

		Py_INCREF(value);
		return value;
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "bpy.app.handlers.persistent expected a function");
		return NULL;
	}
}

/* dummy type because decorators can't be PyCFunctions */
static PyTypeObject BPyPersistent_Type = {

#if defined(_MSC_VER)
	PyVarObject_HEAD_INIT(NULL, 0)
#else
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
#endif

	"persistent",                               /* tp_name */
	0,                                          /* tp_basicsize */
	0,                                          /* tp_itemsize */
	/* methods */
	0,                                          /* tp_dealloc */
	0,                                          /* tp_print */
	0,                                          /* tp_getattr */
	0,                                          /* tp_setattr */
	0,                                          /* tp_reserved */
	0,                                          /* tp_repr */
	0,                                          /* tp_as_number */
	0,                                          /* tp_as_sequence */
	0,                                          /* tp_as_mapping */
	0,                                          /* tp_hash */
	0,                                          /* tp_call */
	0,                                          /* tp_str */
	0,                                          /* tp_getattro */
	0,                                          /* tp_setattro */
	0,                                          /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC |
	Py_TPFLAGS_BASETYPE,                        /* tp_flags */
	0,                                          /* tp_doc */
	0,                                          /* tp_traverse */
	0,                                          /* tp_clear */
	0,                                          /* tp_richcompare */
	0,                                          /* tp_weaklistoffset */
	0,                                          /* tp_iter */
	0,                                          /* tp_iternext */
	0,                                          /* tp_methods */
	0,                                          /* tp_members */
	0,                                          /* tp_getset */
	0,                                          /* tp_base */
	0,                                          /* tp_dict */
	0,                                          /* tp_descr_get */
	0,                                          /* tp_descr_set */
	0,                                          /* tp_dictoffset */
	0,                                          /* tp_init */
	0,                                          /* tp_alloc */
	bpy_app_handlers_persistent_new,            /* tp_new */
	0,                                          /* tp_free */
};

static PyObject *py_cb_array[BLI_CB_EVT_TOT] = {NULL};

static PyObject *make_app_cb_info(void)
{
	PyObject *app_cb_info;
	int pos;

	app_cb_info = PyStructSequence_New(&BlenderAppCbType);
	if (app_cb_info == NULL) {
		return NULL;
	}

	for (pos = 0; pos < BLI_CB_EVT_TOT; pos++) {
		if (app_cb_info_fields[pos].name == NULL) {
			Py_FatalError("invalid callback slots 1");
		}
		PyStructSequence_SET_ITEM(app_cb_info, pos, (py_cb_array[pos] = PyList_New(0)));
	}
	if (app_cb_info_fields[pos + APP_CB_OTHER_FIELDS].name != NULL) {
		Py_FatalError("invalid callback slots 2");
	}

	/* custom function */
	PyStructSequence_SET_ITEM(app_cb_info, pos++, (PyObject *)&BPyPersistent_Type);

	return app_cb_info;
}

PyObject *BPY_app_handlers_struct(void)
{
	PyObject *ret;

#if defined(_MSC_VER)
	BPyPersistent_Type.ob_base.ob_base.ob_type = &PyType_Type;
#endif

	if (PyType_Ready(&BPyPersistent_Type) < 0) {
		BLI_assert(!"error initializing 'bpy.app.handlers.persistent'");
	}

	PyStructSequence_InitType(&BlenderAppCbType, &app_cb_info_desc);

	ret = make_app_cb_info();

	/* prevent user from creating new instances */
	BlenderAppCbType.tp_init = NULL;
	BlenderAppCbType.tp_new = NULL;
	BlenderAppCbType.tp_hash = (hashfunc)_Py_HashPointer; /* without this we can't do set(sys.modules) [#29635] */

	/* assign the C callbacks */
	if (ret) {
		static bCallbackFuncStore funcstore_array[BLI_CB_EVT_TOT] = {{NULL}};
		bCallbackFuncStore *funcstore;
		int pos = 0;

		for (pos = 0; pos < BLI_CB_EVT_TOT; pos++) {
			funcstore = &funcstore_array[pos];
			funcstore->func = bpy_app_generic_callback;
			funcstore->alloc = 0;
			funcstore->arg = SET_INT_IN_POINTER(pos);
			BLI_callback_add(funcstore, pos);
		}
	}

	return ret;
}

void BPY_app_handlers_reset(const short do_all)
{
	PyGILState_STATE gilstate;
	int pos = 0;

	gilstate = PyGILState_Ensure();

	if (do_all) {
		for (pos = 0; pos < BLI_CB_EVT_TOT; pos++) {
			/* clear list */
			PyList_SetSlice(py_cb_array[pos], 0, PY_SSIZE_T_MAX, NULL);
		}
	}
	else {
		/* save string conversion thrashing */
		PyObject *perm_id_str = PyUnicode_FromString(PERMINENT_CB_ID);

		for (pos = 0; pos < BLI_CB_EVT_TOT; pos++) {
			/* clear only items without PERMINENT_CB_ID */
			PyObject *ls = py_cb_array[pos];
			Py_ssize_t i;

			PyObject *item;
			PyObject **dict_ptr;

			for (i = PyList_GET_SIZE(ls) - 1; i >= 0; i--) {

				if ((PyFunction_Check((item = PyList_GET_ITEM(ls, i)))) &&
				    (dict_ptr = _PyObject_GetDictPtr(item)) &&
				    (*dict_ptr) &&
				    (PyDict_GetItem(*dict_ptr, perm_id_str) != NULL))
				{
					/* keep */
				}
				else {
					/* remove */
					/* PySequence_DelItem(ls, i); */ /* more obvious buw slower */
					PyList_SetSlice(ls, i, i + 1, NULL);
				}
			}
		}

		Py_DECREF(perm_id_str);
	}

	PyGILState_Release(gilstate);
}

/* the actual callback - not necessarily called from py */
void bpy_app_generic_callback(struct Main *UNUSED(main), struct ID *id, void *arg)
{
	PyObject *cb_list = py_cb_array[GET_INT_FROM_POINTER(arg)];
	if (PyList_GET_SIZE(cb_list) > 0) {
		PyGILState_STATE gilstate = PyGILState_Ensure();

		PyObject *args = PyTuple_New(1);  /* save python creating each call */
		PyObject *func;
		PyObject *ret;
		Py_ssize_t pos;

		/* setup arguments */
		if (id) {
			PointerRNA id_ptr;
			RNA_id_pointer_create(id, &id_ptr);
			PyTuple_SET_ITEM(args, 0, pyrna_struct_CreatePyObject(&id_ptr));
		}
		else {
			PyTuple_SET_ITEM(args, 0, Py_INCREF_RET(Py_None));
		}

		/* Iterate the list and run the callbacks
		 * note: don't store the list size since the scripts may remove themselves */
		for (pos = 0; pos < PyList_GET_SIZE(cb_list); pos++) {
			func = PyList_GET_ITEM(cb_list, pos);
			ret = PyObject_Call(func, args, NULL);
			if (ret == NULL) {
				/* Don't set last system variables because they might cause some
				 * dangling pointers to external render engines (when exception
				 * happens during rendering) which will break logic of render pipeline
				 * which expects to be the only user of render engine when rendering
				 * is finished.
				 */
				PyErr_PrintEx(0);
				PyErr_Clear();
			}
			else {
				Py_DECREF(ret);
			}
		}

		Py_DECREF(args);

		PyGILState_Release(gilstate);
	}
}
