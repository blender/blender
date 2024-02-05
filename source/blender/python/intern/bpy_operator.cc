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

#include "RNA_types.hh"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "../generic/py_capi_rna.h"
#include "../generic/py_capi_utils.h"
#include "../generic/python_compat.h"
#include "../generic/python_utildefines.h"

#include "BPY_extern.h"
#include "bpy_capi_utils.h"
#include "bpy_operator.h"
#include "bpy_operator_wrap.h"
#include "bpy_rna.h" /* for setting argument properties & type method `get_rna_type`. */

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"

#include "BKE_context.hh"
#include "BKE_report.h"

/* so operators called can spawn threads which acquire the GIL */
#define BPY_RELEASE_GIL

static wmOperatorType *ot_lookup_from_py_string(PyObject *value, const char *py_fn_id)
{
  const char *opname = PyUnicode_AsUTF8(value);
  if (opname == nullptr) {
    PyErr_Format(PyExc_TypeError, "%s() expects a string argument", py_fn_id);
    return nullptr;
  }

  wmOperatorType *ot = WM_operatortype_find(opname, true);
  if (ot == nullptr) {
    PyErr_Format(PyExc_KeyError, "%s(\"%s\") not found", py_fn_id, opname);
    return nullptr;
  }
  return ot;
}

static PyObject *pyop_poll(PyObject * /*self*/, PyObject *args)
{
  wmOperatorType *ot;
  const char *opname;
  const char *context_str = nullptr;
  PyObject *ret;

  wmOperatorCallContext context = WM_OP_EXEC_DEFAULT;

  /* XXX TODO: work out a better solution for passing on context,
   * could make a tuple from self and pack the name and Context into it. */
  bContext *C = BPY_context_get();

  if (C == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "Context is None, can't poll any operators");
    return nullptr;
  }

  /* All arguments are positional. */
  static const char *_keywords[] = {"", "", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "s" /* `opname` */
      "|" /* Optional arguments. */
      "s" /* `context_str` */
      ":_bpy.ops.poll",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args, nullptr, &_parser, &opname, &context_str)) {
    return nullptr;
  }

  ot = WM_operatortype_find(opname, true);

  if (ot == nullptr) {
    PyErr_Format(PyExc_AttributeError,
                 "Polling operator \"bpy.ops.%s\" error, "
                 "could not be found",
                 opname);
    return nullptr;
  }

  if (context_str) {
    int context_int = context;

    if (RNA_enum_value_from_id(rna_enum_operator_context_items, context_str, &context_int) == 0) {
      char *enum_str = pyrna_enum_repr(rna_enum_operator_context_items);
      PyErr_Format(PyExc_TypeError,
                   "Calling operator \"bpy.ops.%s.poll\" error, "
                   "expected a string enum in (%s)",
                   opname,
                   enum_str);
      MEM_freeN(enum_str);
      return nullptr;
    }
    /* Copy back to the properly typed enum. */
    context = wmOperatorCallContext(context_int);
  }

  /* main purpose of this function */
  ret = WM_operator_poll_context((bContext *)C, ot, context) ? Py_True : Py_False;

  return Py_INCREF_RET(ret);
}

