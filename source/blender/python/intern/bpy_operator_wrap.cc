/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file exposes functionality for defining to define operators that C can call into.
 * The generic callback functions for python operators are defines in
 * 'rna_wm.cc', some calling into functions here to do python specific
 * functionality.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "bpy_intern_string.h"
#include "bpy_operator_wrap.h" /* own include */
#include "bpy_rna.h"

static void operator_properties_init(wmOperatorType *ot)
{
  PyTypeObject *py_class = static_cast<PyTypeObject *>(ot->rna_ext.data);
  RNA_struct_blender_type_set(ot->rna_ext.srna, ot);

  /* Only call this so pyrna_deferred_register_class gives a useful error
   * WM_operatortype_append_ptr will call RNA_def_struct_identifier later.
   *
   * Note the 'no_struct_map' function is used since the actual struct name
   * is already used by the operator.
   */
  RNA_def_struct_identifier_no_struct_map(ot->srna, ot->idname);

  if (pyrna_deferred_register_class(ot->srna, py_class) != 0) {
    PyErr_Print(); /* failed to register operator props */
    PyErr_Clear();
  }

  /* set the default property: ot->prop */
  {
    /* Picky developers will notice that 'bl_property' won't work with inheritance
     * get direct from the dict to avoid raising a load of attribute errors (yes this isn't ideal)
     * - campbell. */
    PyObject *py_class_dict = py_class->tp_dict;
    PyObject *bl_property = PyDict_GetItem(py_class_dict, bpy_intern_str_bl_property);
    if (bl_property) {
      const char *prop_id = PyUnicode_AsUTF8(bl_property);
      if (prop_id != nullptr) {
        PointerRNA ptr;
        PropertyRNA *prop;

        RNA_pointer_create(nullptr, ot->srna, nullptr, &ptr);
        prop = RNA_struct_find_property(&ptr, prop_id);
        if (prop) {
          ot->prop = prop;
        }
        else {
          PyErr_Format(
              PyExc_ValueError, "%.200s.bl_property '%.200s' not found", ot->idname, prop_id);

          /* this could be done cleaner, for now its OK */
          PyErr_Print();
          PyErr_Clear();
        }
      }
      else {
        PyErr_Format(PyExc_ValueError,
                     "%.200s.bl_property should be a string, not %.200s",
                     ot->idname,
                     Py_TYPE(bl_property)->tp_name);

        /* this could be done cleaner, for now its OK */
        PyErr_Print();
        PyErr_Clear();
      }
    }
  }
  /* end 'ot->prop' assignment */
}

void BPY_RNA_operator_wrapper(wmOperatorType *ot, void *userdata)
{
  /* take care not to overwrite anything set in
   * WM_operatortype_append_ptr before opfunc() is called */
  StructRNA *srna = ot->srna;
  *ot = *((wmOperatorType *)userdata);
  ot->srna = srna; /* restore */

  /* Use i18n context from rna_ext.srna if possible (py operators). */
  if (ot->rna_ext.srna) {
    RNA_def_struct_translation_context(ot->srna, RNA_struct_translation_context(ot->rna_ext.srna));
  }

  operator_properties_init(ot);
}

void BPY_RNA_operator_macro_wrapper(wmOperatorType *ot, void *userdata)
{
  wmOperatorType *data = (wmOperatorType *)userdata;

  /* only copy a couple of things, the rest is set by the macro registration */
  ot->name = data->name;
  ot->idname = data->idname;
  ot->description = data->description;
  ot->flag |= data->flag; /* append flags to the one set by registration */
  ot->pyop_poll = data->pyop_poll;
  ot->ui = data->ui;
  ot->rna_ext = data->rna_ext;

  /* Use i18n context from rna_ext.srna if possible (py operators). */
  if (ot->rna_ext.srna) {
    RNA_def_struct_translation_context(ot->srna, RNA_struct_translation_context(ot->rna_ext.srna));
  }

  operator_properties_init(ot);
}

PyObject *PYOP_wrap_macro_define(PyObject * /*self*/, PyObject *args)
{
  wmOperatorType *ot;
  wmOperatorTypeMacro *otmacro;
  PyObject *macro;
  PointerRNA ptr_otmacro;
  StructRNA *srna;

  const char *opname;
  const char *macroname;

  if (!PyArg_ParseTuple(args, "Os:_bpy.ops.macro_define", &macro, &opname)) {
    return nullptr;
  }

  if (WM_operatortype_find(opname, true) == nullptr) {
    PyErr_Format(PyExc_ValueError, "Macro Define: '%s' is not a valid operator id", opname);
    return nullptr;
  }

  /* identifiers */
  srna = pyrna_struct_as_srna((PyObject *)macro, false, "Macro Define:");
  if (srna == nullptr) {
    return nullptr;
  }

  macroname = RNA_struct_identifier(srna);
  ot = WM_operatortype_find(macroname, true);

  if (!ot) {
    PyErr_Format(PyExc_ValueError, "Macro Define: '%s' is not a valid macro", macroname);
    return nullptr;
  }

  otmacro = WM_operatortype_macro_define(ot, opname);

  RNA_pointer_create(nullptr, &RNA_OperatorMacro, otmacro, &ptr_otmacro);

  return pyrna_struct_CreatePyObject(&ptr_otmacro);
}
