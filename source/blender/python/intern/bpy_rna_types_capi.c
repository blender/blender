/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file extends RNA types from `bpy.types` with C/Python API methods and attributes.
 *
 * We should avoid adding code here, and prefer:
 * - `source/blender/makesrna/intern/rna_context.c` using the RNA C API.
 * - `scripts/modules/bpy_types.py` when additions c an be written in Python.
 *
 * Otherwise functions can be added here as a last resort.
 */

#include <Python.h>
#include <descrobject.h>

#include "RNA_types.h"

#include "BLI_utildefines.h"

#include "bpy_library.h"
#include "bpy_rna.h"
#include "bpy_rna_callback.h"
#include "bpy_rna_context.h"
#include "bpy_rna_data.h"
#include "bpy_rna_id_collection.h"
#include "bpy_rna_text.h"
#include "bpy_rna_types_capi.h"
#include "bpy_rna_ui.h"

#include "bpy_rna_operator.h"

#include "../generic/py_capi_utils.h"

#include "RNA_access.h"
#include "RNA_prototypes.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

/* -------------------------------------------------------------------- */
/** \name Blend Data
 * \{ */

static struct PyMethodDef pyrna_blenddata_methods[] = {
    {NULL, NULL, 0, NULL}, /* #BPY_rna_id_collection_user_map_method_def */
    {NULL, NULL, 0, NULL}, /* #BPY_rna_id_collection_batch_remove_method_def */
    {NULL, NULL, 0, NULL}, /* #BPY_rna_id_collection_orphans_purge_method_def */
    {NULL, NULL, 0, NULL}, /* #BPY_rna_data_context_method_def */
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend Data Libraries
 * \{ */

static struct PyMethodDef pyrna_blenddatalibraries_methods[] = {
    {NULL, NULL, 0, NULL}, /* #BPY_library_load_method_def */
    {NULL, NULL, 0, NULL}, /* #BPY_library_write_method_def */
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Layout
 * \{ */

static struct PyMethodDef pyrna_uilayout_methods[] = {
    {NULL, NULL, 0, NULL}, /* #BPY_rna_uilayout_introspect_method_def */
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator
 * \{ */

static struct PyMethodDef pyrna_operator_methods[] = {
    {NULL, NULL, 0, NULL}, /* #BPY_rna_operator_poll_message_set */
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Editor
 * \{ */

static struct PyMethodDef pyrna_text_methods[] = {
    {NULL, NULL, 0, NULL}, /* #BPY_rna_region_as_string_method_def */
    {NULL, NULL, 0, NULL}, /* #BPY_rna_region_from_string_method_def */
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Manager Clipboard Property
 *
 * Avoid using the RNA API because this value may change between checking its length
 * and creating the buffer, causing writes past the allocated length.
 * \{ */

PyDoc_STRVAR(pyrna_WindowManager_clipboard_doc, "Clipboard text storage.\n\n:type: string");
static PyObject *pyrna_WindowManager_clipboard_get(PyObject *UNUSED(self), void *UNUSED(flag))
{
  int text_len = 0;
  /* No need for UTF8 validation as #PyC_UnicodeFromBytesAndSize handles invalid byte sequences. */
  char *text = WM_clipboard_text_get(false, false, &text_len);
  PyObject *result = PyC_UnicodeFromBytesAndSize(text ? text : "", text_len);
  if (text != NULL) {
    MEM_freeN(text);
  }
  return result;
}

static int pyrna_WindowManager_clipboard_set(PyObject *UNUSED(self),
                                             PyObject *value,
                                             void *UNUSED(flag))
{
  PyObject *value_coerce = NULL;
  const char *text = PyC_UnicodeAsBytes(value, &value_coerce);
  if (text == NULL) {
    return -1;
  }
  WM_clipboard_text_set(text, false);
  Py_XDECREF(value_coerce);
  return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Manager Type
 * \{ */

PyDoc_STRVAR(
    pyrna_draw_cursor_add_doc,
    ".. method:: draw_cursor_add(callback, args, space_type, region_type)\n"
    "\n"
    "   Add a new draw cursor handler to this space type.\n"
    "   It will be called every time the cursor for the specified region in the space "
    "type will be drawn.\n"
    "   Note: All arguments are positional only for now.\n"
    "\n"
    "   :arg callback:\n"
    "      A function that will be called when the cursor is drawn.\n"
    "      It gets the specified arguments as input with the mouse position (tuple) as last "
    "argument.\n"
    "   :type callback: function\n"
    "   :arg args: Arguments that will be passed to the callback.\n"
    "   :type args: tuple\n"
    "   :arg space_type: The space type the callback draws in; for example ``VIEW_3D``. "
    "(:class:`bpy.types.Space.type`)\n"
    "   :type space_type: str\n"
    "   :arg region_type: The region type the callback draws in; usually ``WINDOW``. "
    "(:class:`bpy.types.Region.type`)\n"
    "   :type region_type: str\n"
    "   :return: Handler that can be removed later on.\n"
    "   :rtype: object\n");

PyDoc_STRVAR(pyrna_draw_cursor_remove_doc,
             ".. method:: draw_cursor_remove(handler)\n"
             "\n"
             "   Remove a draw cursor handler that was added previously.\n"
             "\n"
             "   :arg handler: The draw cursor handler that should be removed.\n"
             "   :type handler: object\n");

static struct PyMethodDef pyrna_windowmanager_methods[] = {
    {"draw_cursor_add",
     (PyCFunction)pyrna_callback_classmethod_add,
     METH_VARARGS | METH_CLASS,
     pyrna_draw_cursor_add_doc},
    {"draw_cursor_remove",
     (PyCFunction)pyrna_callback_classmethod_remove,
     METH_VARARGS | METH_CLASS,
     pyrna_draw_cursor_remove_doc},
    {NULL, NULL, 0, NULL},
};

static struct PyGetSetDef pyrna_windowmanager_getset[] = {
    {"clipboard",
     pyrna_WindowManager_clipboard_get,
     pyrna_WindowManager_clipboard_set,
     pyrna_WindowManager_clipboard_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Type
 * \{ */

static struct PyMethodDef pyrna_context_methods[] = {
    {NULL, NULL, 0, NULL}, /* #BPY_rna_context_temp_override_method_def */
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Type
 * \{ */

PyDoc_STRVAR(
    pyrna_draw_handler_add_doc,
    ".. method:: draw_handler_add(callback, args, region_type, draw_type)\n"
    "\n"
    "   Add a new draw handler to this space type.\n"
    "   It will be called every time the specified region in the space type will be drawn.\n"
    "   Note: All arguments are positional only for now.\n"
    "\n"
    "   :arg callback:\n"
    "      A function that will be called when the region is drawn.\n"
    "      It gets the specified arguments as input.\n"
    "   :type callback: function\n"
    "   :arg args: Arguments that will be passed to the callback.\n"
    "   :type args: tuple\n"
    "   :arg region_type: The region type the callback draws in; usually ``WINDOW``. "
    "(:class:`bpy.types.Region.type`)\n"
    "   :type region_type: str\n"
    "   :arg draw_type: Usually ``POST_PIXEL`` for 2D drawing and ``POST_VIEW`` for 3D drawing. "
    "In some cases ``PRE_VIEW`` can be used. ``BACKDROP`` can be used for backdrops in the node "
    "editor.\n"
    "   :type draw_type: str\n"
    "   :return: Handler that can be removed later on.\n"
    "   :rtype: object");

PyDoc_STRVAR(pyrna_draw_handler_remove_doc,
             ".. method:: draw_handler_remove(handler, region_type)\n"
             "\n"
             "   Remove a draw handler that was added previously.\n"
             "\n"
             "   :arg handler: The draw handler that should be removed.\n"
             "   :type handler: object\n"
             "   :arg region_type: Region type the callback was added to.\n"
             "   :type region_type: str\n");

static struct PyMethodDef pyrna_space_methods[] = {
    {"draw_handler_add",
     (PyCFunction)pyrna_callback_classmethod_add,
     METH_VARARGS | METH_CLASS,
     pyrna_draw_handler_add_doc},
    {"draw_handler_remove",
     (PyCFunction)pyrna_callback_classmethod_remove,
     METH_VARARGS | METH_CLASS,
     pyrna_draw_handler_remove_doc},
    {NULL, NULL, 0, NULL},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void BPY_rna_types_extend_capi(void)
{
  /* BlendData */
  ARRAY_SET_ITEMS(pyrna_blenddata_methods,
                  BPY_rna_id_collection_user_map_method_def,
                  BPY_rna_id_collection_batch_remove_method_def,
                  BPY_rna_id_collection_orphans_purge_method_def,
                  BPY_rna_data_context_method_def);
  BLI_assert(ARRAY_SIZE(pyrna_blenddata_methods) == 5);
  pyrna_struct_type_extend_capi(&RNA_BlendData, pyrna_blenddata_methods, NULL);

  /* BlendDataLibraries */
  ARRAY_SET_ITEMS(
      pyrna_blenddatalibraries_methods, BPY_library_load_method_def, BPY_library_write_method_def);
  BLI_assert(ARRAY_SIZE(pyrna_blenddatalibraries_methods) == 3);
  pyrna_struct_type_extend_capi(&RNA_BlendDataLibraries, pyrna_blenddatalibraries_methods, NULL);

  /* uiLayout */
  ARRAY_SET_ITEMS(pyrna_uilayout_methods, BPY_rna_uilayout_introspect_method_def);
  BLI_assert(ARRAY_SIZE(pyrna_uilayout_methods) == 2);
  pyrna_struct_type_extend_capi(&RNA_UILayout, pyrna_uilayout_methods, NULL);

  /* Space */
  pyrna_struct_type_extend_capi(&RNA_Space, pyrna_space_methods, NULL);

  /* Text Editor */
  ARRAY_SET_ITEMS(pyrna_text_methods,
                  BPY_rna_region_as_string_method_def,
                  BPY_rna_region_from_string_method_def);
  BLI_assert(ARRAY_SIZE(pyrna_text_methods) == 3);
  pyrna_struct_type_extend_capi(&RNA_Text, pyrna_text_methods, NULL);

  /* wmOperator */
  ARRAY_SET_ITEMS(pyrna_operator_methods, BPY_rna_operator_poll_message_set_method_def);
  BLI_assert(ARRAY_SIZE(pyrna_operator_methods) == 2);
  pyrna_struct_type_extend_capi(&RNA_Operator, pyrna_operator_methods, NULL);

  /* WindowManager */
  pyrna_struct_type_extend_capi(
      &RNA_WindowManager, pyrna_windowmanager_methods, pyrna_windowmanager_getset);

  /* Context */
  bpy_rna_context_types_init();

  ARRAY_SET_ITEMS(pyrna_context_methods, BPY_rna_context_temp_override_method_def);
  pyrna_struct_type_extend_capi(&RNA_Context, pyrna_context_methods, NULL);
}

/** \} */
