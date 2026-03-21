/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file defines `_bpy.ops`, an internal python module which gives Python
 * the ability to inspect and call operators (defined by C or Python).
 *
 * \note
 * This C module is private, it should only be used by `scripts/modules/bpy/ops.py` which
 * exposes operators as dynamically defined modules & callable objects to access all operators.
 */

#include <Python.h>

#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "bpy_operator.hh"
#include "bpy_operator_function.hh"
#include "bpy_operator_wrap.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace blender {

/* -------------------------------------------------------------------- */
/** \name Python Module
 * \{ */

static PyObject *pyop_dir(PyObject * /*self*/)
{
  const Span<wmOperatorType *> types = WM_operatortypes_registered_get();
  PyObject *list = PyList_New(types.size());

  int i = 0;
  for (wmOperatorType *ot : types) {
    PyList_SET_ITEM(list, i, PyUnicode_FromString(ot->idname));
    i++;
  }

  return list;
}

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wcast-function-type"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wcast-function-type"
#  endif
#endif

static PyMethodDef bpy_ops_methods[] = {
    {"dir", reinterpret_cast<PyCFunction>(pyop_dir), METH_NOARGS, nullptr},
    {"get_rna_type", static_cast<PyCFunction>(pyop_getrna_type), METH_O, nullptr},
    {"create_function", static_cast<PyCFunction>(pyop_create_function), METH_VARARGS, nullptr},
    {"macro_define", static_cast<PyCFunction>(PYOP_wrap_macro_define), METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyModuleDef bpy_ops_module = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "_bpy.ops",
    /*m_doc*/ nullptr,
    /*m_size*/ -1, /* multiple "initialization" just copies the module dict. */
    /*m_methods*/ bpy_ops_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPY_operator_module()
{
  PyObject *submodule;

  if (BPyOpFunction_InitTypes() < 0) {
    return nullptr;
  }

  submodule = PyModule_Create(&bpy_ops_module);

  return submodule;
}

/** \} */

}  // namespace blender
