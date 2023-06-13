/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file currently exposes callbacks for interface regions but may be
 * extended later.
 */

#include <Python.h>

#include "../generic/py_capi_rna.h"
#include "../generic/python_utildefines.h"

#include "DNA_space_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"
#include "RNA_prototypes.h"

#include "BKE_screen.h"

#include "WM_api.h"

#include "ED_space_api.h"

#include "BPY_extern.h" /* For public API. */

#include "bpy_capi_utils.h"
#include "bpy_rna.h"
#include "bpy_rna_callback.h" /* Own include. */

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
  const int len_src = PyTuple_GET_SIZE(src);
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
  if (srna == &RNA_SpaceProjectSettings) {
    return SPACE_PROJECT_SETTINGS;
  }
  if (srna == &RNA_SpaceClipEditor) {
    return SPACE_CLIP;
  }
  if (srna == &RNA_SpaceSpreadsheet) {
    return SPACE_SPREADSHEET;
  }
  return SPACE_EMPTY;
}

static void cb_rna_capsule_destructor(PyObject *capsule)
{
  PyObject *args = PyCapsule_GetContext(capsule);
  Py_DECREF(args);
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
    struct {
      struct BPy_EnumProperty_Parse space_type_enum;
      struct BPy_EnumProperty_Parse region_type_enum;
    } params = {
        .space_type_enum = {.items = rna_enum_space_type_items, .value = SPACE_TYPE_ANY},
        .region_type_enum = {.items = rna_enum_region_type_items, .value = RGN_TYPE_ANY},
    };

    if (!PyArg_ParseTuple(args,
                          "OOO!|O&O&:WindowManager.draw_cursor_add",
                          &cls,
                          &cb_func, /* already assigned, no matter */
                          &PyTuple_Type,
                          &cb_args,
                          pyrna_enum_value_parse_string,
                          &params.space_type_enum,
                          pyrna_enum_value_parse_string,
                          &params.region_type_enum))
    {
      return NULL;
    }

    handle = WM_paint_cursor_activate(params.space_type_enum.value,
                                      params.region_type_enum.value,
                                      NULL,
                                      cb_wm_cursor_draw,
                                      (void *)args);
  }
  else if (RNA_struct_is_a(srna, &RNA_Space)) {
    struct {
      struct BPy_EnumProperty_Parse region_type_enum;
      struct BPy_EnumProperty_Parse event_enum;
    } params = {
        .region_type_enum = {.items = rna_enum_region_type_items},
        .event_enum = {.items = region_draw_mode_items},
    };

    if (!PyArg_ParseTuple(args,
                          "OOO!O&O&:Space.draw_handler_add",
                          &cls,
                          &cb_func, /* already assigned, no matter */
                          &PyTuple_Type,
                          &cb_args,
                          pyrna_enum_value_parse_string,
                          &params.region_type_enum,
                          pyrna_enum_value_parse_string,
                          &params.event_enum))
    {
      return NULL;
    }

    const eSpace_Type spaceid = rna_Space_refine_reverse(srna);
    if (spaceid == SPACE_EMPTY) {
      PyErr_Format(PyExc_TypeError, "unknown space type '%.200s'", RNA_struct_identifier(srna));
      return NULL;
    }

    SpaceType *st = BKE_spacetype_from_id(spaceid);
    ARegionType *art = BKE_regiontype_from_id(st, params.region_type_enum.value);
    if (art == NULL) {
      PyErr_Format(
          PyExc_TypeError, "region type %R not in space", params.region_type_enum.value_orig);
      return NULL;
    }
    handle = ED_region_draw_cb_activate(
        art, cb_region_draw, (void *)args, params.event_enum.value);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "callback_add(): type does not support callbacks");
    return NULL;
  }

  /* Keep the 'args' reference as long as the callback exists.
   * This reference is decremented in #BPY_callback_screen_free and #BPY_callback_wm_free. */
  Py_INCREF(args);

  PyObject *ret = PyCapsule_New((void *)handle, rna_capsual_id, NULL);

  /* Store 'args' in context as well for simple access. */
  PyCapsule_SetDestructor(ret, cb_rna_capsule_destructor);
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
  bool handle_removed = false;

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

  if (srna == &RNA_WindowManager) {
    if (!PyArg_ParseTuple(
            args, "OO!:WindowManager.draw_cursor_remove", &cls, &PyCapsule_Type, &py_handle))
    {
      return NULL;
    }
    handle_removed = WM_paint_cursor_end(handle);
    capsule_clear = true;
  }
  else if (RNA_struct_is_a(srna, &RNA_Space)) {
    const char *error_prefix = "Space.draw_handler_remove";
    struct {
      struct BPy_EnumProperty_Parse region_type_enum;
    } params = {
        .region_type_enum = {.items = rna_enum_region_type_items},
    };

    if (!PyArg_ParseTuple(args,
                          "OO!O&:Space.draw_handler_remove",
                          &cls,
                          &PyCapsule_Type,
                          &py_handle, /* already assigned, no matter */
                          pyrna_enum_value_parse_string,
                          &params.region_type_enum))
    {
      return NULL;
    }

    const eSpace_Type spaceid = rna_Space_refine_reverse(srna);
    if (spaceid == SPACE_EMPTY) {
      PyErr_Format(PyExc_TypeError,
                   "%s: unknown space type '%.200s'",
                   error_prefix,
                   RNA_struct_identifier(srna));
      return NULL;
    }

    SpaceType *st = BKE_spacetype_from_id(spaceid);
    ARegionType *art = BKE_regiontype_from_id(st, params.region_type_enum.value);
    if (art == NULL) {
      PyErr_Format(PyExc_TypeError,
                   "%s: region type %R not in space",
                   error_prefix,
                   params.region_type_enum.value_orig);
      return NULL;
    }
    handle_removed = ED_region_draw_cb_exit(art, handle);
    capsule_clear = true;
  }
  else {
    PyErr_SetString(PyExc_TypeError, "callback_remove(): type does not support callbacks");
    return NULL;
  }

  /* When `handle_removed == false`: Blender has already freed the data
   * (freeing screen data when loading a new file for example).
   * This will have already decremented the user, so don't decrement twice. */
  if (handle_removed == true) {
    /* The handle has been removed, so decrement its custom-data. */
    PyObject *handle_args = PyCapsule_GetContext(py_handle);
    Py_DECREF(handle_args);
  }

  /* don't allow reuse */
  if (capsule_clear) {
    PyCapsule_Destructor destructor_fn = PyCapsule_GetDestructor(py_handle);
    if (destructor_fn) {
      destructor_fn(py_handle);
      PyCapsule_SetDestructor(py_handle, NULL);
    }
    PyCapsule_SetName(py_handle, rna_capsual_id_invalid);
  }

  Py_RETURN_NONE;
}

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

static void cb_customdata_free(void *customdata)
{
  PyObject *tuple = customdata;
  bool use_gil = true; /* !PyC_IsInterpreterActive(); */

  PyGILState_STATE gilstate;
  if (use_gil) {
    gilstate = PyGILState_Ensure();
  }

  Py_DECREF(tuple);

  if (use_gil) {
    PyGILState_Release(gilstate);
  }
}

void BPY_callback_screen_free(ARegionType *art)
{
  ED_region_draw_cb_remove_by_type(art, cb_region_draw, cb_customdata_free);
}

void BPY_callback_wm_free(wmWindowManager *wm)
{
  WM_paint_cursor_remove_by_type(wm, cb_wm_cursor_draw, cb_customdata_free);
}

/** \} */
