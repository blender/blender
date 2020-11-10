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

#include "BPy_UnaryFunction0DVectorViewShape.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_vector_ViewShape/BPy_GetOccludersF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DVectorViewShape_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction0DVectorViewShape_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction0DVectorViewShape_Type);
  PyModule_AddObject(
      module, "UnaryFunction0DVectorViewShape", (PyObject *)&UnaryFunction0DVectorViewShape_Type);

  if (PyType_Ready(&GetOccludersF0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&GetOccludersF0D_Type);
  PyModule_AddObject(module, "GetOccludersF0D", (PyObject *)&GetOccludersF0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0DVectorViewShape___doc__[] =
    "Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DVectorViewShape`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface0DIterator` and return a list of :class:`ViewShape`\n"
    "objects.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n";

static int UnaryFunction0DVectorViewShape___init__(BPy_UnaryFunction0DVectorViewShape *self,
                                                   PyObject *args,
                                                   PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->uf0D_vectorviewshape = new UnaryFunction0D<std::vector<ViewShape *>>();
  self->uf0D_vectorviewshape->py_uf0D = (PyObject *)self;
  return 0;
}

static void UnaryFunction0DVectorViewShape___dealloc__(BPy_UnaryFunction0DVectorViewShape *self)
{
  delete self->uf0D_vectorviewshape;
  UnaryFunction0D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction0DVectorViewShape___repr__(BPy_UnaryFunction0DVectorViewShape *self)
{
  return PyUnicode_FromFormat(
      "type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf0D_vectorviewshape);
}

static PyObject *UnaryFunction0DVectorViewShape___call__(BPy_UnaryFunction0DVectorViewShape *self,
                                                         PyObject *args,
                                                         PyObject *kwds)
{
  static const char *kwlist[] = {"it", nullptr};
  PyObject *obj;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &Interface0DIterator_Type, &obj)) {
    return nullptr;
  }

  if (typeid(*(self->uf0D_vectorviewshape)) == typeid(UnaryFunction0D<std::vector<ViewShape *>>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->uf0D_vectorviewshape->operator()(*(((BPy_Interface0DIterator *)obj)->if0D_it)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }

  const unsigned int list_len = self->uf0D_vectorviewshape->result.size();
  PyObject *list = PyList_New(list_len);
  for (unsigned int i = 0; i < list_len; i++) {
    ViewShape *v = self->uf0D_vectorviewshape->result[i];
    PyList_SET_ITEM(list, i, v ? BPy_ViewShape_from_ViewShape(*v) : (Py_INCREF(Py_None), Py_None));
  }

  return list;
}

/*-----------------------BPy_UnaryFunction0DVectorViewShape type definition ---------------------*/

PyTypeObject UnaryFunction0DVectorViewShape_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "UnaryFunction0DVectorViewShape", /* tp_name */
    sizeof(BPy_UnaryFunction0DVectorViewShape),                         /* tp_basicsize */
    0,                                                                  /* tp_itemsize */
    (destructor)UnaryFunction0DVectorViewShape___dealloc__,             /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                              /* tp_getattr */
    nullptr,                                              /* tp_setattr */
    nullptr,                                              /* tp_reserved */
    (reprfunc)UnaryFunction0DVectorViewShape___repr__,    /* tp_repr */
    nullptr,                                              /* tp_as_number */
    nullptr,                                              /* tp_as_sequence */
    nullptr,                                              /* tp_as_mapping */
    nullptr,                                              /* tp_hash  */
    (ternaryfunc)UnaryFunction0DVectorViewShape___call__, /* tp_call */
    nullptr,                                              /* tp_str */
    nullptr,                                              /* tp_getattro */
    nullptr,                                              /* tp_setattro */
    nullptr,                                              /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,             /* tp_flags */
    UnaryFunction0DVectorViewShape___doc__,               /* tp_doc */
    nullptr,                                              /* tp_traverse */
    nullptr,                                              /* tp_clear */
    nullptr,                                              /* tp_richcompare */
    0,                                                    /* tp_weaklistoffset */
    nullptr,                                              /* tp_iter */
    nullptr,                                              /* tp_iternext */
    nullptr,                                              /* tp_methods */
    nullptr,                                              /* tp_members */
    nullptr,                                              /* tp_getset */
    &UnaryFunction0D_Type,                                /* tp_base */
    nullptr,                                              /* tp_dict */
    nullptr,                                              /* tp_descr_get */
    nullptr,                                              /* tp_descr_set */
    0,                                                    /* tp_dictoffset */
    (initproc)UnaryFunction0DVectorViewShape___init__,    /* tp_init */
    nullptr,                                              /* tp_alloc */
    nullptr,                                              /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
