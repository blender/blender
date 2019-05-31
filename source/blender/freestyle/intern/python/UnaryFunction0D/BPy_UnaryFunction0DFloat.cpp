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
 * \ingroup freestyle
 */

#include "BPy_UnaryFunction0DFloat.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_float/BPy_GetCurvilinearAbscissaF0D.h"
#include "UnaryFunction0D_float/BPy_GetParameterF0D.h"
#include "UnaryFunction0D_float/BPy_GetViewMapGradientNormF0D.h"
#include "UnaryFunction0D_float/BPy_ReadCompleteViewMapPixelF0D.h"
#include "UnaryFunction0D_float/BPy_ReadMapPixelF0D.h"
#include "UnaryFunction0D_float/BPy_ReadSteerableViewMapPixelF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DFloat_Init(PyObject *module)
{
  if (module == NULL) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction0DFloat_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction0DFloat_Type);
  PyModule_AddObject(module, "UnaryFunction0DFloat", (PyObject *)&UnaryFunction0DFloat_Type);

  if (PyType_Ready(&GetCurvilinearAbscissaF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetCurvilinearAbscissaF0D_Type);
  PyModule_AddObject(
      module, "GetCurvilinearAbscissaF0D", (PyObject *)&GetCurvilinearAbscissaF0D_Type);

  if (PyType_Ready(&GetParameterF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetParameterF0D_Type);
  PyModule_AddObject(module, "GetParameterF0D", (PyObject *)&GetParameterF0D_Type);

  if (PyType_Ready(&GetViewMapGradientNormF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetViewMapGradientNormF0D_Type);
  PyModule_AddObject(
      module, "GetViewMapGradientNormF0D", (PyObject *)&GetViewMapGradientNormF0D_Type);

  if (PyType_Ready(&ReadCompleteViewMapPixelF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ReadCompleteViewMapPixelF0D_Type);
  PyModule_AddObject(
      module, "ReadCompleteViewMapPixelF0D", (PyObject *)&ReadCompleteViewMapPixelF0D_Type);

  if (PyType_Ready(&ReadMapPixelF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ReadMapPixelF0D_Type);
  PyModule_AddObject(module, "ReadMapPixelF0D", (PyObject *)&ReadMapPixelF0D_Type);

  if (PyType_Ready(&ReadSteerableViewMapPixelF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&ReadSteerableViewMapPixelF0D_Type);
  PyModule_AddObject(
      module, "ReadSteerableViewMapPixelF0D", (PyObject *)&ReadSteerableViewMapPixelF0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0DFloat___doc__[] =
    "Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DFloat`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface0DIterator` and return a float value.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n";

static int UnaryFunction0DFloat___init__(BPy_UnaryFunction0DFloat *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->uf0D_float = new UnaryFunction0D<float>();
  self->uf0D_float->py_uf0D = (PyObject *)self;
  return 0;
}

static void UnaryFunction0DFloat___dealloc__(BPy_UnaryFunction0DFloat *self)
{
  if (self->uf0D_float) {
    delete self->uf0D_float;
  }
  UnaryFunction0D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction0DFloat___repr__(BPy_UnaryFunction0DFloat *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf0D_float);
}

static PyObject *UnaryFunction0DFloat___call__(BPy_UnaryFunction0DFloat *self,
                                               PyObject *args,
                                               PyObject *kwds)
{
  static const char *kwlist[] = {"it", NULL};
  PyObject *obj;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &Interface0DIterator_Type, &obj)) {
    return NULL;
  }

  if (typeid(*(self->uf0D_float)) == typeid(UnaryFunction0D<float>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return NULL;
  }
  if (self->uf0D_float->operator()(*(((BPy_Interface0DIterator *)obj)->if0D_it)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return NULL;
  }
  return PyFloat_FromDouble(self->uf0D_float->result);
}

/*-----------------------BPy_UnaryFunction0DFloat type definition ------------------------------*/

PyTypeObject UnaryFunction0DFloat_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "UnaryFunction0DFloat", /* tp_name */
    sizeof(BPy_UnaryFunction0DFloat),                      /* tp_basicsize */
    0,                                                     /* tp_itemsize */
    (destructor)UnaryFunction0DFloat___dealloc__,          /* tp_dealloc */
    0,                                                     /* tp_print */
    0,                                                     /* tp_getattr */
    0,                                                     /* tp_setattr */
    0,                                                     /* tp_reserved */
    (reprfunc)UnaryFunction0DFloat___repr__,               /* tp_repr */
    0,                                                     /* tp_as_number */
    0,                                                     /* tp_as_sequence */
    0,                                                     /* tp_as_mapping */
    0,                                                     /* tp_hash  */
    (ternaryfunc)UnaryFunction0DFloat___call__,            /* tp_call */
    0,                                                     /* tp_str */
    0,                                                     /* tp_getattro */
    0,                                                     /* tp_setattro */
    0,                                                     /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,              /* tp_flags */
    UnaryFunction0DFloat___doc__,                          /* tp_doc */
    0,                                                     /* tp_traverse */
    0,                                                     /* tp_clear */
    0,                                                     /* tp_richcompare */
    0,                                                     /* tp_weaklistoffset */
    0,                                                     /* tp_iter */
    0,                                                     /* tp_iternext */
    0,                                                     /* tp_methods */
    0,                                                     /* tp_members */
    0,                                                     /* tp_getset */
    &UnaryFunction0D_Type,                                 /* tp_base */
    0,                                                     /* tp_dict */
    0,                                                     /* tp_descr_get */
    0,                                                     /* tp_descr_set */
    0,                                                     /* tp_dictoffset */
    (initproc)UnaryFunction0DFloat___init__,               /* tp_init */
    0,                                                     /* tp_alloc */
    0,                                                     /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
