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
 */

#include <Python.h>
#include "BLI_utildefines.h"
#include "BLI_callbacks.h"

#include "RNA_types.h"
#include "RNA_access.h"
#include "bpy_rna.h"
#include "bpy_app_handlers.h"

void bpy_app_generic_callback(struct Main *main, struct ID *id, void *arg);

static PyTypeObject BlenderAppCbType;

static PyStructSequence_Field app_cb_info_fields[]= {
	{(char *)"frame_change_pre", NULL},
	{(char *)"frame_change_post", NULL},
	{(char *)"render_pre", NULL},
	{(char *)"render_post", NULL},
	{(char *)"render_stats", NULL},
	{(char *)"load_pre", NULL},
	{(char *)"load_post", NULL},
	{(char *)"save_pre", NULL},
	{(char *)"save_post", NULL},
	{(char *)"scene_update_pre", NULL},
	{(char *)"scene_update_post", NULL},

	/* sets the permanent tag */
#   define APP_CB_OTHER_FIELDS 1
	{(char *)"permanent_tag", NULL},

	{NULL}
};

static PyStructSequence_Desc app_cb_info_desc= {
	(char *)"bpy.app.handlers",     /* name */
	(char *)"This module contains callbacks",    /* doc */
	app_cb_info_fields,    /* fields */
	(sizeof(app_cb_info_fields)/sizeof(PyStructSequence_Field)) - 1
};

/*
#if (BLI_CB_EVT_TOT != ((sizeof(app_cb_info_fields)/sizeof(PyStructSequence_Field))))
#  error "Callbacks are out of sync"
#endif
*/

/* --------------------------------------------------------------------------*/
/* permanent tagging code */
#define PERMINENT_CB_ID "_bpy_permanent_tag"

PyDoc_STRVAR(bpy_app_handlers_permanent_tag_doc,
".. function:: permanent_tag(func, state=True)\n"
"\n"
"   Set the function as being permanent so its not cleared when new blend files are loaded.\n"
"\n"
"   :arg func: The function  to set as permanent.\n"
"   :type func: function\n"
"   :arg state: Set the permanent state to True or False.\n"
"   :type state: bool\n"
"   :return: the function argument\n"
"   :rtype: function\n"
);

static PyObject *bpy_app_handlers_permanent_tag(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *value;
	int state= 1;

	if(!PyArg_ParseTuple(args, "O|i:permanent_tag", &value, &state))
		return NULL;

	if (PyFunction_Check(value)) {
		PyObject **dict_ptr= _PyObject_GetDictPtr(value);
		if (dict_ptr == NULL) {
			PyErr_SetString(PyExc_ValueError,
			                "bpy.app.handlers.permanent_tag wasn't able to "
			                "get the dictionary from the function passed");
			return NULL;
		}
		else {
			if (state) {
				/* set id */
				if (*dict_ptr == NULL) {
					*dict_ptr= PyDict_New();
				}

				PyDict_SetItemString(*dict_ptr, PERMINENT_CB_ID, Py_None);
			}
			else {
				/* clear id */
				if (*dict_ptr) {
					PyDict_DelItemString(*dict_ptr, PERMINENT_CB_ID);
				}
			}
		}

		Py_INCREF(value);
		return value;
	}
	else {
		PyErr_SetString(PyExc_ValueError,
		                "bpy.app.handlers.permanent_tag expected a function");
		return NULL;
	}
}

static PyMethodDef meth_bpy_app_handlers_permanent_tag= {"permanent_tag", (PyCFunction)bpy_app_handlers_permanent_tag, METH_VARARGS, bpy_app_handlers_permanent_tag_doc};




static PyObject *py_cb_array[BLI_CB_EVT_TOT]= {NULL};