static PyObject *pyop_call(PyObject * /*self*/, PyObject *args)
{
  wmOperatorType *ot;
  int error_val = 0;
  PointerRNA ptr;
  int operator_ret = OPERATOR_CANCELLED;

  const char *opname;
  const char *context_str = nullptr;
  PyObject *kw = nullptr; /* optional args */

  wmOperatorCallContext context = WM_OP_EXEC_DEFAULT;
  int is_undo = false;

  /* XXX TODO: work out a better solution for passing on context,
   * could make a tuple from self and pack the name and Context into it. */
  bContext *C = BPY_context_get();

  if (C == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "Context is None, can't poll any operators");
    return nullptr;
  }

  /* All arguments are positional. */
  static const char *_keywords[] = {"", "", "", "", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "s"  /* `opname` */
      "|"  /* Optional arguments. */
      "O!" /* `kw` */
      "s"  /* `context_str` */
      "i"  /* `is_undo` */
      ":_bpy.ops.call",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, nullptr, &_parser, &opname, &PyDict_Type, &kw, &context_str, &is_undo))
  {
    return nullptr;
  }

  ot = WM_operatortype_find(opname, true);

  if (ot == nullptr) {
    PyErr_Format(PyExc_AttributeError,
                 "Calling operator \"bpy.ops.%s\" error, "
                 "could not be found",
                 opname);
    return nullptr;
  }

  if (!pyrna_write_check()) {
    PyErr_Format(PyExc_RuntimeError,
                 "Calling operator \"bpy.ops.%s\" error, "
                 "can't modify blend data in this state (drawing/rendering)",
                 opname);
    return nullptr;
  }

  if (context_str) {
    int context_int = context;

    if (RNA_enum_value_from_id(rna_enum_operator_context_items, context_str, &context_int) == 0) {
      char *enum_str = pyrna_enum_repr(rna_enum_operator_context_items);
      PyErr_Format(PyExc_TypeError,
                   "Calling operator \"bpy.ops.%s\" error, "
                   "expected a string enum in (%s)",
                   opname,
                   enum_str);
      MEM_freeN(enum_str);
      return nullptr;
    }
    /* Copy back to the properly typed enum. */
    context = wmOperatorCallContext(context_int);
  }

  if (WM_operator_poll_context((bContext *)C, ot, context) == false) {
    bool msg_free = false;
    const char *msg = CTX_wm_operator_poll_msg_get(C, &msg_free);
    PyErr_Format(PyExc_RuntimeError,
                 "Operator bpy.ops.%.200s.poll() %.200s",
                 opname,
                 msg ? msg : "failed, context is incorrect");
    CTX_wm_operator_poll_msg_clear(C);
    if (msg_free) {
      MEM_freeN((void *)msg);
    }
    error_val = -1;
  }
  else {
    WM_operator_properties_create_ptr(&ptr, ot);
    WM_operator_properties_sanitize(&ptr, false);

    if (kw && PyDict_Size(kw)) {
      error_val = pyrna_pydict_to_props(
          &ptr, kw, false, "Converting py args to operator properties: ");
    }

    if (error_val == 0) {
      ReportList *reports;

      reports = static_cast<ReportList *>(MEM_mallocN(sizeof(ReportList), "wmOperatorReportList"));

      /* Own so these don't move into global reports. */
      BKE_reports_init(reports, RPT_STORE | RPT_OP_HOLD | RPT_PRINT_HANDLED_BY_OWNER);

#ifdef BPY_RELEASE_GIL
      /* release GIL, since a thread could be started from an operator
       * that updates a driver */
      /* NOTE: I have not seen any examples of code that does this
       * so it may not be officially supported but seems to work ok. */
      {
        PyThreadState *ts = PyEval_SaveThread();
#endif

        operator_ret = WM_operator_call_py(C, ot, context, &ptr, reports, is_undo);

#ifdef BPY_RELEASE_GIL
        /* regain GIL */
        PyEval_RestoreThread(ts);
      }
#endif

      error_val = BPy_reports_to_error(reports, PyExc_RuntimeError, false);

      /* operator output is nice to have in the terminal/console too */
      if (!BLI_listbase_is_empty(&reports->list)) {
        BPy_reports_write_stdout(reports, nullptr);
      }

      BKE_reports_clear(reports);
      if ((reports->flag & RPT_FREE) == 0) {
        BKE_reports_free(reports);
        MEM_freeN(reports);
      }
      else {
        /* The WM is now responsible for running the modal operator,
         * show reports in the info window. */
        reports->flag &= ~RPT_OP_HOLD;
      }
    }

    WM_operator_properties_free(&ptr);

#if 0
    /* if there is some way to know an operator takes args we should use this */
    {
      /* no props */
      if (kw != nullptr) {
        PyErr_Format(PyExc_AttributeError, "Operator \"%s\" does not take any args", opname);
        return nullptr;
      }

      WM_operator_name_call(C, opname, WM_OP_EXEC_DEFAULT, nullptr, nullptr);
    }
#endif
  }

  if (error_val == -1) {
    return nullptr;
  }

  /* When calling `bpy.ops.wm.read_factory_settings()` `bpy.data's` main pointer
   * is freed by clear_globals(), further access will crash blender.
   * Setting context is not needed in this case, only calling because this
   * function corrects bpy.data (internal Main pointer) */
  BPY_modules_update();

  /* return operator_ret as a bpy enum */
  return pyrna_enum_bitfield_as_set(rna_enum_operator_return_items, operator_ret);
}

