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

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BKE_main.hh"
#include "DNA_scene_types.h"

#include "../generic/py_capi_utils.hh"
#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

#include "bpy_capi_utils.hh"
#include "bpy_operator_function.hh"

#include "BKE_context.hh"
#include "BKE_scene.hh"

#include "WM_api.hh"

#include "DEG_depsgraph.hh"

/* -------------------------------------------------------------------- */
/** \name Private Utility Functions
 * \{ */

/**
 * Update view layer dependencies.
 * If there is no active view layer update all view layers.
 */
static void bpy_op_fn_view_layer_update(bContext *C)
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
    LISTBASE_FOREACH (Scene *, scene_iter, &bmain->scenes) {
      LISTBASE_FOREACH (ViewLayer *, vl, &scene_iter->view_layers) {
        Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene_iter, vl);
        if (depsgraph && !DEG_is_evaluating(depsgraph)) {
          DEG_make_active(depsgraph);
          BKE_scene_graph_update_tagged(depsgraph, bmain);
        }
      }
    }
  }
}

static bool bpy_op_fn_parse_args(PyObject *args, const char **r_context_str, bool *r_is_undo)
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
/** \name Type Functions for #BPyOpFunction
 * \{ */

static void bpy_op_fn_dealloc(BPyOpFunction *self)
{
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *bpy_op_fn_call(BPyOpFunction *self, PyObject *args, PyObject *kwargs)
{
  bContext *C = BPY_context_get();
  if (UNLIKELY(C == nullptr)) {
    PyErr_SetString(PyExc_RuntimeError, "Context is None, cannot call an operator");
    return nullptr;
  }

  /* Store the window manager before operator execution to check if it changes. */
  wmWindowManager *wm = CTX_wm_manager(C);

  /* Convert Blender format to Python format for the call. */
  char idname_py[OP_MAX_TYPENAME];
  WM_operator_py_idname(idname_py, self->idname);

  PyObject *opname = PyUnicode_FromString(idname_py);
  if (!opname) {
    return nullptr;
  }

  PyObject *kwobj = kwargs ? Py_NewRef(kwargs) : PyDict_New();
  if (!kwobj) {
    Py_DECREF(opname);
    return nullptr;
  }

  /* Build args tuple for `pyop_call: (opname, kw, ...extra args...)`.
   * Create the child objects first so we can handle allocation failures cleanly. */
  Py_ssize_t args_len = PyTuple_GET_SIZE(args);
  PyObject *new_args = PyTuple_New(2 + args_len);
  if (!new_args) {
    Py_DECREF(opname);
    Py_DECREF(kwobj);
    return nullptr;
  }

  /* Steal references into the tuple. */
  PyTuple_SET_ITEM(new_args, 0, opname);
  PyTuple_SET_ITEM(new_args, 1, kwobj);

  for (Py_ssize_t i = 0; i < args_len; i++) {
    PyObject *item = Py_NewRef(PyTuple_GET_ITEM(args, i));
    BLI_assert(item);
    PyTuple_SET_ITEM(new_args, i + 2, item);
  }

  /* Pre-call view-layer update.
   *
   * Run to account for any RNA values the user changes.
   * NOTE: We only update active view-layer, since that's what
   * operators are supposed to operate on. There might be some
   * corner cases when operator need a full scene update though. */
  bpy_op_fn_view_layer_update(C);

  PyObject *result = pyop_call(nullptr, new_args);
  Py_DECREF(new_args);

  /* Post-call: if operator finished and window manager unchanged, update view-layer again. */
  if (result) {
    /* Check membership 'FINISHED' in result using a single temporary PyObject. */
    PyObject *finished_str = PyUnicode_FromString("FINISHED");
    if (finished_str) {
      int has_finished = PySequence_Contains(result, finished_str);
      if (has_finished == 1) {
        if (CTX_wm_manager(C) == wm) {
          bpy_op_fn_view_layer_update(C);
        }
      }
      else if (has_finished == -1) {
        PyErr_Clear();
      }
      Py_DECREF(finished_str);
    }
    else {
      PyErr_Clear();
    }
  }
  return result;
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
    return PyUnicode_FromFormat("<function bpy.ops.%s at %p>", idname_py, (void *)self);
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
  BLI_strncpy(op_fn_str, dot_pos + 1, sizeof(op_fn_str));

  return PyUnicode_FromFormat(
      "<function bpy.ops.%s.%s at %p>", op_mod_str, op_fn_str, (void *)self);
}

/**
 * Return a string representation of the operator for debugging.
 */
static PyObject *bpy_op_fn_repr(BPyOpFunction *self)
{
  /* Use the same format as the original Python implementation */
  PyObject *args = PyTuple_New(1);
  if (!args) {
    return nullptr;
  }
  PyObject *name_obj = PyUnicode_FromString(self->idname);
  if (!name_obj) {
    Py_DECREF(args);
    return nullptr;
  }
  PyTuple_SET_ITEM(args, 0, name_obj);

  PyObject *result = pyop_as_string(nullptr, args);
  Py_DECREF(args);

  if (!result) {
    /* Fallback to simple string if pyop_as_string fails. */
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
    bpy_op_fn_poll_doc,
    ".. method:: poll(context='EXEC_DEFAULT')\n"
    "\n"
    "Test if the operator can be executed in the current context.\n"
    "\n"
    ":arg context: Execution context (optional)\n"
    ":type context: str\n"
    ":return: True if the operator can be executed\n"
    ":rtype: bool\n");
static PyObject *bpy_op_fn_poll(BPyOpFunction *self, PyObject *args)
{
  const char *context_str;
  if (!bpy_op_fn_parse_args(args, &context_str, nullptr)) {
    return nullptr;
  }

  /* Convert Blender format to Python format for the poll call. */
  char idname_py[OP_MAX_TYPENAME];
  WM_operator_py_idname(idname_py, self->idname);

  PyObject *idname_obj = PyUnicode_FromString(idname_py);
  if (!idname_obj) {
    return nullptr;
  }

  PyObject *context_obj = PyUnicode_FromString(context_str);
  if (!context_obj) {
    Py_DECREF(idname_obj);
    return nullptr;
  }

  PyObject *poll_args = PyTuple_New(2);
  if (!poll_args) {
    Py_DECREF(idname_obj);
    Py_DECREF(context_obj);
    return nullptr;
  }

  PyTuple_SET_ITEM(poll_args, 0, idname_obj);
  PyTuple_SET_ITEM(poll_args, 1, context_obj);

  PyObject *result = pyop_poll(nullptr, poll_args);
  Py_DECREF(poll_args);
  return result;
}

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
  PyObject *idname_obj = PyUnicode_FromString(self->idname);
  if (!idname_obj) {
    return nullptr;
  }

  PyObject *result = pyop_getrna_type(nullptr, idname_obj);
  Py_DECREF(idname_obj);
  return result;
}

static PyObject *bpy_op_fn_get_doc_impl(BPyOpFunction *self)
{
  /* Get operator signature using Blender format idname:
   * `_op_as_string(self.idname())` where `idname()` returns Blender format). */
  PyObject *args = PyTuple_New(1);
  if (!args) {
    return nullptr;
  }
  PyObject *name_obj = PyUnicode_FromString(self->idname);
  if (!name_obj) {
    Py_DECREF(args);
    return nullptr;
  }
  PyTuple_SET_ITEM(args, 0, name_obj);

  PyObject *sig_result = pyop_as_string(nullptr, args);
  Py_DECREF(args);

  if (!sig_result) {
    /* Fallback to simple string if pyop_as_string fails. */
    PyErr_Clear();
    char idname_py[OP_MAX_TYPENAME];
    WM_operator_py_idname(idname_py, self->idname);
    return PyUnicode_FromFormat("bpy.ops.%s(...)", idname_py);
  }

  /* Get RNA type and description using Blender format idname. */
  PyObject *idname_bl_obj = PyUnicode_FromString(self->idname);
  if (!idname_bl_obj) {
    Py_DECREF(sig_result);
    return sig_result; /* Return just signature on failure. */
  }

  PyObject *rna_type = pyop_getrna_type(nullptr, idname_bl_obj);
  Py_DECREF(idname_bl_obj);

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
    {"poll", (PyCFunction)bpy_op_fn_poll, METH_VARARGS, bpy_op_fn_poll_doc},
    {"get_rna_type", (PyCFunction)bpy_op_fn_get_rna_type, METH_NOARGS, bpy_op_fn_get_rna_type_doc},
    {"idname", (PyCFunction)bpy_op_fn_idname, METH_NOARGS, bpy_op_fn_idname_doc},
    {"idname_py", (PyCFunction)bpy_op_fn_idname_py, METH_NOARGS, bpy_op_fn_idname_py_doc},
    {nullptr, nullptr, 0, nullptr}};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Get/Set for #BPyOpFunctionType
 * \{ */

PyDoc_STRVAR(
    /* Wrap. */
    bpy_op_fn_get_bl_options_doc,
    "Set of option flags for this operator (e.g. 'REGISTER', 'UNDO')");
static PyObject *bpy_op_fn_get_bl_options(BPyOpFunction *self, void * /*closure*/)
{
  PyObject *idname_obj = PyUnicode_FromString(self->idname);
  if (!idname_obj) {
    return nullptr;
  }
  PyObject *result = pyop_get_bl_options(nullptr, idname_obj);
  Py_DECREF(idname_obj);
  return result;
}

static PyObject *bpy_op_fn_get_doc(BPyOpFunction *self, void * /*closure*/)
{
  return bpy_op_fn_get_doc_impl(self);
}

static PyGetSetDef bpy_op_fn_getsetters[] = {
    {"bl_options",
     (getter)bpy_op_fn_get_bl_options,
     nullptr,
     bpy_op_fn_get_bl_options_doc,
     nullptr},
    /* No doc-string, as this is standard part of the Python spec. */
    {"__doc__", (getter)bpy_op_fn_get_doc, nullptr, nullptr, nullptr},
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
    "   :arg context: Execution context (optional)\n"
    "   :type context: str\n"
    "   :arg undo: Force undo behavior (optional)\n"
    "   :type undo: bool\n"
    "   :arg kwargs: Operator properties\n"
    "   :return: Set of completion status flags\n"
    "   :rtype: set[str]\n");
PyTypeObject BPyOpFunctionType = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "BPyOpFunction",
    /*tp_basicsize*/ sizeof(BPyOpFunction),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)bpy_op_fn_dealloc,
    /*tp_print*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)bpy_op_fn_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ (ternaryfunc)bpy_op_fn_call,
    /*tp_str*/ (reprfunc)bpy_op_fn_str,
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
  BPyOpFunction *op_fn = (BPyOpFunction *)PyObject_New(BPyOpFunction, &BPyOpFunctionType);
  if (!op_fn) {
    return nullptr;
  }

  /* Construct the Blender `idname` (e.g., `OBJECT_OT_select_all`). */
  char op_mod_str_upper[OP_MAX_TYPENAME];
  BLI_strncpy(op_mod_str_upper, op_mod_str, sizeof(op_mod_str_upper));
  BLI_str_toupper_ascii(op_mod_str_upper, sizeof(op_mod_str_upper));

  const size_t idname_len = BLI_snprintf(
      op_fn->idname, sizeof(op_fn->idname), "%s_OT_%s", op_mod_str_upper, op_fn_str);
  /* Prevented by the #OP_MAX_TYPENAME check. */
  BLI_assert(idname_len < sizeof(op_fn->idname));
  UNUSED_VARS_NDEBUG(idname_len);
  return (PyObject *)op_fn;
}

/** \} */