static PyObject *make_app_cb_info(void)
{
	PyObject *app_cb_info;
	int pos= 0;

	app_cb_info= PyStructSequence_New(&BlenderAppCbType);
	if (app_cb_info == NULL) {
		return NULL;
	}

	for (pos= 0; pos < BLI_CB_EVT_TOT; pos++) {
		if (app_cb_info_fields[pos].name == NULL) {
			Py_FatalError("invalid callback slots 1");
		}
		PyStructSequence_SET_ITEM(app_cb_info, pos, (py_cb_array[pos]= PyList_New(0)));
	}
	if (app_cb_info_fields[pos + APP_CB_OTHER_FIELDS].name != NULL) {
		Py_FatalError("invalid callback slots 2");
	}

	/* custom function */
	PyStructSequence_SET_ITEM(app_cb_info, pos++, (PyObject *)PyCFunction_New(&meth_bpy_app_handlers_permanent_tag, NULL));

	return app_cb_info;
}

PyObject *BPY_app_handlers_struct(void)
{
	PyObject *ret;

	PyStructSequence_InitType(&BlenderAppCbType, &app_cb_info_desc);

	ret= make_app_cb_info();

	/* prevent user from creating new instances */
	BlenderAppCbType.tp_init= NULL;
	BlenderAppCbType.tp_new= NULL;

	/* assign the C callbacks */
	if (ret) {
		static bCallbackFuncStore funcstore_array[BLI_CB_EVT_TOT]= {{NULL}};
		bCallbackFuncStore *funcstore;
		int pos= 0;

		for (pos= 0; pos < BLI_CB_EVT_TOT; pos++) {
			funcstore= &funcstore_array[pos];
			funcstore->func= bpy_app_generic_callback;
			funcstore->alloc= 0;
			funcstore->arg= SET_INT_IN_POINTER(pos);
			BLI_add_cb(funcstore, pos);
		}
	}

	return ret;
}

void BPY_app_handlers_reset(const short do_all)
{
	int pos= 0;

	if (do_all) {
	for (pos= 0; pos < BLI_CB_EVT_TOT; pos++) {
			/* clear list */
			PyList_SetSlice(py_cb_array[pos], 0, PY_SSIZE_T_MAX, NULL);
		}
	}
	else {
		/* save string conversion thrashing */
		PyObject *perm_id_str= PyUnicode_FromString(PERMINENT_CB_ID);

		for (pos= 0; pos < BLI_CB_EVT_TOT; pos++) {
			/* clear only items without PERMINENT_CB_ID */
			PyObject *ls= py_cb_array[pos];
			Py_ssize_t i;

			PyObject *item;
			PyObject **dict_ptr;

			for(i= PyList_GET_SIZE(ls) - 1; i >= 0; i--) {

				if (    (PyFunction_Check((item= PyList_GET_ITEM(ls, i)))) &&
				        (dict_ptr= _PyObject_GetDictPtr(item)) &&
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
}

/* the actual callback - not necessarily called from py */
void bpy_app_generic_callback(struct Main *UNUSED(main), struct ID *id, void *arg)
{
	PyObject *cb_list= py_cb_array[GET_INT_FROM_POINTER(arg)];
	Py_ssize_t cb_list_len;
	if ((cb_list_len= PyList_GET_SIZE(cb_list)) > 0) {
		PyGILState_STATE gilstate= PyGILState_Ensure();

		PyObject* args= PyTuple_New(1); // save python creating each call
		PyObject* func;
		PyObject* ret;
		Py_ssize_t pos;

		/* setup arguments */
		if (id) {
			PointerRNA id_ptr;
			RNA_id_pointer_create(id, &id_ptr);
			PyTuple_SET_ITEM(args, 0, pyrna_struct_CreatePyObject(&id_ptr));
		}
		else {
			PyTuple_SET_ITEM(args, 0, Py_None);
			Py_INCREF(Py_None);
		}

		// Iterate the list and run the callbacks
		for (pos=0; pos < cb_list_len; pos++) {
			func= PyList_GET_ITEM(cb_list, pos);
			ret= PyObject_Call(func, args, NULL);
			if (ret==NULL) {
				PyErr_Print();
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
