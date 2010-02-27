/**
 * $Id:
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

#include "Python.h"

#include "bpy_rna.h"
#include "bpy_util.h"

#include "BLI_path_util.h"
#include "DNA_screen_types.h"
#include "BKE_context.h"
#include "ED_space_api.h"

EnumPropertyItem region_draw_mode_items[] = {
	{REGION_DRAW_POST_VIEW, "POST_VIEW", 0, "Pose View", ""},
	{REGION_DRAW_POST_PIXEL, "POST_PIXEL", 0, "Post Pixel", ""},
	{REGION_DRAW_PRE_VIEW, "PRE_VIEW", 0, "Pre View", ""},
	{0, NULL, 0, NULL, NULL}};


void cb_region_draw(const bContext *C, ARegion *ar, void *customdata)
{
	PyObject *cb_func, *cb_args, *result;
	PyGILState_STATE gilstate;

	bpy_context_set((bContext *)C, &gilstate);

	cb_func= PyTuple_GET_ITEM((PyObject *)customdata, 0);
	cb_args= PyTuple_GET_ITEM((PyObject *)customdata, 1);
	result = PyObject_CallObject(cb_func, cb_args);

	if(result) {
		Py_DECREF(result);
	}
	else {
		PyErr_Print();
		PyErr_Clear();
	}

	bpy_context_clear((bContext *)C, &gilstate);
}

PyObject *pyrna_callback_add(BPy_StructRNA *self, PyObject *args)
{
	void *handle;

	PyObject *cb_func, *cb_args;
	char *cb_event_str= NULL;
	int cb_event;

	if (!PyArg_ParseTuple(args, "OO|s:bpy_struct.callback_add", &cb_func, &cb_args, &cb_event_str))
		return NULL;

	if(RNA_struct_is_a(self->ptr.type, &RNA_Region)) {

		if(pyrna_enum_value_from_id(region_draw_mode_items, cb_event_str, &cb_event, "bpy_struct.callback_add()") < 0)
			return NULL;

		handle= ED_region_draw_cb_activate(((ARegion *)self->ptr.data)->type, cb_region_draw, (void *)args, cb_event);
		Py_INCREF(args);
	}
	else {
		PyErr_SetString(PyExc_TypeError, "callbcak_add(): type does not suppport cllbacks");
		return NULL;
	}

	return PyCapsule_New((void *)handle, NULL, NULL);
}

PyObject *pyrna_callback_remove(BPy_StructRNA *self, PyObject *args)
{
	PyObject *py_handle;
	PyObject *py_args;
	void *handle;
	void *customdata;

	if (!PyArg_ParseTuple(args, "O!:callback_remove", &PyCapsule_Type, &py_handle))
		return NULL;

	handle= PyCapsule_GetPointer(py_handle, NULL);

	if(RNA_struct_is_a(self->ptr.type, &RNA_Region)) {
		customdata= ED_region_draw_cb_customdata(handle);
		Py_DECREF((PyObject *)customdata);

		ED_region_draw_cb_exit(((ARegion *)self->ptr.data)->type, handle);
	}

	Py_RETURN_NONE;
}
