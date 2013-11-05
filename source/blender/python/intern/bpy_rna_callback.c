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

/** \file blender/python/intern/bpy_rna_callback.c
 *  \ingroup pythonintern
 *
 * This file currently exposes callbacks for interface regions but may be
 * extended later.
 */


#include <Python.h>

#include "RNA_types.h"

#include "BLI_utildefines.h"

#include "bpy_rna.h"
#include "bpy_rna_callback.h"
#include "bpy_util.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "ED_space_api.h"

/* use this to stop other capsules from being mis-used */
#define RNA_CAPSULE_ID "RNA_HANDLE"
#define RNA_CAPSULE_ID_INVALID "RNA_HANDLE_REMOVED"

static EnumPropertyItem region_draw_mode_items[] = {
	{REGION_DRAW_POST_PIXEL, "POST_PIXEL", 0, "Post Pixel", ""},
	{REGION_DRAW_POST_VIEW, "POST_VIEW", 0, "Post View", ""},
	{REGION_DRAW_PRE_VIEW, "PRE_VIEW", 0, "Pre View", ""},
	{0, NULL, 0, NULL, NULL}
};

static void cb_region_draw(const bContext *C, ARegion *UNUSED(ar), void *customdata)
{
	PyObject *cb_func, *cb_args, *result;
	PyGILState_STATE gilstate;

	bpy_context_set((bContext *)C, &gilstate);

	cb_func = PyTuple_GET_ITEM((PyObject *)customdata, 1);
	cb_args = PyTuple_GET_ITEM((PyObject *)customdata, 2);
	result = PyObject_CallObject(cb_func, cb_args);

	if (result) {
		Py_DECREF(result);
	}
	else {
		PyErr_Print();
		PyErr_Clear();
	}

	bpy_context_clear((bContext *)C, &gilstate);
}

#if 0
PyObject *pyrna_callback_add(BPy_StructRNA *self, PyObject *args)
{
	void *handle;

	PyObject *cb_func, *cb_args;
	char *cb_event_str = NULL;
	int cb_event;

	if (!PyArg_ParseTuple(args, "OO!|s:bpy_struct.callback_add", &cb_func, &PyTuple_Type, &cb_args, &cb_event_str))
		return NULL;
	
	if (!PyCallable_Check(cb_func)) {
		PyErr_SetString(PyExc_TypeError, "callback_add(): first argument isn't callable");
		return NULL;
	}

	if (RNA_struct_is_a(self->ptr.type, &RNA_Region)) {
		if (cb_event_str) {
			if (pyrna_enum_value_from_id(region_draw_mode_items, cb_event_str, &cb_event, "bpy_struct.callback_add()") == -1) {
				return NULL;
			}
		}
		else {
			cb_event = REGION_DRAW_POST_PIXEL;
		}

		handle = ED_region_draw_cb_activate(((ARegion *)self->ptr.data)->type, cb_region_draw, (void *)args, cb_event);
		Py_INCREF(args);
	}
	else {
		PyErr_SetString(PyExc_TypeError, "callback_add(): type does not support callbacks");
		return NULL;
	}

	return PyCapsule_New((void *)handle, RNA_CAPSULE_ID, NULL);
}

PyObject *pyrna_callback_remove(BPy_StructRNA *self, PyObject *args)
{
	PyObject *py_handle;
	void *handle;
	void *customdata;

	if (!PyArg_ParseTuple(args, "O!:callback_remove", &PyCapsule_Type, &py_handle))
		return NULL;

	handle = PyCapsule_GetPointer(py_handle, RNA_CAPSULE_ID);

	if (handle == NULL) {
		PyErr_SetString(PyExc_ValueError, "callback_remove(handle): NULL handle given, invalid or already removed");
		return NULL;
	}

	if (RNA_struct_is_a(self->ptr.type, &RNA_Region)) {
		customdata = ED_region_draw_cb_customdata(handle);
		Py_DECREF((PyObject *)customdata);

		ED_region_draw_cb_exit(((ARegion *)self->ptr.data)->type, handle);
	}
	else {
		PyErr_SetString(PyExc_TypeError, "callback_remove(): type does not support callbacks");
		return NULL;
	}

	/* don't allow reuse */
	PyCapsule_SetName(py_handle, RNA_CAPSULE_ID_INVALID);

	Py_RETURN_NONE;
}
#endif

/* reverse of rna_Space_refine() */
static eSpace_Type rna_Space_refine_reverse(StructRNA *srna)
{
	if (srna == &RNA_SpaceView3D)           return SPACE_VIEW3D;
	if (srna == &RNA_SpaceGraphEditor)      return SPACE_IPO;
	if (srna == &RNA_SpaceOutliner)         return SPACE_OUTLINER;
	if (srna == &RNA_SpaceProperties)       return SPACE_BUTS;
	if (srna == &RNA_SpaceFileBrowser)      return SPACE_FILE;
	if (srna == &RNA_SpaceImageEditor)      return SPACE_IMAGE;
	if (srna == &RNA_SpaceInfo)             return SPACE_INFO;
	if (srna == &RNA_SpaceSequenceEditor)   return SPACE_SEQ;
	if (srna == &RNA_SpaceTextEditor)       return SPACE_TEXT;
	if (srna == &RNA_SpaceDopeSheetEditor)  return SPACE_ACTION;
	if (srna == &RNA_SpaceNLA)              return SPACE_NLA;
	if (srna == &RNA_SpaceTimeline)         return SPACE_TIME;
	if (srna == &RNA_SpaceNodeEditor)       return SPACE_NODE;
	if (srna == &RNA_SpaceLogicEditor)      return SPACE_LOGIC;
	if (srna == &RNA_SpaceConsole)          return SPACE_CONSOLE;
	if (srna == &RNA_SpaceUserPreferences)  return SPACE_USERPREF;
	if (srna == &RNA_SpaceClipEditor)       return SPACE_CLIP;
	return -1;
}

