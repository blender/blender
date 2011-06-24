/*
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
	{(char *)"render_pre", NULL},
	{(char *)"render_post", NULL},
    {(char *)"load_pre", NULL},
	{(char *)"load_post", NULL},
    {(char *)"save_pre", NULL},
	{(char *)"save_post", NULL},
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

static PyObject *py_cb_array[BLI_CB_EVT_TOT]= {0};

static PyObject *make_app_cb_info(void)
{
	PyObject *app_cb_info;
	int pos= 0;

	app_cb_info= PyStructSequence_New(&BlenderAppCbType);
	if (app_cb_info == NULL) {
		return NULL;
	}

	for(pos= 0; pos < BLI_CB_EVT_TOT; pos++) {
		if(app_cb_info_fields[pos].name == NULL) {
			Py_FatalError("invalid callback slots 1");
		}
		PyStructSequence_SET_ITEM(app_cb_info, pos, (py_cb_array[pos]= PyList_New(0)));
	}
	if(app_cb_info_fields[pos].name != NULL) {
		Py_FatalError("invalid callback slots 2");
	}

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
	if(ret) {
		static bCallbackFuncStore funcstore_array[BLI_CB_EVT_TOT]= {{0}};
		bCallbackFuncStore *funcstore;
		int pos= 0;

		for(pos= 0; pos < BLI_CB_EVT_TOT; pos++) {
			funcstore= &funcstore_array[pos];
			funcstore->func= bpy_app_generic_callback;
			funcstore->alloc= 0;
			funcstore->arg= SET_INT_IN_POINTER(pos);
			BLI_add_cb(funcstore, pos);
		}
	}

	return ret;
}

void BPY_app_handlers_reset(void)
{
	int pos= 0;

	for(pos= 0; pos < BLI_CB_EVT_TOT; pos++) {
		PyList_SetSlice(py_cb_array[pos], 0, PY_SSIZE_T_MAX, NULL);
	}
}

/* the actual callback - not necessarily called from py */
void bpy_app_generic_callback(struct Main *UNUSED(main), struct ID *id, void *arg)
{
	PyObject *cb_list= py_cb_array[GET_INT_FROM_POINTER(arg)];
	Py_ssize_t cb_list_len;
	if((cb_list_len= PyList_GET_SIZE(cb_list)) > 0) {
		PyGILState_STATE gilstate= PyGILState_Ensure();

		PyObject* args= PyTuple_New(1); // save python creating each call
		PyObject* func;
		PyObject* ret;
		Py_ssize_t pos;

		/* setup arguments */
		if(id) {
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
