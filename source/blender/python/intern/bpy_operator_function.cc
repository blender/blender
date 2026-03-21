/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file implements the #BPyOpFunction type,
 * a Python C-API implementation of a callable Blender operator.
 */

#include <Python.h>
#include <optional>

#include "DNA_windowmanager_types.h"
#include "RNA_types.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "../generic/py_capi_rna.hh"
#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "BPY_extern.hh"
#include "bpy_capi_utils.hh"
#include "bpy_operator_function.hh"
#include "bpy_rna.hh" /* for setting argument properties & type method `get_rna_type`. */

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "MEM_guardedalloc.h"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DNA_scene_types.h"

#include "DEG_depsgraph.hh"

#include "CLG_log.h"

namespace blender {

/**
 * A callable operator.
 *
 * Exposed by `bpy.ops.{module}.{operator}()` to allow Blender operators to be called from Python.
 */
struct BPyOpFunction {
  PyObject_HEAD
  /** Operator ID name (e.g., `OBJECT_OT_select_all`). */
  char idname[OP_MAX_TYPENAME];
};

/* so operators called can spawn threads which acquire the GIL */
#define BPY_RELEASE_GIL

/**
 * Convert a Blender-format operator name to Python-format, returning the buffer for inline use.
 */
static const char *opname_as_py(const char *opname, char opname_buf[OP_MAX_TYPENAME])
{
  WM_operator_py_idname(opname_buf, opname);
  return opname_buf;
}

/* -------------------------------------------------------------------- */
/** \name Private Utility Functions
 * \{ */

/**
 * Update view layer dependencies.
 * If there is no active view layer update all view layers.
 */
static void bpy_op_view_layer_update(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* None in background mode. */
  if (view_layer) {
    /* Update the active view layer. */
    Scene *scene = CTX_data_scene(C);
    Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
    if (depsgraph && !DEG_is_evaluating(depsgraph)) {
      DEG_make_active(depsgraph);
      BKE_scene_graph_update_tagged(depsgraph, bmain);
    }
  }
  else {
    /* No active view layer: update all view layers in all scenes. */
    for (Scene &scene_iter : bmain->scenes) {
      for (ViewLayer &vl : scene_iter.view_layers) {
        Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, &scene_iter, &vl);
        if (depsgraph && !DEG_is_evaluating(depsgraph)) {
          DEG_make_active(depsgraph);
          BKE_scene_graph_update_tagged(depsgraph, bmain);
        }
      }
    }
  }
}