PyObject *pyrna_callback_classmethod_add(PyObject *UNUSED(self), PyObject *args)
{
	void *handle;
	PyObject *cls;
	PyObject *cb_func, *cb_args;
	char *cb_regiontype_str;
	char *cb_event_str;
	int cb_event;
	int cb_regiontype;
	StructRNA *srna;

	if (PyTuple_GET_SIZE(args) < 2) {
		PyErr_SetString(PyExc_ValueError, "handler_add(handle): expected at least 2 args");
		return NULL;
	}

	cls = PyTuple_GET_ITEM(args, 0);
	if (!(srna = pyrna_struct_as_srna(cls, false, "handler_add"))) {
		return NULL;
	}
	cb_func = PyTuple_GET_ITEM(args, 1);
	if (!PyCallable_Check(cb_func)) {
		PyErr_SetString(PyExc_TypeError, "first argument isn't callable");
		return NULL;
	}

	/* class specific callbacks */
	if (RNA_struct_is_a(srna, &RNA_Space)) {
		if (!PyArg_ParseTuple(args, "OOO!ss:Space.draw_handler_add",
		                      &cls, &cb_func,  /* already assigned, no matter */
		                      &PyTuple_Type, &cb_args, &cb_regiontype_str, &cb_event_str))
		{
			return NULL;
		}

		if (pyrna_enum_value_from_id(region_draw_mode_items, cb_event_str, &cb_event, "bpy_struct.callback_add()") == -1) {
			return NULL;
		}
		else if (pyrna_enum_value_from_id(region_type_items, cb_regiontype_str, &cb_regiontype, "bpy_struct.callback_add()") == -1) {
			return NULL;
		}
		else {
			const eSpace_Type spaceid = rna_Space_refine_reverse(srna);
			if (spaceid == -1) {
				PyErr_Format(PyExc_TypeError, "unknown space type '%.200s'", RNA_struct_identifier(srna));
				return NULL;
			}
			else {
				SpaceType *st = BKE_spacetype_from_id(spaceid);
				ARegionType *art = BKE_regiontype_from_id(st, cb_regiontype);

				handle = ED_region_draw_cb_activate(art, cb_region_draw, (void *)args, cb_event);
				Py_INCREF(args);
			}
		}
	}
	else {
		PyErr_SetString(PyExc_TypeError, "callback_add(): type does not support callbacks");
		return NULL;
	}

	return PyCapsule_New((void *)handle, RNA_CAPSULE_ID, NULL);
}

PyObject *pyrna_callback_classmethod_remove(PyObject *UNUSED(self), PyObject *args)
{
	PyObject *cls;
	PyObject *py_handle;
	void *handle;
	void *customdata;
	StructRNA *srna;
	char *cb_regiontype_str;
	int cb_regiontype;

	if (PyTuple_GET_SIZE(args) < 2) {
		PyErr_SetString(PyExc_ValueError, "callback_remove(handle): expected at least 2 args");
		return NULL;
	}

	cls = PyTuple_GET_ITEM(args, 0);
	if (!(srna = pyrna_struct_as_srna(cls, false, "callback_remove"))) {
		return NULL;
	}
	py_handle = PyTuple_GET_ITEM(args, 1);
	handle = PyCapsule_GetPointer(py_handle, RNA_CAPSULE_ID);
	if (handle == NULL) {
		PyErr_SetString(PyExc_ValueError, "callback_remove(handle): NULL handle given, invalid or already removed");
		return NULL;
	}

	if (RNA_struct_is_a(srna, &RNA_Space)) {
		if (!PyArg_ParseTuple(args, "OO!s:Space.draw_handler_remove",
		                      &cls, &PyCapsule_Type, &py_handle,  /* already assigned, no matter */
		                      &cb_regiontype_str))
		{
			return NULL;
		}

		customdata = ED_region_draw_cb_customdata(handle);
		Py_DECREF((PyObject *)customdata);

		if (pyrna_enum_value_from_id(region_type_items, cb_regiontype_str, &cb_regiontype, "bpy_struct.callback_remove()") == -1) {
			return NULL;
		}
		else {
			const eSpace_Type spaceid = rna_Space_refine_reverse(srna);
			if (spaceid == -1) {
				PyErr_Format(PyExc_TypeError, "unknown space type '%.200s'", RNA_struct_identifier(srna));
				return NULL;
			}
			else {
				SpaceType *st = BKE_spacetype_from_id(spaceid);
				ARegionType *art = BKE_regiontype_from_id(st, cb_regiontype);

				ED_region_draw_cb_exit(art, handle);
			}
		}
	}
	else {
		PyErr_SetString(PyExc_TypeError, "callback_remove(): type does not support callbacks");
		return NULL;
	}

	/* don't allow reuse */
	PyCapsule_SetName(py_handle, RNA_CAPSULE_ID_INVALID);

	Py_RETURN_NONE;
}
