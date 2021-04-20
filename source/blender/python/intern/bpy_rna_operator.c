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
 * This file extends `bpy.types.Operator` with C/Python API methods and attributes.
 */

#include <Python.h>

#include "BLI_string.h"

#include "BKE_context.h"

#include "../generic/python_utildefines.h"

#include "BPY_extern.h"
#include "bpy_capi_utils.h"

/* -------------------------------------------------------------------- */
/** \name Operator `poll_message_set` Method
 * \{ */

static char *pyop_poll_message_get_fn(bContext *UNUSED(C), void *user_data)
{
  PyGILState_STATE gilstate = PyGILState_Ensure();

  PyObject *py_args = user_data;
  PyObject *py_func_or_msg = PyTuple_GET_ITEM(py_args, 0);

  if (PyUnicode_Check(py_func_or_msg)) {
    return BLI_strdup(PyUnicode_AsUTF8(py_func_or_msg));
  }

  PyObject *py_args_after_first = PyTuple_GetSlice(py_args, 1, PY_SSIZE_T_MAX);
  PyObject *py_msg = PyObject_CallObject(py_func_or_msg, py_args_after_first);
  Py_DECREF(py_args_after_first);

  char *msg = NULL;
  bool error = false;

  /* NULL for no string. */
  if (py_msg == NULL) {
    error = true;
  }
  else {
    if (py_msg == Py_None) {
      /* pass */
    }
    else if (PyUnicode_Check(py_msg)) {
      msg = BLI_strdup(PyUnicode_AsUTF8(py_msg));
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

static void pyop_poll_message_free_fn(bContext *UNUSED(C), void *user_data)
{
  /* Handles the GIL. */
  BPY_DECREF(user_data);
}

PyDoc_STRVAR(BPY_rna_operator_poll_message_set_doc,
             ".. method:: poll_message_set(message, ...)\n"
             "\n"
             "   Set the message to show in the tool-tip when poll fails.\n"
             "\n"
             "   When message is callable, "
             "additional user defined positional arguments are passed to the message function.\n"
             "\n"
             "   :param message: The message or a function that returns the message.\n"
             "   :type message: string or a callable that returns a string or None.\n");

static PyObject *BPY_rna_operator_poll_message_set(PyObject *UNUSED(self), PyObject *args)
{
  const ssize_t args_len = PyTuple_GET_SIZE(args);
  if (args_len == 0) {
    PyErr_SetString(PyExc_ValueError,
                    "poll_message_set(message, ...): requires a message argument");
    return NULL;
  }

  PyObject *py_func_or_msg = PyTuple_GET_ITEM(args, 0);

  if (PyUnicode_Check(py_func_or_msg)) {
    if (args_len > 1) {
      PyErr_SetString(PyExc_ValueError,
                      "poll_message_set(message): does not support additional arguments");
      return NULL;
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
    return NULL;
  }

  bContext *C = BPY_context_get();
  struct bContextPollMsgDyn_Params params = {
      .get_fn = pyop_poll_message_get_fn,
      .free_fn = pyop_poll_message_free_fn,
      .user_data = Py_INCREF_RET(args),
  };

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
