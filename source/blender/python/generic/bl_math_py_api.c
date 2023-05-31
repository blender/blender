/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * \file
 * \ingroup pygen
 *
 * This file defines the 'bl_math' module, a module for math utilities.
 */

#include <Python.h>

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "py_capi_utils.h"

#include "bl_math_py_api.h"

/* -------------------------------------------------------------------- */
/** \name Module Doc String
 * \{ */

PyDoc_STRVAR(M_bl_math_doc, "Miscellaneous math utilities module");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Python Functions
 * \{ */

PyDoc_STRVAR(py_bl_math_clamp_doc,
             ".. function:: clamp(value, min=0, max=1)\n"
             "\n"
             "   Clamps the float value between minimum and maximum. To avoid\n"
             "   confusion, any call must use either one or all three arguments.\n"
             "\n"
             "   :arg value: The value to clamp.\n"
             "   :type value: float\n"
             "   :arg min: The minimum value, defaults to 0.\n"
             "   :type min: float\n"
             "   :arg max: The maximum value, defaults to 1.\n"
             "   :type max: float\n"
             "   :return: The clamped value.\n"
             "   :rtype: float\n");
static PyObject *py_bl_math_clamp(PyObject *UNUSED(self), PyObject *args)
{
  double x, minv = 0.0, maxv = 1.0;

  if (PyTuple_Size(args) <= 1) {
    if (!PyArg_ParseTuple(args, "d:clamp", &x)) {
      return NULL;
    }
  }
  else {
    if (!PyArg_ParseTuple(args, "ddd:clamp", &x, &minv, &maxv)) {
      return NULL;
    }
  }

  CLAMP(x, minv, maxv);

  return PyFloat_FromDouble(x);
}

PyDoc_STRVAR(py_bl_math_lerp_doc,
             ".. function:: lerp(from_value, to_value, factor)\n"
             "\n"
             "   Linearly interpolate between two float values based on factor.\n"
             "\n"
             "   :arg from_value: The value to return when factor is 0.\n"
             "   :type from_value: float\n"
             "   :arg to_value: The value to return when factor is 1.\n"
             "   :type to_value: float\n"
             "   :arg factor: The interpolation value, normally in [0.0, 1.0].\n"
             "   :type factor: float\n"
             "   :return: The interpolated value.\n"
             "   :rtype: float\n");
static PyObject *py_bl_math_lerp(PyObject *UNUSED(self), PyObject *args)
{
  double a, b, x;
  if (!PyArg_ParseTuple(args, "ddd:lerp", &a, &b, &x)) {
    return NULL;
  }

  return PyFloat_FromDouble(a * (1.0 - x) + b * x);
}

PyDoc_STRVAR(py_bl_math_smoothstep_doc,
             ".. function:: smoothstep(from_value, to_value, value)\n"
             "\n"
             "   Performs smooth interpolation between 0 and 1 as value changes between from and "
             "to values.\n"
             "   Outside the range the function returns the same value as the nearest edge.\n"
             "\n"
             "   :arg from_value: The edge value where the result is 0.\n"
             "   :type from_value: float\n"
             "   :arg to_value: The edge value where the result is 1.\n"
             "   :type to_value: float\n"
             "   :arg factor: The interpolation value.\n"
             "   :type factor: float\n"
             "   :return: The interpolated value in [0.0, 1.0].\n"
             "   :rtype: float\n");
static PyObject *py_bl_math_smoothstep(PyObject *UNUSED(self), PyObject *args)
{
  double a, b, x;
  if (!PyArg_ParseTuple(args, "ddd:smoothstep", &a, &b, &x)) {
    return NULL;
  }

  double t = (x - a) / (b - a);

  CLAMP(t, 0.0, 1.0);

  return PyFloat_FromDouble(t * t * (3.0 - 2.0 * t));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Module Definition
 * \{ */

static PyMethodDef M_bl_math_methods[] = {
    {"clamp", (PyCFunction)py_bl_math_clamp, METH_VARARGS, py_bl_math_clamp_doc},
    {"lerp", (PyCFunction)py_bl_math_lerp, METH_VARARGS, py_bl_math_lerp_doc},
    {"smoothstep", (PyCFunction)py_bl_math_smoothstep, METH_VARARGS, py_bl_math_smoothstep_doc},
    {NULL, NULL, 0, NULL},
};

static struct PyModuleDef M_bl_math_module_def = {
    PyModuleDef_HEAD_INIT,
    /*m_name*/ "bl_math",
    /*m_doc*/ M_bl_math_doc,
    /*m_size*/ 0,
    /*m_methods*/ M_bl_math_methods,
    /*m_slots*/ NULL,
    /*m_traverse*/ NULL,
    /*m_clear*/ NULL,
    /*m_free*/ NULL,
};

PyMODINIT_FUNC BPyInit_bl_math(void)
{
  PyObject *submodule = PyModule_Create(&M_bl_math_module_def);
  return submodule;
}

/** \} */
