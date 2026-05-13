/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file extends RNA types from `bpy.types` with C/Python API methods and attributes.
 *
 * We should avoid adding code here, and prefer:
 * - `source/blender/makesrna/intern/rna_context.cc` using the RNA C API.
 * - `scripts/modules/_bpy_types.py` when additions can be written in Python.
 *
 * Otherwise functions can be added here as a last resort.
 */

#include <Python.h>
#include <descrobject.h>

#include "BLI_utildefines.h"

#include "bpy_library.hh"
#include "bpy_rna.hh"
#include "bpy_rna_callback.hh"
#include "bpy_rna_context.hh"
#include "bpy_rna_data.hh"
#include "bpy_rna_id_collection.hh"
#include "bpy_rna_text.hh"
#include "bpy_rna_types_capi.hh"
#include "bpy_rna_ui.hh"
#include "bpy_rna_wm.hh"

#include "bpy_rna_operator.hh"

#include "RNA_prototypes.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Blend Data
 * \{ */

static PyMethodDef pyrna_blenddata_methods[] = {
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_id_collection_user_map_method_def */
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_id_collection_file_path_map_method_def */
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_id_collection_file_path_foreach_method_def */
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_id_collection_batch_remove_method_def */
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_id_collection_orphans_purge_method_def */
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_data_context_method_def */
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend Data Libraries
 * \{ */

static PyMethodDef pyrna_blenddatalibraries_methods[] = {
    {nullptr, nullptr, 0, nullptr}, /* #BPY_library_load_method_def */
    {nullptr, nullptr, 0, nullptr}, /* #BPY_library_write_method_def */
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name UI Layout
 * \{ */

static PyMethodDef pyrna_uilayout_methods[] = {
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_uilayout_introspect_method_def */
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator
 * \{ */

static PyMethodDef pyrna_operator_methods[] = {
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_operator_poll_message_set */
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Text Editor
 * \{ */

static PyMethodDef pyrna_text_methods[] = {
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_region_as_string_method_def */
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_region_from_string_method_def */
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Manager Type
 * \{ */

static PyMethodDef pyrna_windowmanager_methods[] = {
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_windowmanager_draw_cursor_add_method_def */
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_windowmanager_draw_cursor_remove_method_def */
    {nullptr, nullptr, 0, nullptr},
};

static PyGetSetDef pyrna_windowmanager_getset[] = {
    {nullptr,
     nullptr,
     nullptr,
     nullptr,
     nullptr}, /* #BPY_rna_windowmanager_clipboard_getset_def */
    {nullptr, nullptr, nullptr, nullptr, nullptr}, /* Sentinel */
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Window Type
 * \{ */

static PyMethodDef pyrna_window_methods[] = {
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_window_screenshot_method_def */
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Context Type
 * \{ */

static PyMethodDef pyrna_context_methods[] = {
    {nullptr, nullptr, 0, nullptr}, /* #BPY_rna_context_temp_override_method_def */
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Space Type
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    pyrna_draw_handler_add_doc,
    ".. classmethod:: draw_handler_add(callback, args, region_type, draw_type)\n"
    "\n"
    "   Add a new draw handler to this space type.\n"
    "   It will be called every time the specified region in the space type will be drawn.\n"
    "   Note: All arguments are positional only for now.\n"
    "\n"
    "   :param callback:\n"
    "      A function that will be called when the region is drawn.\n"
    "      It gets the specified arguments as input, it's return value is ignored.\n"
    "   :type callback: Callable[..., Any]\n"
    "   :param args: Arguments that will be passed to the callback.\n"
    "   :type args: tuple[Any, ...]\n"
    "   :param region_type: The region type the callback draws in; usually ``WINDOW``. "
    "(:class:`bpy.types.Region.type`)\n"
    "   :type region_type: str\n"
    "   :param draw_type: Usually ``POST_PIXEL`` for 2D drawing and ``POST_VIEW`` for 3D drawing. "
    "In some cases ``PRE_VIEW`` can be used. ``BACKDROP`` can be used for backdrops in the node "
    "editor.\n"
    "   :type draw_type: str\n"
    "   :return: Handler that can be removed later on.\n"
    "   :rtype: object\n");
PyDoc_STRVAR(
    /* Wrap. */
    pyrna_draw_handler_remove_doc,
    ".. classmethod:: draw_handler_remove(handler, region_type)\n"
    "\n"
    "   Remove a draw handler that was added previously.\n"
    "\n"
    "   :param handler: The draw handler that should be removed.\n"
    "   :type handler: object\n"
    "   :param region_type: Region type the callback was added to.\n"
    "   :type region_type: str\n");

static PyMethodDef pyrna_space_methods[] = {
    {"draw_handler_add",
     static_cast<PyCFunction>(pyrna_callback_classmethod_add),
     METH_VARARGS | METH_CLASS,
     pyrna_draw_handler_add_doc},
    {"draw_handler_remove",
     static_cast<PyCFunction>(pyrna_callback_classmethod_remove),
     METH_VARARGS | METH_CLASS,
     pyrna_draw_handler_remove_doc},
    {nullptr, nullptr, 0, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

void BPY_rna_types_extend_capi(PyObject *bpy_types)
{
  /* BlendData */
  ARRAY_SET_ITEMS(pyrna_blenddata_methods,
                  BPY_rna_id_collection_user_map_method_def,
                  BPY_rna_id_collection_file_path_map_method_def,
                  BPY_rna_id_collection_file_path_foreach_method_def,
                  BPY_rna_id_collection_batch_remove_method_def,
                  BPY_rna_id_collection_orphans_purge_method_def,
                  BPY_rna_data_context_method_def);
  BLI_STATIC_ASSERT(ARRAY_SIZE(pyrna_blenddata_methods) == 7, "Unexpected number of methods")
  pyrna_struct_type_extend_capi(RNA_BlendData, pyrna_blenddata_methods, nullptr);

  /* BlendDataLibraries */
  ARRAY_SET_ITEMS(
      pyrna_blenddatalibraries_methods, BPY_library_load_method_def, BPY_library_write_method_def);
  BLI_STATIC_ASSERT(ARRAY_SIZE(pyrna_blenddatalibraries_methods) == 3,
                    "Unexpected number of methods")
  pyrna_struct_type_extend_capi(RNA_BlendDataLibraries, pyrna_blenddatalibraries_methods, nullptr);

  /* ui::Layout */
  ARRAY_SET_ITEMS(pyrna_uilayout_methods, BPY_rna_uilayout_introspect_method_def);
  BLI_STATIC_ASSERT(ARRAY_SIZE(pyrna_uilayout_methods) == 2, "Unexpected number of methods")
  pyrna_struct_type_extend_capi(RNA_UILayout, pyrna_uilayout_methods, nullptr);

  /* Space */
  pyrna_struct_type_extend_capi(RNA_Space, pyrna_space_methods, nullptr);

  /* Text Editor */
  ARRAY_SET_ITEMS(pyrna_text_methods,
                  BPY_rna_region_as_string_method_def,
                  BPY_rna_region_from_string_method_def);
  BLI_STATIC_ASSERT(ARRAY_SIZE(pyrna_text_methods) == 3, "Unexpected number of methods")
  pyrna_struct_type_extend_capi(RNA_Text, pyrna_text_methods, nullptr);

  /* wmOperator */
  ARRAY_SET_ITEMS(pyrna_operator_methods, BPY_rna_operator_poll_message_set_method_def);
  BLI_STATIC_ASSERT(ARRAY_SIZE(pyrna_operator_methods) == 2, "Unexpected number of methods")
  pyrna_struct_type_extend_capi(RNA_Operator, pyrna_operator_methods, nullptr);

  /* WindowManager */
  ARRAY_SET_ITEMS(pyrna_windowmanager_methods,
                  BPY_rna_windowmanager_draw_cursor_add_method_def,
                  BPY_rna_windowmanager_draw_cursor_remove_method_def);
  BLI_STATIC_ASSERT(ARRAY_SIZE(pyrna_windowmanager_methods) == 3, "Unexpected number of methods")
  ARRAY_SET_ITEMS(pyrna_windowmanager_getset, BPY_rna_windowmanager_clipboard_getset_def);
  pyrna_struct_type_extend_capi(
      RNA_WindowManager, pyrna_windowmanager_methods, pyrna_windowmanager_getset);

  /* Window */
  ARRAY_SET_ITEMS(pyrna_window_methods, BPY_rna_window_screenshot_method_def);
  BLI_STATIC_ASSERT(ARRAY_SIZE(pyrna_window_methods) == 2, "Unexpected number of methods")
  pyrna_struct_type_extend_capi(RNA_Window, pyrna_window_methods, nullptr);

  /* Context */
  bpy_rna_context_types_init(bpy_types);

  ARRAY_SET_ITEMS(pyrna_context_methods, BPY_rna_context_temp_override_method_def);
  pyrna_struct_type_extend_capi(RNA_Context, pyrna_context_methods, nullptr);
}

/** \} */

}  // namespace blender