static PyObject *pyop_as_string(PyObject * /*self*/, PyObject *args)
{
  wmOperatorType *ot;

  const char *opname;
  PyObject *kw = nullptr; /* optional args */
  bool all_args = true;
  bool macro_args = true;
  int error_val = 0;

  PyObject *pybuf;

  bContext *C = BPY_context_get();

  if (C == nullptr) {
    PyErr_SetString(PyExc_RuntimeError,
                    "Context is None, can't get the string representation of this object.");
    return nullptr;
  }

  /* All arguments are positional. */
  static const char *_keywords[] = {"", "", "", "", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "s"  /* `opname` */
      "|"  /* Optional arguments. */
      "O!" /* `kw` */
      "O&" /* `all_args` */
      "O&" /* `macro_args` */
      ":_bpy.ops.as_string",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(args,
                                        nullptr,
                                        &_parser,
                                        &opname,
                                        &PyDict_Type,
                                        &kw,
                                        PyC_ParseBool,
                                        &all_args,
                                        PyC_ParseBool,
                                        &macro_args))
  {
    return nullptr;
  }

  ot = WM_operatortype_find(opname, true);

  if (ot == nullptr) {
    PyErr_Format(PyExc_AttributeError,
                 "_bpy.ops.as_string: operator \"%.200s\" "
                 "could not be found",
                 opname);
    return nullptr;
  }

  // WM_operator_properties_create(&ptr, opname);
  /* Save another lookup */
  PointerRNA ptr = RNA_pointer_create(nullptr, ot->srna, nullptr);

  if (kw && PyDict_Size(kw)) {
    error_val = pyrna_pydict_to_props(
        &ptr, kw, false, "Converting py args to operator properties: ");
  }

  std::string op_string;
  if (error_val == 0) {
    op_string = WM_operator_pystring_ex(C, nullptr, all_args, macro_args, ot, &ptr);
  }

  WM_operator_properties_free(&ptr);

  if (error_val == -1) {
    return nullptr;
  }

  if (!op_string.empty()) {
    pybuf = PyUnicode_FromString(op_string.c_str());
  }
  else {
    pybuf = PyUnicode_FromString("");
  }

  return pybuf;
}

static PyObject *pyop_dir(PyObject * /*self*/)
{
  GHashIterator iter;
  PyObject *list;
  int i;

  WM_operatortype_iter(&iter);
  list = PyList_New(BLI_ghash_len(iter.gh));

  for (i = 0; !BLI_ghashIterator_done(&iter); BLI_ghashIterator_step(&iter), i++) {
    wmOperatorType *ot = static_cast<wmOperatorType *>(BLI_ghashIterator_getValue(&iter));
    PyList_SET_ITEM(list, i, PyUnicode_FromString(ot->idname));
  }

  return list;
}

static PyObject *pyop_getrna_type(PyObject * /*self*/, PyObject *value)
{
  wmOperatorType *ot;
  if ((ot = ot_lookup_from_py_string(value, "get_rna_type")) == nullptr) {
    return nullptr;
  }

  PointerRNA ptr = RNA_pointer_create(nullptr, &RNA_Struct, ot->srna);
  BPy_StructRNA *pyrna = (BPy_StructRNA *)pyrna_struct_CreatePyObject(&ptr);
  return (PyObject *)pyrna;
}

static PyObject *pyop_get_bl_options(PyObject * /*self*/, PyObject *value)
{
  wmOperatorType *ot;
  if ((ot = ot_lookup_from_py_string(value, "get_bl_options")) == nullptr) {
    return nullptr;
  }
  return pyrna_enum_bitfield_as_set(rna_enum_operator_type_flag_items, ot->flag);
}

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static PyMethodDef bpy_ops_methods[] = {
    {"poll", (PyCFunction)pyop_poll, METH_VARARGS, nullptr},
    {"call", (PyCFunction)pyop_call, METH_VARARGS, nullptr},
    {"as_string", (PyCFunction)pyop_as_string, METH_VARARGS, nullptr},
    {"dir", (PyCFunction)pyop_dir, METH_NOARGS, nullptr},
    {"get_rna_type", (PyCFunction)pyop_getrna_type, METH_O, nullptr},
    {"get_bl_options", (PyCFunction)pyop_get_bl_options, METH_O, nullptr},
    {"macro_define", (PyCFunction)PYOP_wrap_macro_define, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
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

  submodule = PyModule_Create(&bpy_ops_module);

  return submodule;
}
