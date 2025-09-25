/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_timer.h"

#include <Python.h>

#include <algorithm>

#include "bpy_app_timers.hh"

#include "../generic/python_compat.hh" /* IWYU pragma: keep. */

static double handle_returned_value(PyObject *function, PyObject *ret)
{
  if (ret == nullptr) {
    PyErr_PrintEx(0);
    return -1;
  }

  if (ret == Py_None) {
    return -1;
  }

  double value = PyFloat_AsDouble(ret);
  if (value == -1.0f && PyErr_Occurred()) {
    PyErr_Clear();
    printf("Error: 'bpy.app.timers' callback ");
    PyObject_Print(function, stdout, Py_PRINT_RAW);
    printf(" did not return None or float.\n");
    return -1;
  }

  value = std::max(value, 0.0);

  return value;
}

static double py_timer_execute(uintptr_t /*uuid*/, void *user_data)
{
  PyGILState_STATE gilstate = PyGILState_Ensure();

  PyObject *function = static_cast<PyObject *>(user_data);

  PyObject *py_ret = PyObject_CallObject(function, nullptr);
  const double ret = handle_returned_value(function, py_ret);

  PyGILState_Release(gilstate);

  return ret;
}

static void py_timer_free(uintptr_t /*uuid*/, void *user_data)
{
  PyGILState_STATE gilstate = PyGILState_Ensure();

  PyObject *function = static_cast<PyObject *>(user_data);
  Py_DECREF(function);

  PyGILState_Release(gilstate);
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_app_timers_register_doc,
    ".. function:: register(function, *, first_interval=0, persistent=False)\n"
    "\n"
    "   Add a new function that will be called after the specified amount of seconds.\n"
    "   The function gets no arguments and is expected to return either None or a float.\n"
    "   If ``None`` is returned, the timer will be unregistered.\n"
    "   A returned number specifies the delay until the function is called again.\n"
    "   ``functools.partial`` can be used to assign some parameters.\n"
    "\n"
    "   :arg function: The function that should called.\n"
    "   :type function: Callable[[], float | None]\n"
    "   :arg first_interval: Seconds until the callback should be called the first time.\n"
    "   :type first_interval: float\n"
    "   :arg persistent: Don't remove timer when a new file is loaded.\n"
    "   :type persistent: bool\n");
static PyObject *bpy_app_timers_register(PyObject * /*self*/, PyObject *args, PyObject *kw)
{
  PyObject *function;
  double first_interval = 0;
  int persistent = false;

  static const char *_keywords[] = {"function", "first_interval", "persistent", nullptr};
  static _PyArg_Parser _parser = {
      PY_ARG_PARSER_HEAD_COMPAT()
      "O"  /* `function` */
      "|$" /* Optional keyword only arguments. */
      "d"  /* `first_interval` */
      "p"  /* `persistent` */
      ":register",
      _keywords,
      nullptr,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &function, &first_interval, &persistent))
  {
    return nullptr;
  }

  if (!PyCallable_Check(function)) {
    PyErr_SetString(PyExc_TypeError, "function is not callable");
    return nullptr;
  }

  Py_INCREF(function);
  BLI_timer_register(
      intptr_t(function), py_timer_execute, function, py_timer_free, first_interval, persistent);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_app_timers_unregister_doc,
    ".. function:: unregister(function)\n"
    "\n"
    "   Unregister timer.\n"
    "\n"
    "   :arg function: Function to unregister.\n"
    "   :type function: Callable[[], float | None]\n");
static PyObject *bpy_app_timers_unregister(PyObject * /*self*/, PyObject *function)
{
  if (!BLI_timer_unregister(intptr_t(function))) {
    PyErr_SetString(PyExc_ValueError, "function is not registered");
    return nullptr;
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(
    /* Wrap. */
    bpy_app_timers_is_registered_doc,
    ".. function:: is_registered(function)\n"
    "\n"
    "   Check if this function is registered as a timer.\n"
    "\n"
    "   :arg function: Function to check.\n"
    "   :type function: Callable[[], float | None]\n"
    "   :return: True when this function is registered, otherwise False.\n"
    "   :rtype: bool\n");
static PyObject *bpy_app_timers_is_registered(PyObject * /*self*/, PyObject *function)
{
  const bool ret = BLI_timer_is_registered(intptr_t(function));
  return PyBool_FromLong(ret);
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

static PyMethodDef M_AppTimers_methods[] = {
    {"register",
     (PyCFunction)bpy_app_timers_register,
     METH_VARARGS | METH_KEYWORDS,
     bpy_app_timers_register_doc},
    {"unregister", (PyCFunction)bpy_app_timers_unregister, METH_O, bpy_app_timers_unregister_doc},
    {"is_registered",
     (PyCFunction)bpy_app_timers_is_registered,
     METH_O,
     bpy_app_timers_is_registered_doc},
    {nullptr, nullptr, 0, nullptr},
};

#ifdef __GNUC__
#  ifdef __clang__
#    pragma clang diagnostic pop
#  else
#    pragma GCC diagnostic pop
#  endif
#endif

static PyModuleDef M_AppTimers_module_def = {
    /*m_base*/ PyModuleDef_HEAD_INIT,
    /*m_name*/ "bpy.app.timers",
    /*m_doc*/ nullptr,
    /*m_size*/ 0,
    /*m_methods*/ M_AppTimers_methods,
    /*m_slots*/ nullptr,
    /*m_traverse*/ nullptr,
    /*m_clear*/ nullptr,
    /*m_free*/ nullptr,
};

PyObject *BPY_app_timers_module()
{
  PyObject *sys_modules = PyImport_GetModuleDict();
  PyObject *mod = PyModule_Create(&M_AppTimers_module_def);
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(mod), mod);
  return mod;
}
