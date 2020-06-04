/*
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
 */

/** \file
 * \ingroup pythonintern
 *
 * This file currently exposes callbacks for interface regions but may be
 * extended later.
 */

#include <Python.h>

#include "RNA_types.h"

#include "BLI_utildefines.h"

#include "bpy_capi_utils.h"
#include "bpy_rna.h"
#include "bpy_rna_callback.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.h"

#include "ED_space_api.h"

#include "../generic/python_utildefines.h"

/* Use this to stop other capsules from being mis-used. */
static const char *rna_capsual_id = "RNA_HANDLE";
static const char *rna_capsual_id_invalid = "RNA_HANDLE_REMOVED";

static const EnumPropertyItem region_draw_mode_items[] = {
    {REGION_DRAW_POST_PIXEL, "POST_PIXEL", 0, "Post Pixel", ""},
    {REGION_DRAW_POST_VIEW, "POST_VIEW", 0, "Post View", ""},
    {REGION_DRAW_PRE_VIEW, "PRE_VIEW", 0, "Pre View", ""},
    {REGION_DRAW_BACKDROP, "BACKDROP", 0, "Backdrop", ""},
    {0, NULL, 0, NULL, NULL},
};

static void cb_region_draw(const bContext *C, ARegion *UNUSED(region), void *customdata)
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

/* We could make generic utility */
static PyObject *PyC_Tuple_CopySized(PyObject *src, int len_dst)
{
  PyObject *dst = PyTuple_New(len_dst);
  int len_src = PyTuple_GET_SIZE(src);
  BLI_assert(len_src <= len_dst);
  for (int i = 0; i < len_src; i++) {
    PyObject *item = PyTuple_GET_ITEM(src, i);
    PyTuple_SET_ITEM(dst, i, item);
    Py_INCREF(item);
  }
  return dst;
}

static void cb_wm_cursor_draw(bContext *C, int x, int y, void *customdata)
{
  PyObject *cb_func, *cb_args, *result;
  PyGILState_STATE gilstate;

  bpy_context_set((bContext *)C, &gilstate);

  cb_func = PyTuple_GET_ITEM((PyObject *)customdata, 1);
  cb_args = PyTuple_GET_ITEM((PyObject *)customdata, 2);

  const int cb_args_len = PyTuple_GET_SIZE(cb_args);

  PyObject *cb_args_xy = PyTuple_New(2);
  PyTuple_SET_ITEMS(cb_args_xy, PyLong_FromLong(x), PyLong_FromLong(y));

  PyObject *cb_args_with_xy = PyC_Tuple_CopySized(cb_args, cb_args_len + 1);
  PyTuple_SET_ITEM(cb_args_with_xy, cb_args_len, cb_args_xy);

  result = PyObject_CallObject(cb_func, cb_args_with_xy);

  Py_DECREF(cb_args_with_xy);

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

  if (!PyArg_ParseTuple(args,
                        "OO!|s:bpy_struct.callback_add",
                        &cb_func,
                        &PyTuple_Type,
                        &cb_args,
                        &cb_event_str)) {
    return NULL;
  }

  if (!PyCallable_Check(cb_func)) {
    PyErr_SetString(PyExc_TypeError, "callback_add(): first argument isn't callable");
    return NULL;
  }

  if (RNA_struct_is_a(self->ptr.type, &RNA_Region)) {
    if (cb_event_str) {
      if (pyrna_enum_value_from_id(
              region_draw_mode_items, cb_event_str, &cb_event, "bpy_struct.callback_add()") ==
          -1) {
        return NULL;
      }
    }
    else {
      cb_event = REGION_DRAW_POST_PIXEL;
    }

    handle = ED_region_draw_cb_activate(
        ((ARegion *)self->ptr.data)->type, cb_region_draw, (void *)args, cb_event);
    Py_INCREF(args);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "callback_add(): type does not support callbacks");
    return NULL;
  }

  return PyCapsule_New((void *)handle, rna_capsual_id, NULL);
}