static bool bpy_op_parse_args(PyObject *args, const char **r_context_str, bool *r_is_undo)
{
  const char *C_exec = "EXEC_DEFAULT";
  bool C_undo = false;
  bool is_exec = false;
  bool is_undo_set = false;

  Py_ssize_t args_len = PyTuple_GET_SIZE(args);
  for (Py_ssize_t i = 0; i < args_len; i++) {
    PyObject *arg = PyTuple_GET_ITEM(args, i);

    if (!is_exec && PyUnicode_Check(arg)) {
      if (is_undo_set) {
        PyErr_SetString(PyExc_ValueError, "string arg must come before the boolean");
        return false;
      }
      C_exec = PyUnicode_AsUTF8(arg);
      is_exec = true;
    }
    else if ((r_is_undo != nullptr) && (!is_undo_set && (PyBool_Check(arg) || PyLong_Check(arg))))
    {
      C_undo = PyObject_IsTrue(arg);
      is_undo_set = true;
    }
    else {
      PyErr_SetString(PyExc_ValueError, "1-2 args execution context is supported");
      return false;
    }
  }

  *r_context_str = C_exec;
  if (r_is_undo) {
    *r_is_undo = C_undo;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Function: Poll
 * \{ */

static PyObject *bpy_op_fn_poll_impl(const char *opname, const char *context_str)
{
  char opname_buf[OP_MAX_TYPENAME];
  wm::OpCallContext context = wm::OpCallContext::ExecDefault;

  /* XXX TODO: work out a better solution for passing on context,
   * could make a tuple from self and pack the name and Context into it. */
  bContext *C = BPY_context_get();

  if (C == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "Context is None, cannot poll any operators");
    return nullptr;
  }

  wmOperatorType *ot = WM_operatortype_find(opname, true);

  if (ot == nullptr) {
    PyErr_Format(PyExc_AttributeError,
                 "Polling operator \"bpy.ops.%s\" error, "
                 "could not be found",
                 opname_as_py(opname, opname_buf));
    return nullptr;
  }

  if (context_str) {
    int context_int = int(context);

    if (RNA_enum_value_from_id(rna_enum_operator_context_items, context_str, &context_int) == 0) {
      char *enum_str = pyrna_enum_repr(rna_enum_operator_context_items);
      PyErr_Format(PyExc_TypeError,
                   "Calling operator \"bpy.ops.%s.poll\" error, "
                   "expected a string enum in (%s)",
                   opname_as_py(opname, opname_buf),
                   enum_str);
      MEM_delete(enum_str);
      return nullptr;
    }
    /* Copy back to the properly typed enum. */
    context = wm::OpCallContext(context_int);
  }

  /* main purpose of this function */
  PyObject *ret = WM_operator_poll_context(C, ot, context) ? Py_True : Py_False;

  return Py_NewRef(ret);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_op_fn_poll_doc,
    ".. method:: poll(context='EXEC_DEFAULT')\n"
    "\n"
    "Test if the operator can be executed in the current context.\n"
    "\n"
    ":param context: Execution context (optional)\n"
    ":type context: str\n"
    ":return: True if the operator can be executed\n"
    ":rtype: bool\n");
static PyObject *bpy_op_fn_poll(BPyOpFunction *self, PyObject *args)
{
  const char *context_str;
  if (!bpy_op_parse_args(args, &context_str, nullptr)) {
    return nullptr;
  }

  return bpy_op_fn_poll_impl(self->idname, context_str);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Function: Call
 * \{ */

static std::optional<wmOperatorStatus> bpy_op_fn_call_impl(const char *opname,
                                                           PyObject *kw,
                                                           const char *context_str,
                                                           bool is_undo)
{
  char opname_buf[OP_MAX_TYPENAME];
  wmOperatorType *ot;
  int error_val = 0;
  wmOperatorStatus retval = OPERATOR_CANCELLED;

  wm::OpCallContext context = wm::OpCallContext::ExecDefault;

  /* XXX TODO: work out a better solution for passing on context,
   * could make a tuple from self and pack the name and Context into it. */
  bContext *C = BPY_context_get();

  if (C == nullptr) {
    PyErr_SetString(PyExc_RuntimeError, "Context is None, cannot poll any operators");
    return std::nullopt;
  }

  ot = WM_operatortype_find(opname, true);

  if (ot == nullptr) {
    PyErr_Format(PyExc_AttributeError,
                 "Calling operator \"bpy.ops.%s\" error, "
                 "could not be found",
                 opname_as_py(opname, opname_buf));
    return std::nullopt;
  }

  if (!pyrna_write_check()) {
    PyErr_Format(PyExc_RuntimeError,
                 "Calling operator \"bpy.ops.%s\" error, "
                 "cannot modify blend data in this state (drawing/rendering)",
                 opname_as_py(opname, opname_buf));
    return std::nullopt;
  }

  if (context_str) {
    int context_int = int(context);

    if (RNA_enum_value_from_id(rna_enum_operator_context_items, context_str, &context_int) == 0) {
      char *enum_str = pyrna_enum_repr(rna_enum_operator_context_items);
      PyErr_Format(PyExc_TypeError,
                   "Calling operator \"bpy.ops.%s\" error, "
                   "expected a string enum in (%s)",
                   opname_as_py(opname, opname_buf),
                   enum_str);
      MEM_delete(enum_str);
      return std::nullopt;
    }
    /* Copy back to the properly typed enum. */
    context = wm::OpCallContext(context_int);
  }

  if (WM_operator_poll_context(C, ot, context) == false) {
    bool msg_free = false;
    const char *msg = CTX_wm_operator_poll_msg_get(C, &msg_free);
    PyErr_Format(PyExc_RuntimeError,
                 "Operator bpy.ops.%.200s.poll() %.200s",
                 opname_as_py(opname, opname_buf),
                 msg ? msg : "failed, context is incorrect");
    CTX_wm_operator_poll_msg_clear(C);
    if (msg_free) {
      MEM_delete(msg);
    }
    error_val = -1;
  }
  else {
    PointerRNA ptr = WM_operator_properties_create_ptr(ot);
    WM_operator_properties_sanitize(&ptr, false);

    if (kw && PyDict_Size(kw)) {
      error_val = pyrna_pydict_to_props(
          &ptr, kw, false, "Converting py args to operator properties:");
    }

    if (error_val == 0) {
      ReportList *reports;

      reports = MEM_new<ReportList>("wmOperatorReportList");

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

        retval = WM_operator_call_py(C, ot, context, &ptr, reports, is_undo);

#ifdef BPY_RELEASE_GIL
        /* regain GIL */
        PyEval_RestoreThread(ts);
      }
#endif

      error_val = BPy_reports_to_error(reports, PyExc_RuntimeError, false);

      /* operator output is nice to have in the terminal/console too */
      if (!BLI_listbase_is_empty(&reports->list)) {
        /* Restore the print level as this is owned by the operator now. */
        eReportType level = eReportType(reports->printlevel);
        BKE_report_print_level_set(reports, CLG_quiet_get() ? RPT_WARNING : RPT_DEBUG);
        BPy_reports_write_stdout(reports, nullptr);
        BKE_report_print_level_set(reports, level);
      }

      BKE_reports_clear(reports);
      if ((reports->flag & RPT_FREE) == 0) {
        BKE_reports_free(reports);
        MEM_delete(reports);
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
        return std::nullopt;
      }

      WM_operator_name_call(C, opname, wm::OpCallContext::ExecDefault, nullptr, nullptr);
    }
#endif
  }

  if (error_val == -1) {
    return std::nullopt;
  }

  /* When calling `bpy.ops.wm.read_factory_settings()` `bpy.data's` main pointer
   * is freed by clear_globals(), further access will crash blender.
   * Setting context is not needed in this case, only calling because this
   * function corrects bpy.data (internal Main pointer) */
  BPY_modules_update();

  return retval;
}

static PyObject *bpy_op_fn_call(BPyOpFunction *self, PyObject *args, PyObject *kwargs)
{
  bContext *C = BPY_context_get();
  if (UNLIKELY(C == nullptr)) {
    PyErr_SetString(PyExc_RuntimeError, "Context is None, cannot call an operator");
    return nullptr;
  }

  /* Validate keyword argument keys are strings, since Python's `tp_call`
   * does not enforce this (unlike regular Python function calls). */
  if (kwargs && !PyC_Dict_CheckKeysAreStrings(kwargs)) {
    PyErr_SetString(PyExc_TypeError, "keywords must be strings");
    return nullptr;
  }

  /* Store the window manager before operator execution to check if it changes. */
  wmWindowManager *wm = CTX_wm_manager(C);

  /* Parse the user's positional args (context string, undo boolean)
   * before forwarding to `bpy_op_fn_call_impl` which expects strict types. */
  const char *context_str;
  bool is_undo;
  if (!bpy_op_parse_args(args, &context_str, &is_undo)) {
    return nullptr;
  }

  /* Pre-call view-layer update.
   *
   * Run to account for any RNA values the user changes.
   * NOTE: We only update active view-layer, since that's what
   * operators are supposed to operate on. There might be some
   * corner cases when operator need a full scene update though. */
  bpy_op_view_layer_update(C);

  std::optional<wmOperatorStatus> result = bpy_op_fn_call_impl(
      self->idname, kwargs, context_str, is_undo);

  if (!result) {
    return nullptr;
  }

  /* Post-call: if operator finished and window manager unchanged, update view-layer again. */
  if ((*result & OPERATOR_FINISHED) && CTX_wm_manager(C) == wm) {
    bpy_op_view_layer_update(C);
  }

  /* Return `retval` flag as a set. */
  return pyrna_enum_bitfield_as_set(rna_enum_operator_return_items, int(*result));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Function: As-String
 * \{ */

static PyObject *bpy_op_fn_as_string_impl(const char *opname,
                                          PyObject *kw,
                                          bool all_args,
                                          bool macro_args)
{
  wmOperatorType *ot;
  int error_val = 0;

  bContext *C = BPY_context_get();

  if (C == nullptr) {
    PyErr_SetString(PyExc_RuntimeError,
                    "Context is None, cannot get the string representation of this object.");
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

  // ptr = WM_operator_properties_create(opname);
  /* Save another lookup */
  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ot->srna, nullptr);

  if (kw && PyDict_Size(kw)) {
    error_val = pyrna_pydict_to_props(
        &ptr, kw, false, "Converting py args to operator properties:");
  }

  std::string op_string;
  if (error_val == 0) {
    op_string = WM_operator_pystring_ex(C, nullptr, all_args, macro_args, ot, &ptr);
  }

  WM_operator_properties_free(&ptr);

  if (error_val == -1) {
    return nullptr;
  }

  return PyC_UnicodeFromStdStr(op_string);
}

static PyObject *bpy_op_fn_as_string(BPyOpFunction *self)
{
  return bpy_op_fn_as_string_impl(self->idname, nullptr, true, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Function: Get RNA Type
 * \{ */

static PyObject *bpy_op_fn_get_rna_type_impl(const char *opname)
{
  wmOperatorType *ot = WM_operatortype_find(opname, true);
  if (ot == nullptr) {
    PyErr_Format(PyExc_KeyError, "get_rna_type(\"%s\") not found", opname);
    return nullptr;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(nullptr, RNA_Struct, ot->srna);
  BPy_StructRNA *pyrna = reinterpret_cast<BPy_StructRNA *>(pyrna_struct_CreatePyObject(&ptr));
  return reinterpret_cast<PyObject *>(pyrna);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_op_fn_get_rna_type_doc,
    ".. method:: get_rna_type()\n"
    "\n"
    "Get the RNA type definition for this operator.\n"
    "\n"
    ":return: RNA type object for introspection\n"
    ":rtype: :class:`bpy.types.Struct`\n");
static PyObject *bpy_op_fn_get_rna_type(BPyOpFunction *self, PyObject * /*args*/)
{
  return bpy_op_fn_get_rna_type_impl(self->idname);
}

PyObject *pyop_getrna_type(PyObject * /*self*/, PyObject *value)
{
  const char *opname = PyUnicode_AsUTF8(value);
  if (opname == nullptr) {
    PyErr_Format(PyExc_TypeError, "get_rna_type() expects a string argument");
    return nullptr;
  }
  return bpy_op_fn_get_rna_type_impl(opname);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operator Function: Get BL-Options
 * \{ */

static PyObject *bpy_op_fn_get_bl_options_impl(const char *opname)
{
  wmOperatorType *ot = WM_operatortype_find(opname, true);
  if (ot == nullptr) {
    PyErr_Format(PyExc_KeyError, "get_bl_options(\"%s\") not found", opname);
    return nullptr;
  }
  return pyrna_enum_bitfield_as_set(rna_enum_operator_type_flag_items, ot->flag);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_op_fn_get_bl_options_doc,
    "Set of option flags for this operator (e.g. 'REGISTER', 'UNDO')");
static PyObject *bpy_op_fn_get_bl_options(BPyOpFunction *self, void * /*closure*/)
{
  return bpy_op_fn_get_bl_options_impl(self->idname);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type Functions for #BPyOpFunction
 * \{ */

static void bpy_op_fn_dealloc(BPyOpFunction *self)
{
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject *>(self));
}

/**
 * Return a user-friendly string representation of the operator.
 */
static PyObject *bpy_op_fn_str(BPyOpFunction *self)
{
  char idname_py[OP_MAX_TYPENAME];
  WM_operator_py_idname(idname_py, self->idname);

  /* Extract module and function from idname_py. */
  const char *dot_pos = strchr(idname_py, '.');
  if (!dot_pos) {
    return PyUnicode_FromFormat(
        "<function bpy.ops.%s at %p>", idname_py, static_cast<void *>(self));
  }

  size_t op_mod_str_len = dot_pos - idname_py;
  char op_mod_str[OP_MAX_TYPENAME];
  char op_fn_str[OP_MAX_TYPENAME];

  /* Copy with bounds checking. */
  if (op_mod_str_len >= sizeof(op_mod_str)) {
    /* Truncate if necessary. */
    op_mod_str_len = sizeof(op_mod_str) - 1;
  }
  memcpy(op_mod_str, idname_py, op_mod_str_len);
  op_mod_str[op_mod_str_len] = '\0';
  STRNCPY(op_fn_str, dot_pos + 1);

  return PyUnicode_FromFormat(
      "<function bpy.ops.%s.%s at %p>", op_mod_str, op_fn_str, static_cast<void *>(self));
}

/**
 * Return a string representation of the operator for debugging.
 */
static PyObject *bpy_op_fn_repr(BPyOpFunction *self)
{
  /* Use the same format as the original Python implementation */
  PyObject *result = bpy_op_fn_as_string(self);
  if (!result) {
    /* Fallback to simple string if bpy_op_fn_as_string fails. */
    PyErr_Clear();
    char idname_py[OP_MAX_TYPENAME];
    WM_operator_py_idname(idname_py, self->idname);
    return PyUnicode_FromFormat("<bpy.ops.%s function>", idname_py);
  }

  return result;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Methods for #BPyOpFunctionType
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_op_fn_idname_doc,
    ".. method:: idname()\n"
    "\n"
    ":return: Return the Blender-format operator idname (e.g., 'OBJECT_OT_select_all').\n"
    ":rtype: str\n");
static PyObject *bpy_op_fn_idname(BPyOpFunction *self, PyObject * /*args*/)
{
  return PyUnicode_FromString(self->idname);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_op_fn_idname_py_doc,
    ".. method:: idname_py()\n"
    "\n"
    ":return: Return the Python-format operator idname (e.g., 'object.select_all').\n"
    ":rtype: str\n");
static PyObject *bpy_op_fn_idname_py(BPyOpFunction *self, PyObject * /*args*/)
{
  char idname_py[OP_MAX_TYPENAME];
  WM_operator_py_idname(idname_py, self->idname);
  return PyUnicode_FromString(idname_py);
}

static PyObject *bpy_op_fn_get_doc_impl(BPyOpFunction *self)
{
  /* Get operator signature using Blender format idname:
   * `_op_as_string(self.idname())` where `idname()` returns Blender format). */
  PyObject *sig_result = bpy_op_fn_as_string(self);

  if (!sig_result) {
    /* Fallback to simple string if `bpy_op_fn_as_string` fails. */
    PyErr_Clear();
    char idname_py[OP_MAX_TYPENAME];
    WM_operator_py_idname(idname_py, self->idname);
    return PyUnicode_FromFormat("bpy.ops.%s(...)", idname_py);
  }

  /* Get RNA type and description using Blender format idname. */
  PyObject *rna_type = bpy_op_fn_get_rna_type(self, nullptr);

  if (!rna_type) {
    PyErr_Clear();
    return sig_result; /* Return just signature on failure. */
  }

  /* Get description attribute from RNA type. */
  PyObject *description = PyObject_GetAttrString(rna_type, "description");
  Py_DECREF(rna_type);

  if (!description) {
    PyErr_Clear();
    return sig_result; /* Return just signature on failure. */
  }

  /* Combine signature and description with newline. */
  PyObject *combined = PyUnicode_FromFormat("%U\n%U", sig_result, description);
  Py_DECREF(sig_result);
  Py_DECREF(description);

  if (!combined) {
    char idname_py[OP_MAX_TYPENAME];
    WM_operator_py_idname(idname_py, self->idname);
    return PyUnicode_FromFormat("bpy.ops.%s(...)", idname_py);
  }

  return combined;
}

/** Method definitions for BPyOpFunction. */
static PyMethodDef bpy_op_fn_methods[] = {
    {"poll", reinterpret_cast<PyCFunction>(bpy_op_fn_poll), METH_VARARGS, bpy_op_fn_poll_doc},
    {"get_rna_type",
     reinterpret_cast<PyCFunction>(bpy_op_fn_get_rna_type),
     METH_NOARGS,
     bpy_op_fn_get_rna_type_doc},
    {"idname", reinterpret_cast<PyCFunction>(bpy_op_fn_idname), METH_NOARGS, bpy_op_fn_idname_doc},
    {"idname_py",
     reinterpret_cast<PyCFunction>(bpy_op_fn_idname_py),
     METH_NOARGS,
     bpy_op_fn_idname_py_doc},
    {nullptr, nullptr, 0, nullptr}};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Get/Set for #BPyOpFunctionType
 * \{ */

static PyObject *bpy_op_fn_get_doc(BPyOpFunction *self, void * /*closure*/)
{
  return bpy_op_fn_get_doc_impl(self);
}

static PyGetSetDef bpy_op_fn_getsetters[] = {
    {"bl_options",
     reinterpret_cast<getter>(bpy_op_fn_get_bl_options),
     nullptr,
     bpy_op_fn_get_bl_options_doc,
     nullptr},
    /* No doc-string, as this is standard part of the Python spec. */
    {"__doc__", reinterpret_cast<getter>(bpy_op_fn_get_doc), nullptr, nullptr, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Type Declaration #BPyOpFunctionType
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_op_fn_doc,
    "BPyOpFunction(context='EXEC_DEFAULT', undo=False, **kwargs)\n"
    "\n"
    "   Execute the operator with the given parameters.\n"
    "\n"
    "   :param context: Execution context (optional)\n"
    "   :type context: str\n"
    "   :param undo: Force undo behavior (optional)\n"
    "   :type undo: bool\n"
    "   :param kwargs: Operator properties\n"
    "   :return: Set of completion status flags\n"
    "   :rtype: set[str]\n");
static PyTypeObject BPyOpFunctionType = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "BPyOpFunction",
    /*tp_basicsize*/ sizeof(BPyOpFunction),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ reinterpret_cast<destructor>(bpy_op_fn_dealloc),
    /*tp_print*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ reinterpret_cast<reprfunc>(bpy_op_fn_repr),
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ reinterpret_cast<ternaryfunc>(bpy_op_fn_call),
    /*tp_str*/ reinterpret_cast<reprfunc>(bpy_op_fn_str),
    /*tp_getattro*/ PyObject_GenericGetAttr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ bpy_op_fn_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ bpy_op_fn_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ bpy_op_fn_getsetters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ nullptr,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ nullptr,
    /*tp_free*/ nullptr,
    /*tp_is_gc*/ nullptr,
    /*tp_bases*/ nullptr,
    /*tp_mro*/ nullptr,
    /*tp_cache*/ nullptr,
    /*tp_subclasses*/ nullptr,
    /*tp_weaklist*/ nullptr,
    /*tp_del*/ nullptr,
    /*tp_version_tag*/ 0,
    /*tp_finalize*/ nullptr,
    /*tp_vectorcall*/ nullptr,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Public API
 * \{ */

int BPyOpFunction_InitTypes()
{
  if (PyType_Ready(&BPyOpFunctionType) < 0) {
    return -1;
  }
  return 0;
}

PyObject *pyop_create_function(PyObject * /*self*/, PyObject *args)
{
  const char *op_mod_str, *op_fn_str;

  if (!PyArg_ParseTuple(args, "ss", &op_mod_str, &op_fn_str)) {
    return nullptr;
  }

  /* Validate operator name lengths before constructing strings. */
  size_t bl_len = strlen(op_mod_str) + 4 + strlen(op_fn_str) + 1; /* "_OT_" + null terminator */
  if (bl_len > OP_MAX_TYPENAME) {
    PyErr_Format(PyExc_ValueError, "Operator name too long: %s.%s", op_mod_str, op_fn_str);
    return nullptr;
  }

  /* Create a new #BPyOpFunction instance for direct operator execution. */
  BPyOpFunction *op_fn = static_cast<BPyOpFunction *> PyObject_New(BPyOpFunction,
                                                                   &BPyOpFunctionType);
  if (!op_fn) {
    return nullptr;
  }

  /* Construct the Blender `idname` (e.g., `OBJECT_OT_select_all`). */
  char op_mod_str_upper[OP_MAX_TYPENAME];
  STRNCPY(op_mod_str_upper, op_mod_str);
  BLI_str_toupper_ascii(op_mod_str_upper, sizeof(op_mod_str_upper));

  const size_t idname_len = SNPRINTF(op_fn->idname, "%s_OT_%s", op_mod_str_upper, op_fn_str);
  /* Prevented by the #OP_MAX_TYPENAME check. */
  BLI_assert(idname_len < sizeof(op_fn->idname));
  UNUSED_VARS_NDEBUG(idname_len);
  return reinterpret_cast<PyObject *>(op_fn);
}

/** \} */

}  // namespace blender
