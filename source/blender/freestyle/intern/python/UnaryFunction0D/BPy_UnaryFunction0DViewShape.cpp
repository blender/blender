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

#include "BPy_UnaryFunction0DViewShape.h"

#include "../BPy_Convert.h"
#include "../Iterator/BPy_Interface0DIterator.h"

#include "UnaryFunction0D_ViewShape/BPy_GetOccludeeF0D.h"
#include "UnaryFunction0D_ViewShape/BPy_GetShapeF0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction0DViewShape_Init(PyObject *module)
{
  if (module == NULL)
    return -1;

  if (PyType_Ready(&UnaryFunction0DViewShape_Type) < 0)
    return -1;
  Py_INCREF(&UnaryFunction0DViewShape_Type);
  PyModule_AddObject(
      module, "UnaryFunction0DViewShape", (PyObject *)&UnaryFunction0DViewShape_Type);

  if (PyType_Ready(&GetOccludeeF0D_Type) < 0)
    return -1;
  Py_INCREF(&GetOccludeeF0D_Type);
  PyModule_AddObject(module, "GetOccludeeF0D", (PyObject *)&GetOccludeeF0D_Type);

  if (PyType_Ready(&GetShapeF0D_Type) < 0)
    return -1;
  Py_INCREF(&GetShapeF0D_Type);
  PyModule_AddObject(module, "GetShapeF0D", (PyObject *)&GetShapeF0D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0DViewShape___doc__[] =
    "Class hierarchy: :class:`UnaryFunction0D` > :class:`UnaryFunction0DViewShape`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface0DIterator` and return a :class:`ViewShape` object.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n";

static int UnaryFunction0DViewShape___init__(BPy_UnaryFunction0DViewShape *self,
                                             PyObject *args,
                                             PyObject *kwds)
{
  static const char *kwlist[] = {NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist))
    return -1;
  self->uf0D_viewshape = new UnaryFunction0D<ViewShape *>();
  self->uf0D_viewshape->py_uf0D = (PyObject *)self;
  return 0;
}

static void UnaryFunction0DViewShape___dealloc__(BPy_UnaryFunction0DViewShape *self)
{
  if (self->uf0D_viewshape)
    delete self->uf0D_viewshape;
  UnaryFunction0D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction0DViewShape___repr__(BPy_UnaryFunction0DViewShape *self)
{
  return PyUnicode_FromFormat(
      "type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf0D_viewshape);
}

static PyObject *UnaryFunction0DViewShape___call__(BPy_UnaryFunction0DViewShape *self,
                                                   PyObject *args,
                                                   PyObject *kwds)
{
  static const char *kwlist[] = {"it", NULL};
  PyObject *obj;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "O!", (char **)kwlist, &Interface0DIterator_Type, &obj))
    return NULL;

  if (typeid(*(self->uf0D_viewshape)) == typeid(UnaryFunction0D<ViewShape *>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return NULL;
  }
  if (self->uf0D_viewshape->operator()(*(((BPy_Interface0DIterator *)obj)->if0D_it)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return NULL;
  }
  return BPy_ViewShape_from_ViewShape(*(self->uf0D_viewshape->result));
}

/*-----------------------BPy_UnaryFunction0DViewShape type definition ---------------------------*/

PyTypeObject UnaryFunction0DViewShape_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "UnaryFunction0DViewShape", /* tp_name */
    sizeof(BPy_UnaryFunction0DViewShape),                      /* tp_basicsize */
    0,                                                         /* tp_itemsize */
    (destructor)UnaryFunction0DViewShape___dealloc__,          /* tp_dealloc */
    0,                                                         /* tp_print */
    0,                                                         /* tp_getattr */
    0,                                                         /* tp_setattr */
    0,                                                         /* tp_reserved */
    (reprfunc)UnaryFunction0DViewShape___repr__,               /* tp_repr */
    0,                                                         /* tp_as_number */
    0,                                                         /* tp_as_sequence */
    0,                                                         /* tp_as_mapping */
    0,                                                         /* tp_hash  */
    (ternaryfunc)UnaryFunction0DViewShape___call__,            /* tp_call */
    0,                                                         /* tp_str */
    0,                                                         /* tp_getattro */
    0,                                                         /* tp_setattro */
    0,                                                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,                  /* tp_flags */
    UnaryFunction0DViewShape___doc__,                          /* tp_doc */
    0,                                                         /* tp_traverse */
    0,                                                         /* tp_clear */
    0,                                                         /* tp_richcompare */
    0,                                                         /* tp_weaklistoffset */
    0,                                                         /* tp_iter */
    0,                                                         /* tp_iternext */
    0,                                                         /* tp_methods */
    0,                                                         /* tp_members */
    0,                                                         /* tp_getset */
    &UnaryFunction0D_Type,                                     /* tp_base */
    0,                                                         /* tp_dict */
    0,                                                         /* tp_descr_get */
    0,                                                         /* tp_descr_set */
    0,                                                         /* tp_dictoffset */
    (initproc)UnaryFunction0DViewShape___init__,               /* tp_init */
    0,                                                         /* tp_alloc */
    0,                                                         /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