PyObject *pyrna_callback_remove(BPy_StructRNA *self, PyObject *args)
{
  PyObject *py_handle;
  void *handle;
  void *customdata;

  if (!PyArg_ParseTuple(args, "O!:callback_remove", &PyCapsule_Type, &py_handle)) {
    return NULL;
  }

  handle = PyCapsule_GetPointer(py_handle, rna_capsual_id);

  if (handle == NULL) {
    PyErr_SetString(PyExc_ValueError,
                    "callback_remove(handle): NULL handle given, invalid or already removed");
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
  PyCapsule_SetName(py_handle, rna_capsual_id_invalid);

  Py_RETURN_NONE;
}
#endif

/* reverse of rna_Space_refine() */
static eSpace_Type rna_Space_refine_reverse(StructRNA *srna)
{
  if (srna == &RNA_SpaceView3D) {
    return SPACE_VIEW3D;
  }
  if (srna == &RNA_SpaceGraphEditor) {
    return SPACE_GRAPH;
  }
  if (srna == &RNA_SpaceOutliner) {
    return SPACE_OUTLINER;
  }
  if (srna == &RNA_SpaceProperties) {
    return SPACE_PROPERTIES;
  }
  if (srna == &RNA_SpaceFileBrowser) {
    return SPACE_FILE;
  }
  if (srna == &RNA_SpaceImageEditor) {
    return SPACE_IMAGE;
  }
  if (srna == &RNA_SpaceInfo) {
    return SPACE_INFO;
  }
  if (srna == &RNA_SpaceSequenceEditor) {
    return SPACE_SEQ;
  }
  if (srna == &RNA_SpaceTextEditor) {
    return SPACE_TEXT;
  }
  if (srna == &RNA_SpaceDopeSheetEditor) {
    return SPACE_ACTION;
  }
  if (srna == &RNA_SpaceNLA) {
    return SPACE_NLA;
  }
  if (srna == &RNA_SpaceNodeEditor) {
    return SPACE_NODE;
  }
  if (srna == &RNA_SpaceConsole) {
    return SPACE_CONSOLE;
  }
  if (srna == &RNA_SpacePreferences) {
    return SPACE_USERPREF;
  }
  if (srna == &RNA_SpaceClipEditor) {
    return SPACE_CLIP;
  }
  return SPACE_EMPTY;
}

PyObject *pyrna_callback_classmethod_add(PyObject *UNUSED(self), PyObject *args)
{
  void *handle;
  PyObject *cls;
  PyObject *cb_func, *cb_args;
  StructRNA *srna;

  if (PyTuple_GET_SIZE(args) < 2) {
    PyErr_SetString(PyExc_ValueError, "handler_add(handler): expected at least 2 args");
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

  if (srna == &RNA_WindowManager) {
    const char *error_prefix = "WindowManager.draw_cursor_add";
    struct {
      const char *space_type_str;
      const char *region_type_str;

      int space_type;
      int region_type;
    } params = {
        .space_type_str = NULL,
        .region_type_str = NULL,
        .space_type = SPACE_TYPE_ANY,
        .region_type = RGN_TYPE_ANY,
    };

    if (!PyArg_ParseTuple(args,
                          "OOO!|ss:WindowManager.draw_cursor_add",
                          &cls,
                          &cb_func, /* already assigned, no matter */
                          &PyTuple_Type,
                          &cb_args,
                          &params.space_type_str,
                          &params.region_type_str)) {
      return NULL;
    }

    if (params.space_type_str && pyrna_enum_value_from_id(rna_enum_space_type_items,
                                                          params.space_type_str,
                                                          &params.space_type,
                                                          error_prefix) == -1) {
      return NULL;
    }
    else if (params.region_type_str && pyrna_enum_value_from_id(rna_enum_region_type_items,
                                                                params.region_type_str,
                                                                &params.region_type,
                                                                error_prefix) == -1) {
      return NULL;
    }

    handle = WM_paint_cursor_activate(
        params.space_type, params.region_type, NULL, cb_wm_cursor_draw, (void *)args);
  }
  else if (RNA_struct_is_a(srna, &RNA_Space)) {
    const char *error_prefix = "Space.draw_handler_add";
    struct {
      const char *region_type_str;
      const char *event_str;

      int region_type;
      int event;
    } params;

    if (!PyArg_ParseTuple(args,
                          "OOO!ss:Space.draw_handler_add",
                          &cls,
                          &cb_func, /* already assigned, no matter */
                          &PyTuple_Type,
                          &cb_args,
                          &params.region_type_str,
                          &params.event_str)) {
      return NULL;
    }

    if (pyrna_enum_value_from_id(
            region_draw_mode_items, params.event_str, &params.event, error_prefix) == -1) {
      return NULL;
    }
    else if (pyrna_enum_value_from_id(rna_enum_region_type_items,
                                      params.region_type_str,
                                      &params.region_type,
                                      error_prefix) == -1) {
      return NULL;
    }
    else {
      const eSpace_Type spaceid = rna_Space_refine_reverse(srna);
      if (spaceid == SPACE_EMPTY) {
        PyErr_Format(PyExc_TypeError, "unknown space type '%.200s'", RNA_struct_identifier(srna));
        return NULL;
      }
      else {
        SpaceType *st = BKE_spacetype_from_id(spaceid);
        ARegionType *art = BKE_regiontype_from_id(st, params.region_type);
        if (art == NULL) {
          PyErr_Format(
              PyExc_TypeError, "region type '%.200s' not in space", params.region_type_str);
          return NULL;
        }
        handle = ED_region_draw_cb_activate(art, cb_region_draw, (void *)args, params.event);
      }
    }
  }
  else {
    PyErr_SetString(PyExc_TypeError, "callback_add(): type does not support callbacks");
    return NULL;
  }

  PyObject *ret = PyCapsule_New((void *)handle, rna_capsual_id, NULL);

  /* Store 'args' in context as well as the handler custom-data,
   * because the handle may be freed by Blender (new file, new window... etc) */
  PyCapsule_SetContext(ret, args);
  Py_INCREF(args);

  return ret;
}

PyObject *pyrna_callback_classmethod_remove(PyObject *UNUSED(self), PyObject *args)
{
  PyObject *cls;
  PyObject *py_handle;
  void *handle;
  StructRNA *srna;
  bool capsule_clear = false;

  if (PyTuple_GET_SIZE(args) < 2) {
    PyErr_SetString(PyExc_ValueError, "callback_remove(handler): expected at least 2 args");
    return NULL;
  }

  cls = PyTuple_GET_ITEM(args, 0);
  if (!(srna = pyrna_struct_as_srna(cls, false, "callback_remove"))) {
    return NULL;
  }
  py_handle = PyTuple_GET_ITEM(args, 1);
  handle = PyCapsule_GetPointer(py_handle, rna_capsual_id);
  if (handle == NULL) {
    PyErr_SetString(PyExc_ValueError,
                    "callback_remove(handler): NULL handler given, invalid or already removed");
    return NULL;
  }
  PyObject *handle_args = PyCapsule_GetContext(py_handle);

  if (srna == &RNA_WindowManager) {
    if (!PyArg_ParseTuple(
            args, "OO!:WindowManager.draw_cursor_remove", &cls, &PyCapsule_Type, &py_handle)) {
      return NULL;
    }
    WM_paint_cursor_end(handle);
    capsule_clear = true;
  }
  else if (RNA_struct_is_a(srna, &RNA_Space)) {
    const char *error_prefix = "Space.draw_handler_remove";
    struct {
      const char *region_type_str;

      int region_type;
    } params;

    if (!PyArg_ParseTuple(args,
                          "OO!s:Space.draw_handler_remove",
                          &cls,
                          &PyCapsule_Type,
                          &py_handle, /* already assigned, no matter */
                          &params.region_type_str)) {
      return NULL;
    }

    if (pyrna_enum_value_from_id(rna_enum_region_type_items,
                                 params.region_type_str,
                                 &params.region_type,
                                 error_prefix) == -1) {
      return NULL;
    }
    else {
      const eSpace_Type spaceid = rna_Space_refine_reverse(srna);
      if (spaceid == SPACE_EMPTY) {
        PyErr_Format(PyExc_TypeError, "unknown space type '%.200s'", RNA_struct_identifier(srna));
        return NULL;
      }
      else {
        SpaceType *st = BKE_spacetype_from_id(spaceid);
        ARegionType *art = BKE_regiontype_from_id(st, params.region_type);
        if (art == NULL) {
          PyErr_Format(
              PyExc_TypeError, "region type '%.200s' not in space", params.region_type_str);
          return NULL;
        }
        ED_region_draw_cb_exit(art, handle);
        capsule_clear = true;
      }
    }
  }
  else {
    PyErr_SetString(PyExc_TypeError, "callback_remove(): type does not support callbacks");
    return NULL;
  }

  /* don't allow reuse */
  if (capsule_clear) {
    Py_DECREF(handle_args);
    PyCapsule_SetName(py_handle, rna_capsual_id_invalid);
  }

  Py_RETURN_NONE;
}
