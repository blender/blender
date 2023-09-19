/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 *
 * This file inserts an exit callback into Python's 'atexit' module.
 * Without this `sys.exit()` can crash because blender is not properly closing
 * resources.
 */

#include <Python.h>

#include "BLI_utildefines.h"

#include "bpy.h" /* own include */
#include "bpy_capi_utils.h"

#include "WM_api.hh"

static PyObject *bpy_atexit(PyObject * /*self*/, PyObject * /*args*/, PyObject * /*kw*/)
{
  /* NOTE(@ideasman42): This doesn't have to match Blender shutting down exactly,
   * leaks reported by memory checking tools may be reported but are harmless
   * and don't have to be *fixed* unless doing so is trivial.
   *
   * Just handle the basics:
   * - Free resources avoiding crashes and errors on exit.
   * - Remove Blender's temporary directory.
   *
   * Anything else that prevents `sys.exit(..)` from exiting gracefully should be handled here too.
   */

  bContext *C = BPY_context_get();
  /* As Python requested the exit, it handles shutting it's self down. */
  const bool do_python = false;
  /* User actions such as saving the session, preferences, recent-files for e.g.
   * should be skipped because an explicit call to exit is more likely to be used as part of
   * automated processes shouldn't impact the users session in the future. */
  const bool do_user_exit_actions = false;

  WM_exit_ex(C, do_python, do_user_exit_actions);

  Py_RETURN_NONE;
}

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static PyMethodDef meth_bpy_atexit = {"bpy_atexit", (PyCFunction)bpy_atexit, METH_NOARGS, nullptr};

#if (defined(__GNUC__) && !defined(__clang__))
#  pragma GCC diagnostic pop
#endif

static PyObject *func_bpy_atregister = nullptr; /* borrowed reference, `atexit` holds. */

static void atexit_func_call(const char *func_name, PyObject *atexit_func_arg)
{
  /* NOTE(@ideasman42): no error checking, if any of these fail we'll get a crash
   * this is intended, but if its problematic it could be changed. */

  PyObject *atexit_mod = PyImport_ImportModuleLevel("atexit", nullptr, nullptr, nullptr, 0);
  PyObject *atexit_func = PyObject_GetAttrString(atexit_mod, func_name);
  PyObject *args = PyTuple_New(1);
  PyObject *ret;

  PyTuple_SET_ITEM(args, 0, atexit_func_arg);
  Py_INCREF(atexit_func_arg); /* only incref so we don't dec'ref along with 'args' */

  ret = PyObject_CallObject(atexit_func, args);

  Py_DECREF(atexit_mod);
  Py_DECREF(atexit_func);
  Py_DECREF(args);

  if (ret) {
    Py_DECREF(ret);
  }
  else { /* should never happen */
    PyErr_Print();
  }
}

void BPY_atexit_register()
{
  /* atexit module owns this new function reference */
  BLI_assert(func_bpy_atregister == nullptr);

  func_bpy_atregister = (PyObject *)PyCFunction_New(&meth_bpy_atexit, nullptr);
  atexit_func_call("register", func_bpy_atregister);
}

void BPY_atexit_unregister()
{
  BLI_assert(func_bpy_atregister != nullptr);

  atexit_func_call("unregister", func_bpy_atregister);
  func_bpy_atregister = nullptr; /* don't really need to set but just in case */
}
