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

#include "BPy_UnaryFunction1DFloat.h"

#include "../BPy_Convert.h"
#include "../BPy_Interface1D.h"
#include "../BPy_IntegrationType.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction1DFloat_Init(PyObject *module)
{
  if (module == NULL) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction1DFloat_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction1DFloat_Type);
  PyModule_AddObject(module, "UnaryFunction1DFloat", (PyObject *)&UnaryFunction1DFloat_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction1DFloat___doc__[] =
    "Class hierarchy: :class:`UnaryFunction1D` > :class:`UnaryFunction1DFloat`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface1D` and return a float value.\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Default constructor.\n"
    "\n"
    ".. method:: __init__(integration_type)\n"
    "\n"
    "   Builds a unary 1D function using the integration method given as\n"
    "   argument.\n"
    "\n"
    "   :arg integration_type: An integration method.\n"
    "   :type integration_type: :class:`IntegrationType`\n";

static int UnaryFunction1DFloat___init__(BPy_UnaryFunction1DFloat *self,
                                         PyObject *args,
                                         PyObject *kwds)
{
  static const char *kwlist[] = {"integration", NULL};
  PyObject *obj = 0;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist, &IntegrationType_Type, &obj)) {
    return -1;
  }

  if (!obj) {
    self->uf1D_float = new UnaryFunction1D<float>();
  }
  else {
    self->uf1D_float = new UnaryFunction1D<float>(IntegrationType_from_BPy_IntegrationType(obj));
  }

  self->uf1D_float->py_uf1D = (PyObject *)self;

  return 0;
}

static void UnaryFunction1DFloat___dealloc__(BPy_UnaryFunction1DFloat *self)
{
  if (self->uf1D_float) {
    delete self->uf1D_float;
  }
  UnaryFunction1D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction1DFloat___repr__(BPy_UnaryFunction1DFloat *self)
{
  return PyUnicode_FromFormat("type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf1D_float);
}

static PyObject *UnaryFunction1DFloat___call__(BPy_UnaryFunction1DFloat *self,
                                               PyObject *args,
                                               PyObject *kwds)
{
  static const char *kwlist[] = {"inter", NULL};
  PyObject *obj = 0;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &Interface1D_Type, &obj)) {
    return NULL;
  }

  if (typeid(*(self->uf1D_float)) == typeid(UnaryFunction1D<float>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return NULL;
  }
  if (self->uf1D_float->operator()(*(((BPy_Interface1D *)obj)->if1D)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return NULL;
  }
  return PyFloat_FromDouble(self->uf1D_float->result);
}

/*----------------------UnaryFunction1DFloat get/setters ----------------------------*/

PyDoc_STRVAR(integration_type_doc,
             "The integration method.\n"
             "\n"
             ":type: :class:`IntegrationType`");

static PyObject *integration_type_get(BPy_UnaryFunction1DFloat *self, void *UNUSED(closure))
{
  return BPy_IntegrationType_from_IntegrationType(self->uf1D_float->getIntegrationType());
}

static int integration_type_set(BPy_UnaryFunction1DFloat *self,
                                PyObject *value,
                                void *UNUSED(closure))
{
  if (!BPy_IntegrationType_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an IntegrationType");
    return -1;
  }
  self->uf1D_float->setIntegrationType(IntegrationType_from_BPy_IntegrationType(value));
  return 0;
}

static PyGetSetDef BPy_UnaryFunction1DFloat_getseters[] = {
    {(char *)"integration_type",
     (getter)integration_type_get,
     (setter)integration_type_set,
     (char *)integration_type_doc,
     NULL},
    {NULL, NULL, NULL, NULL, NULL} /* Sentinel */
};

/*-----------------------BPy_UnaryFunction1DFloat type definition ------------------------------*/

PyTypeObject UnaryFunction1DFloat_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "UnaryFunction1DFloat", /* tp_name */
    sizeof(BPy_UnaryFunction1DFloat),                      /* tp_basicsize */
    0,                                                     /* tp_itemsize */
    (destructor)UnaryFunction1DFloat___dealloc__,          /* tp_dealloc */
    0,                                                     /* tp_print */
    0,                                                     /* tp_getattr */
    0,                                                     /* tp_setattr */
    0,                                                     /* tp_reserved */
    (reprfunc)UnaryFunction1DFloat___repr__,               /* tp_repr */
    0,                                                     /* tp_as_number */
    0,                                                     /* tp_as_sequence */
    0,                                                     /* tp_as_mapping */
    0,                                                     /* tp_hash  */
    (ternaryfunc)UnaryFunction1DFloat___call__,            /* tp_call */
    0,                                                     /* tp_str */
    0,                                                     /* tp_getattro */
    0,                                                     /* tp_setattro */
    0,                                                     /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,              /* tp_flags */
    UnaryFunction1DFloat___doc__,                          /* tp_doc */
    0,                                                     /* tp_traverse */
    0,                                                     /* tp_clear */
    0,                                                     /* tp_richcompare */
    0,                                                     /* tp_weaklistoffset */
    0,                                                     /* tp_iter */
    0,                                                     /* tp_iternext */
    0,                                                     /* tp_methods */
    0,                                                     /* tp_members */
    BPy_UnaryFunction1DFloat_getseters,                    /* tp_getset */
    &UnaryFunction1D_Type,                                 /* tp_base */
    0,                                                     /* tp_dict */
    0,                                                     /* tp_descr_get */
    0,                                                     /* tp_descr_set */
    0,                                                     /* tp_dictoffset */
    (initproc)UnaryFunction1DFloat___init__,               /* tp_init */
    0,                                                     /* tp_alloc */
    0,                                                     /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
