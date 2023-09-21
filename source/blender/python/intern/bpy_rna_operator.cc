/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file extends `bpy.types.Operator` with C/Python API methods and attributes.
 */

#include <Python.h>

#include "BLI_string.h"

#include "BKE_context.h"

#include "../generic/python_utildefines.h"

#include "BPY_extern.h"
#include "bpy_capi_utils.h"

#include "bpy_rna_operator.h" /* Own include. */

/* -------------------------------------------------------------------- */
/** \name Operator `poll_message_set` Method
 * \{ */

static char *pyop_poll_message_get_fn(bContext * /*C*/, void *user_data)
{
  PyGILState_STATE gilstate = PyGILState_Ensure();

  PyObject *py_args = static_cast<PyObject *>(user_data);
  PyObject *py_func_or_msg = PyTuple_GET_ITEM(py_args, 0);

  if (PyUnicode_Check(py_func_or_msg)) {
    Py_ssize_t msg_len;
    const char *msg = PyUnicode_AsUTF8AndSize(py_func_or_msg, &msg_len);
    return BLI_strdupn(msg, msg_len);
  }

  PyObject *py_args_after_first = PyTuple_GetSlice(py_args, 1, PY_SSIZE_T_MAX);
  PyObject *py_msg = PyObject_CallObject(py_func_or_msg, py_args_after_first);
  Py_DECREF(py_args_after_first);

  char *msg = nullptr;
  bool error = false;

  /* nullptr for no string. */
  if (py_msg == nullptr) {
    error = true;
  }
  else {
    if (py_msg == Py_None) {
      /* pass */
    }
    else if (PyUnicode_Check(py_msg)) {
      Py_ssize_t msg_src_len;
      const char *msg_src = PyUnicode_AsUTF8AndSize(py_msg, &msg_src_len);
      msg = BLI_strdupn(msg_src, msg_src_len);
    }
    else {
      PyErr_Format(PyExc_TypeError,
                   "poll_message_set(function, ...): expected string or None, got %.200s",
                   Py_TYPE(py_msg)->tp_name);
      error = true;
    }
    Py_DECREF(py_msg);
  }

  if (error) {
    PyErr_Print();
    PyErr_Clear();
  }

  PyGILState_Release(gilstate);
  return msg;
}

static void pyop_poll_message_free_fn(bContext * /*C*/, void *user_data)
{
  /* Handles the GIL. */
  BPY_DECREF(user_data);
}

PyDoc_STRVAR(BPY_rna_operator_poll_message_set_doc,
             ".. classmethod:: poll_message_set(message, *args)\n"
             "\n"
             "   Set the message to show in the tool-tip when poll fails.\n"
             "\n"
             "   When message is callable, "
             "additional user defined positional arguments are passed to the message function.\n"
             "\n"
             "   :arg message: The message or a function that returns the message.\n"
             "   :type message: string or a callable that returns a string or None.\n");

static PyObject *BPY_rna_operator_poll_message_set(PyObject * /*self*/, PyObject *args)
{
  const Py_ssize_t args_len = PyTuple_GET_SIZE(args);
  if (args_len == 0) {
    PyErr_SetString(PyExc_ValueError,
                    "poll_message_set(message, ...): requires a message argument");
    return nullptr;
  }

  PyObject *py_func_or_msg = PyTuple_GET_ITEM(args, 0);

  if (PyUnicode_Check(py_func_or_msg)) {
    if (args_len > 1) {
      PyErr_SetString(PyExc_ValueError,
                      "poll_message_set(message): does not support additional arguments");
      return nullptr;
    }
  }
  else if (PyCallable_Check(py_func_or_msg)) {
    /* pass */
  }
  else {
    PyErr_Format(PyExc_TypeError,
                 "poll_message_set(message, ...): "
                 "expected at least 1 string or callable argument, got %.200s",
                 Py_TYPE(py_func_or_msg)->tp_name);
    return nullptr;
  }

  bContext *C = BPY_context_get();
  bContextPollMsgDyn_Params params{};
  params.get_fn = pyop_poll_message_get_fn;
  params.free_fn = pyop_poll_message_free_fn;
  params.user_data = Py_INCREF_RET(args);

  CTX_wm_operator_poll_msg_set_dynamic(C, &params);

  Py_RETURN_NONE;
}

PyMethodDef BPY_rna_operator_poll_message_set_method_def = {
    "poll_message_set",
    (PyCFunction)BPY_rna_operator_poll_message_set,
    METH_VARARGS | METH_STATIC,
    BPY_rna_operator_poll_message_set_doc,
};

/** \} */
