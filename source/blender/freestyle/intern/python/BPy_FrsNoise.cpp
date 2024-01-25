/* SPDX-FileCopyrightText: 2004-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_FrsNoise.h"
#include "BPy_Convert.h"

#include "../system/RandGen.h"

#include "BLI_sys_types.h"

#include <sstream>

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int FrsNoise_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&FrsNoise_Type) < 0) {
    return -1;
  }
  Py_INCREF(&FrsNoise_Type);
  PyModule_AddObject(module, "Noise", (PyObject *)&FrsNoise_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    /* Wrap. */
    FrsNoise_doc,
    "Class to provide Perlin noise functionalities.\n"
    "\n"
    ".. method:: __init__(seed = -1)\n"
    "\n"
    "   Builds a Noise object. Seed is an optional argument. The seed value is used\n"
    "   as a seed for random number generation if it is equal to or greater than zero;\n"
    "   otherwise, time is used as a seed.\n"
    "\n"
    "   :arg seed: Seed for random number generation.\n"
    "   :type seed: int");

static int FrsNoise_init(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"seed", nullptr};
  long seed = -1;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|l", (char **)kwlist, &seed)) {
    return -1;
  }
  self->n = new Noise(seed);
  self->pn = new PseudoNoise();
  return 0;
}

static void FrsNoise_dealloc(BPy_FrsNoise *self)
{
  delete self->n;
  delete self->pn;
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *FrsNoise_repr(BPy_FrsNoise *self)
{
  return PyUnicode_FromFormat("Noise - address: %p", self->n);
}

PyDoc_STRVAR(
    /* Wrap. */
    FrsNoise_turbulence1_doc,
    ".. method:: turbulence1(v, freq, amp, oct=4)\n"
    "\n"
    "   Returns a noise value for a 1D element.\n"
    "\n"
    "   :arg v: One-dimensional sample point.\n"
    "   :type v: float\n"
    "   :arg freq: Noise frequency.\n"
    "   :type freq: float\n"
    "   :arg amp: Amplitude.\n"
    "   :type amp: float\n"
    "   :arg oct: Number of octaves.\n"
    "   :type oct: int\n"
    "   :return: A noise value.\n"
    "   :rtype: float");

static PyObject *FrsNoise_drand(BPy_FrsNoise * /*self*/, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"seed", nullptr};
  long seed = 0;
  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|I", (char **)kwlist, &seed)) {
    PyErr_SetString(PyExc_TypeError, "optional argument 1 must be of type int");
    return nullptr;
  }
  if (seed) {
    RandGen::srand48(seed);
  }
  return PyFloat_FromDouble(RandGen::drand48());
}

static PyObject *FrsNoise_turbulence_smooth(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"v", "oct", nullptr};

  double x;  // NOTE: this has to be a double (not float)
  uint nbOctaves = 8;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "d|I", (char **)kwlist, &x, &nbOctaves)) {
    return nullptr;
  }
  return PyFloat_FromDouble(self->pn->turbulenceSmooth(x, nbOctaves));
}

static PyObject *FrsNoise_turbulence1(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"v", "freq", "amp", "oct", nullptr};
  float f1, f2, f3;
  uint i = 4;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "fff|I", (char **)kwlist, &f1, &f2, &f3, &i)) {
    return nullptr;
  }
  return PyFloat_FromDouble(self->n->turbulence1(f1, f2, f3, i));
}

PyDoc_STRVAR(
    /* Wrap. */
    FrsNoise_turbulence2_doc,
    ".. method:: turbulence2(v, freq, amp, oct=4)\n"
    "\n"
    "   Returns a noise value for a 2D element.\n"
    "\n"
    "   :arg v: Two-dimensional sample point.\n"
    "   :type v: :class:`mathutils.Vector`, list or tuple of 2 real numbers\n"
    "   :arg freq: Noise frequency.\n"
    "   :type freq: float\n"
    "   :arg amp: Amplitude.\n"
    "   :type amp: float\n"
    "   :arg oct: Number of octaves.\n"
    "   :type oct: int\n"
    "   :return: A noise value.\n"
    "   :rtype: float");

static PyObject *FrsNoise_turbulence2(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"v", "freq", "amp", "oct", nullptr};
  PyObject *obj1;
  float f2, f3;
  uint i = 4;
  Vec2f vec;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "Off|I", (char **)kwlist, &obj1, &f2, &f3, &i)) {
    return nullptr;
  }
  if (!Vec2f_ptr_from_PyObject(obj1, vec)) {
    PyErr_SetString(PyExc_TypeError,
                    "argument 1 must be a 2D vector (either a list of 2 elements or Vector)");
    return nullptr;
  }
  float t = self->n->turbulence2(vec, f2, f3, i);
  return PyFloat_FromDouble(t);
}

PyDoc_STRVAR(
    /* Wrap. */
    FrsNoise_turbulence3_doc,
    ".. method:: turbulence3(v, freq, amp, oct=4)\n"
    "\n"
    "   Returns a noise value for a 3D element.\n"
    "\n"
    "   :arg v: Three-dimensional sample point.\n"
    "   :type v: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n"
    "   :arg freq: Noise frequency.\n"
    "   :type freq: float\n"
    "   :arg amp: Amplitude.\n"
    "   :type amp: float\n"
    "   :arg oct: Number of octaves.\n"
    "   :type oct: int\n"
    "   :return: A noise value.\n"
    "   :rtype: float");

