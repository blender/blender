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

#include "BPy_UnaryFunction0DUnsigned.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_unsigned_int/BPy_QuantitativeInvisibilityF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DUnsigned_Init(PyObject *module)
{
  if (module == NULL) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction0DUnsigned_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction0DUnsigned_Type);
  PyModule_AddObject(module, "UnaryFunction0DUnsigned", (PyObject *)&UnaryFunction0DUnsigned_Type);

  if (PyType_Ready(&QuantitativeInvisibilityF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&QuantitativeInvisibilityF0D_Type);
  PyModule_AddObject(
      module, "QuantitativeInvisibilityF0D", (PyObject *)&QuantitativeInvisibilityF0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0DUnsigned___doc__[] =
    "Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DUnsigned`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface0DIterator` and return an int value.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n";

static int UnaryFunction0DUnsigned___init__(BPy_UnaryFunction0DUnsigned *self,
                                            PyObject *args,
                                            PyObject *kwds)
{
  static const char *kwlist[] = {NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->uf0D_unsigned = new UnaryFunction0D<unsigned int>();
  self->uf0D_unsigned->py_uf0D = (PyObject *)self;
  return 0;
}

static void UnaryFunction0DUnsigned___dealloc__(BPy_UnaryFunction0DUnsigned *self)
{
  if (self->uf0D_unsigned) {
    delete self->uf0D_unsigned;
  }
  UnaryFunction0D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction0DUnsigned___repr__(BPy_UnaryFunction0DUnsigned *self)
{
  return PyUnicode_FromFormat(
      "type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf0D_unsigned);
}

static PyObject *UnaryFunction0DUnsigned___call__(BPy_UnaryFunction0DUnsigned *self,
                                                  PyObject *args,
                                                  PyObject *kwds)
{
  static const char *kwlist[] = {"it", NULL};
  PyObject *obj;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &Interface0DIterator_Type, &obj)) {
    return NULL;
  }

  if (typeid(*(self->uf0D_unsigned)) == typeid(UnaryFunction0D<unsigned int>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return NULL;
  }
  if (self->uf0D_unsigned->operator()(*(((BPy_Interface0DIterator *)obj)->if0D_it)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return NULL;
  }
  return PyLong_FromLong(self->uf0D_unsigned->result);
}

/*-----------------------BPy_UnaryFunction0DUnsigned type definition ----------------------------*/

PyTypeObject UnaryFunction0DUnsigned_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "UnaryFunction0DUnsigned", /* tp_name */
    sizeof(BPy_UnaryFunction0DUnsigned),                      /* tp_basicsize */
    0,                                                        /* tp_itemsize */
    (destructor)UnaryFunction0DUnsigned___dealloc__,          /* tp_dealloc */
    0,                                                        /* tp_print */
    0,                                                        /* tp_getattr */
    0,                                                        /* tp_setattr */
    0,                                                        /* tp_reserved */
    (reprfunc)UnaryFunction0DUnsigned___repr__,               /* tp_repr */
    0,                                                        /* tp_as_number */
    0,                                                        /* tp_as_sequence */
    0,                                                        /* tp_as_mapping */
    0,                                                        /* tp_hash  */
    (ternaryfunc)UnaryFunction0DUnsigned___call__,            /* tp_call */
    0,                                                        /* tp_str */
    0,                                                        /* tp_getattro */
    0,                                                        /* tp_setattro */
    0,                                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                 /* tp_flags */
    UnaryFunction0DUnsigned___doc__,                          /* tp_doc */
    0,                                                        /* tp_traverse */
    0,                                                        /* tp_clear */
    0,                                                        /* tp_richcompare */
    0,                                                        /* tp_weaklistoffset */
    0,                                                        /* tp_iter */
    0,                                                        /* tp_iternext */
    0,                                                        /* tp_methods */
    0,                                                        /* tp_members */
    0,                                                        /* tp_getset */
    &UnaryFunction0D_Type,                                    /* tp_base */
    0,                                                        /* tp_dict */
    0,                                                        /* tp_descr_get */
    0,                                                        /* tp_descr_set */
    0,                                                        /* tp_dictoffset */
    (initproc)UnaryFunction0DUnsigned___init__,               /* tp_init */
    0,                                                        /* tp_alloc */
    0,                                                        /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
