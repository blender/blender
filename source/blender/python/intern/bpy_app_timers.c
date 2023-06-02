/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup pythonintern
 */

#include "BLI_timer.h"
#include "BLI_utildefines.h"
#include "PIL_time.h"
#include <Python.h>

#include "BPY_extern.h"
#include "bpy_app_timers.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

static double handle_returned_value(PyObject *function, PyObject *ret)
{
  if (ret == NULL) {
    PyErr_PrintEx(0);
    PyErr_Clear();
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

  if (value < 0.0) {
    value = 0.0;
  }

  return value;
}

static double py_timer_execute(uintptr_t UNUSED(uuid), void *user_data)
{
  PyObject *function = user_data;

  PyGILState_STATE gilstate;
  gilstate = PyGILState_Ensure();

  PyObject *py_ret = PyObject_CallObject(function, NULL);
  const double ret = handle_returned_value(function, py_ret);

  PyGILState_Release(gilstate);

  return ret;
}

static void py_timer_free(uintptr_t UNUSED(uuid), void *user_data)
{
  PyObject *function = user_data;

  PyGILState_STATE gilstate;
  gilstate = PyGILState_Ensure();

  Py_DECREF(function);

  PyGILState_Release(gilstate);
}

PyDoc_STRVAR(
    bpy_app_timers_register_doc,
    ".. function:: register(function, first_interval=0, persistent=False)\n"
    "\n"
    "   Add a new function that will be called after the specified amount of seconds.\n"
    "   The function gets no arguments and is expected to return either None or a float.\n"
    "   If ``None`` is returned, the timer will be unregistered.\n"
    "   A returned number specifies the delay until the function is called again.\n"
    "   ``functools.partial`` can be used to assign some parameters.\n"
    "\n"
    "   :arg function: The function that should called.\n"
    "   :type function: Callable[[], Union[float, None]]\n"
    "   :arg first_interval: Seconds until the callback should be called the first time.\n"
    "   :type first_interval: float\n"
    "   :arg persistent: Don't remove timer when a new file is loaded.\n"
    "   :type persistent: bool\n");
static PyObject *bpy_app_timers_register(PyObject *UNUSED(self), PyObject *args, PyObject *kw)
{
  PyObject *function;
  double first_interval = 0;
  int persistent = false;

  static const char *_keywords[] = {"function", "first_interval", "persistent", NULL};
  static _PyArg_Parser _parser = {
      "O"  /* `function` */
      "|$" /* Optional keyword only arguments. */
      "d"  /* `first_interval` */
      "p"  /* `persistent` */
      ":register",
      _keywords,
      0,
  };
  if (!_PyArg_ParseTupleAndKeywordsFast(
          args, kw, &_parser, &function, &first_interval, &persistent)) {
    return NULL;
  }

  if (!PyCallable_Check(function)) {
    PyErr_SetString(PyExc_TypeError, "function is not callable");
    return NULL;
  }

  Py_INCREF(function);
  BLI_timer_register(
      (intptr_t)function, py_timer_execute, function, py_timer_free, first_interval, persistent);
  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_app_timers_unregister_doc,
             ".. function:: unregister(function)\n"
             "\n"
             "   Unregister timer.\n"
             "\n"
             "   :arg function: Function to unregister.\n"
             "   :type function: function\n");
static PyObject *bpy_app_timers_unregister(PyObject *UNUSED(self), PyObject *function)
{
  if (!BLI_timer_unregister((intptr_t)function)) {
    PyErr_SetString(PyExc_ValueError, "Error: function is not registered");
    return NULL;
  }
  Py_RETURN_NONE;
}

PyDoc_STRVAR(bpy_app_timers_is_registered_doc,
             ".. function:: is_registered(function)\n"
             "\n"
             "   Check if this function is registered as a timer.\n"
             "\n"
             "   :arg function: Function to check.\n"
             "   :type function: int\n"
             "   :return: True when this function is registered, otherwise False.\n"
             "   :rtype: bool\n");
static PyObject *bpy_app_timers_is_registered(PyObject *UNUSED(self), PyObject *function)
{
  const bool ret = BLI_timer_is_registered((intptr_t)function);
  return PyBool_FromLong(ret);
}

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
    {NULL, NULL, 0, NULL},
};

static PyModuleDef M_AppTimers_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "bpy.app.timers",
    /*m_doc*/ NULL,
    /*m_size*/ 0,
    /*m_methods*/ M_AppTimers_methods,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyObject *BPY_app_timers_module(void)
{
  PyObject *sys_modules = PyImport_GetModuleDict();
  PyObject *mod = PyModule_Create(&M_AppTimers_module_def);
  PyDict_SetItem(sys_modules, PyModule_GetNameObject(mod), mod);
  return mod;
}