static PyObject *FrsNoise_turbulence3(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"v", "freq", "amp", "oct", nullptr};
  PyObject *obj1;
  float f2, f3;
  uint i = 4;
  Vec3f vec;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "Off|I", (char **)kwlist, &obj1, &f2, &f3, &i)) {
    return nullptr;
  }
  if (!Vec3f_ptr_from_PyObject(obj1, vec)) {
    PyErr_SetString(PyExc_TypeError,
                    "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
    return nullptr;
  }
  float t = self->n->turbulence3(vec, f2, f3, i);
  return PyFloat_FromDouble(t);
}

PyDoc_STRVAR(
    /* Wrap. */
    FrsNoise_smoothNoise1_doc,
    ".. method:: smoothNoise1(v)\n"
    "\n"
    "   Returns a smooth noise value for a 1D element.\n"
    "\n"
    "   :arg v: One-dimensional sample point.\n"
    "   :type v: float\n"
    "   :return: A smooth noise value.\n"
    "   :rtype: float");

static PyObject *FrsNoise_smoothNoise1(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"v", nullptr};
  float f;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "f", (char **)kwlist, &f)) {
    return nullptr;
  }
  return PyFloat_FromDouble(self->n->smoothNoise1(f));
}

PyDoc_STRVAR(
    /* Wrap. */
    FrsNoise_smoothNoise2_doc,
    ".. method:: smoothNoise2(v)\n"
    "\n"
    "   Returns a smooth noise value for a 2D element.\n"
    "\n"
    "   :arg v: Two-dimensional sample point.\n"
    "   :type v: :class:`mathutils.Vector`, list or tuple of 2 real numbers\n"
    "   :return: A smooth noise value.\n"
    "   :rtype: float");

static PyObject *FrsNoise_smoothNoise2(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"v", nullptr};
  PyObject *obj;
  Vec2f vec;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (char **)kwlist, &obj)) {
    return nullptr;
  }
  if (!Vec2f_ptr_from_PyObject(obj, vec)) {
    PyErr_SetString(PyExc_TypeError,
                    "argument 1 must be a 2D vector (either a list of 2 elements or Vector)");
    return nullptr;
  }
  float t = self->n->smoothNoise2(vec);
  return PyFloat_FromDouble(t);
}

PyDoc_STRVAR(
    /* Wrap. */
    FrsNoise_smoothNoise3_doc,
    ".. method:: smoothNoise3(v)\n"
    "\n"
    "   Returns a smooth noise value for a 3D element.\n"
    "\n"
    "   :arg v: Three-dimensional sample point.\n"
    "   :type v: :class:`mathutils.Vector`, list or tuple of 3 real numbers\n"
    "   :return: A smooth noise value.\n"
    "   :rtype: float");

static PyObject *FrsNoise_smoothNoise3(BPy_FrsNoise *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"v", nullptr};
  PyObject *obj;
  Vec3f vec;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O", (char **)kwlist, &obj)) {
    return nullptr;
  }
  if (!Vec3f_ptr_from_PyObject(obj, vec)) {
    PyErr_SetString(PyExc_TypeError,
                    "argument 1 must be a 3D vector (either a list of 3 elements or Vector)");
    return nullptr;
  }
  float t = self->n->smoothNoise3(vec);
  return PyFloat_FromDouble(t);
}

static PyMethodDef BPy_FrsNoise_methods[] = {
    {"turbulence1",
     (PyCFunction)FrsNoise_turbulence1,
     METH_VARARGS | METH_KEYWORDS,
     FrsNoise_turbulence1_doc},
    {"turbulence2",
     (PyCFunction)FrsNoise_turbulence2,
     METH_VARARGS | METH_KEYWORDS,
     FrsNoise_turbulence2_doc},
    {"turbulence3",
     (PyCFunction)FrsNoise_turbulence3,
     METH_VARARGS | METH_KEYWORDS,
     FrsNoise_turbulence3_doc},
    {"smoothNoise1",
     (PyCFunction)FrsNoise_smoothNoise1,
     METH_VARARGS | METH_KEYWORDS,
     FrsNoise_smoothNoise1_doc},
    {"smoothNoise2",
     (PyCFunction)FrsNoise_smoothNoise2,
     METH_VARARGS | METH_KEYWORDS,
     FrsNoise_smoothNoise2_doc},
    {"smoothNoise3",
     (PyCFunction)FrsNoise_smoothNoise3,
     METH_VARARGS | METH_KEYWORDS,
     FrsNoise_smoothNoise3_doc},
    {"rand", (PyCFunction)FrsNoise_drand, METH_VARARGS | METH_KEYWORDS, nullptr},
    {"turbulence_smooth",
     (PyCFunction)FrsNoise_turbulence_smooth,
     METH_VARARGS | METH_KEYWORDS,
     nullptr},
    {nullptr, nullptr, 0, nullptr},
};

/*-----------------------BPy_FrsNoise type definition ------------------------------*/

PyTypeObject FrsNoise_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Noise",
    /*tp_basicsize*/ sizeof(BPy_FrsNoise),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)FrsNoise_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)FrsNoise_repr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    /*tp_doc*/ FrsNoise_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ BPy_FrsNoise_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)FrsNoise_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
