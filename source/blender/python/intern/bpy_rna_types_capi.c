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
 * This file extends RNA types from `bpy.types` with C/Python API methods and attributes.
 *
 * We should avoid adding code here, and prefer:
 * - `source/blender/makesrna/intern/rna_context.c` using the RNA C API.
 * - `release/scripts/modules/bpy_types.py` when additions c an be written in Python.
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
#include "bpy_rna_data.h"
#include "bpy_rna_id_collection.h"
#include "bpy_rna_types_capi.h"
#include "bpy_rna_ui.h"

#include "../generic/py_capi_utils.h"

#include "RNA_access.h"

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
/** \name Window Manager Clipboard Property
 *
 * Avoid using the RNA API because this value may change between checking its length
 * and creating the buffer, causing writes past the allocated length.
 * \{ */

static PyObject *pyrna_WindowManager_clipboard_get(PyObject *UNUSED(self), void *UNUSED(flag))
{
  int text_len = 0;
  char *text = WM_clipboard_text_get(false, &text_len);
  PyObject *result = PyC_UnicodeFromByteAndSize(text ? text : "", text_len);
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
  const char *text = PyC_UnicodeAsByte(value, &value_coerce);
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

static struct PyMethodDef pyrna_windowmanager_methods[] = {
    {"draw_cursor_add",
     (PyCFunction)pyrna_callback_classmethod_add,
     METH_VARARGS | METH_CLASS,
     ""},
    {"draw_cursor_remove",
     (PyCFunction)pyrna_callback_classmethod_remove,
     METH_VARARGS | METH_CLASS,
     ""},
    {NULL, NULL, 0, NULL},
};

static struct PyGetSetDef pyrna_windowmanager_getset[] = {
    {"clipboard",
     pyrna_WindowManager_clipboard_get,
     pyrna_WindowManager_clipboard_set,
     NULL,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
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
    "   :param callback:\n"
    "      A function that will be called when the region is drawn.\n"
    "      It gets the specified arguments as input.\n"
    "   :type callback: function\n"
    "   :param args: Arguments that will be passed to the callback.\n"
    "   :type args: tuple\n"
    "   :param region_type: The region type the callback draws in; usually ``WINDOW``. "
    "(:class:`bpy.types.Region.type`)\n"
    "   :type region_type: str\n"
    "   :param draw_type: Usually ``POST_PIXEL`` for 2D drawing and ``POST_VIEW`` for 3D drawing. "
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
             "   :param handler: The draw handler that should be removed.\n"
             "   :type handler: object\n"
             "   :param region_type: Region type the callback was added to.\n"
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

  /* WindowManager */
  pyrna_struct_type_extend_capi(
      &RNA_WindowManager, pyrna_windowmanager_methods, pyrna_windowmanager_getset);
}

/** \} */
