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

#include "BPy_UnaryFunction1DEdgeNature.h"

#include "../BPy_Convert.h"
#include "../BPy_IntegrationType.h"
#include "../BPy_Interface1D.h"

#include "UnaryFunction1D_Nature_EdgeNature/BPy_CurveNatureF1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------

int UnaryFunction1DEdgeNature_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction1DEdgeNature_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction1DEdgeNature_Type);
  PyModule_AddObject(
      module, "UnaryFunction1DEdgeNature", (PyObject *)&UnaryFunction1DEdgeNature_Type);

  if (PyType_Ready(&CurveNatureF1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&CurveNatureF1D_Type);
  PyModule_AddObject(module, "CurveNatureF1D", (PyObject *)&CurveNatureF1D_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction1DEdgeNature___doc__[] =
    "Class hierarchy: :class:`UnaryFunction1D` > :class:`UnaryFunction1DEdgeNature`\n"
    "\n"
    "Base class for unary functions (functors) that work on\n"
    ":class:`Interface1D` and return a :class:`Nature` object.\n"
    "\n"
    ".. method:: __init__()\n"
    "            __init__(integration_type)\n"
    "\n"
    "   Builds a unary 1D function using the default constructor\n"
    "   or the integration method given as an argument.\n"
    "\n"
    "   :arg integration_type: An integration method.\n"
    "   :type integration_type: :class:`IntegrationType`\n";

static int UnaryFunction1DEdgeNature___init__(BPy_UnaryFunction1DEdgeNature *self,
                                              PyObject *args,
                                              PyObject *kwds)
{
  static const char *kwlist[] = {"integration", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|O!", (char **)kwlist, &IntegrationType_Type, &obj)) {
    return -1;
  }

  if (!obj) {
    self->uf1D_edgenature = new UnaryFunction1D<Nature::EdgeNature>();
  }
  else {
    self->uf1D_edgenature = new UnaryFunction1D<Nature::EdgeNature>(
        IntegrationType_from_BPy_IntegrationType(obj));
  }

  self->uf1D_edgenature->py_uf1D = (PyObject *)self;

  return 0;
}

static void UnaryFunction1DEdgeNature___dealloc__(BPy_UnaryFunction1DEdgeNature *self)
{
  delete self->uf1D_edgenature;
  UnaryFunction1D_Type.tp_dealloc((PyObject *)self);
}

static PyObject *UnaryFunction1DEdgeNature___repr__(BPy_UnaryFunction1DEdgeNature *self)
{
  return PyUnicode_FromFormat(
      "type: %s - address: %p", Py_TYPE(self)->tp_name, self->uf1D_edgenature);
}

static PyObject *UnaryFunction1DEdgeNature___call__(BPy_UnaryFunction1DEdgeNature *self,
                                                    PyObject *args,
                                                    PyObject *kwds)
{
  static const char *kwlist[] = {"inter", nullptr};
  PyObject *obj = nullptr;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist, &Interface1D_Type, &obj)) {
    return nullptr;
  }

  if (typeid(*(self->uf1D_edgenature)) == typeid(UnaryFunction1D<Nature::EdgeNature>)) {
    PyErr_SetString(PyExc_TypeError, "__call__ method not properly overridden");
    return nullptr;
  }
  if (self->uf1D_edgenature->operator()(*(((BPy_Interface1D *)obj)->if1D)) < 0) {
    if (!PyErr_Occurred()) {
      string class_name(Py_TYPE(self)->tp_name);
      PyErr_SetString(PyExc_RuntimeError, (class_name + " __call__ method failed").c_str());
    }
    return nullptr;
  }
  return BPy_Nature_from_Nature(self->uf1D_edgenature->result);
}

/*----------------------UnaryFunction1DEdgeNature get/setters ----------------------------*/

PyDoc_STRVAR(integration_type_doc,
             "The integration method.\n"
             "\n"
             ":type: :class:`IntegrationType`");

static PyObject *integration_type_get(BPy_UnaryFunction1DEdgeNature *self, void *UNUSED(closure))
{
  return BPy_IntegrationType_from_IntegrationType(self->uf1D_edgenature->getIntegrationType());
}

static int integration_type_set(BPy_UnaryFunction1DEdgeNature *self,
                                PyObject *value,
                                void *UNUSED(closure))
{
  if (!BPy_IntegrationType_Check(value)) {
    PyErr_SetString(PyExc_TypeError, "value must be an IntegrationType");
    return -1;
  }
  self->uf1D_edgenature->setIntegrationType(IntegrationType_from_BPy_IntegrationType(value));
  return 0;
}

static PyGetSetDef BPy_UnaryFunction1DEdgeNature_getseters[] = {
    {"integration_type",
     (getter)integration_type_get,
     (setter)integration_type_set,
     integration_type_doc,
     nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_UnaryFunction1DEdgeNature type definition --------------------------*/

PyTypeObject UnaryFunction1DEdgeNature_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "UnaryFunction1DEdgeNature", /* tp_name */
    sizeof(BPy_UnaryFunction1DEdgeNature),                         /* tp_basicsize */
    0,                                                             /* tp_itemsize */
    (destructor)UnaryFunction1DEdgeNature___dealloc__,             /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                         /* tp_getattr */
    nullptr,                                         /* tp_setattr */
    nullptr,                                         /* tp_reserved */
    (reprfunc)UnaryFunction1DEdgeNature___repr__,    /* tp_repr */
    nullptr,                                         /* tp_as_number */
    nullptr,                                         /* tp_as_sequence */
    nullptr,                                         /* tp_as_mapping */
    nullptr,                                         /* tp_hash  */
    (ternaryfunc)UnaryFunction1DEdgeNature___call__, /* tp_call */
    nullptr,                                         /* tp_str */
    nullptr,                                         /* tp_getattro */
    nullptr,                                         /* tp_setattro */
    nullptr,                                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /* tp_flags */
    UnaryFunction1DEdgeNature___doc__,               /* tp_doc */
    nullptr,                                         /* tp_traverse */
    nullptr,                                         /* tp_clear */
    nullptr,                                         /* tp_richcompare */
    0,                                               /* tp_weaklistoffset */
    nullptr,                                         /* tp_iter */
    nullptr,                                         /* tp_iternext */
    nullptr,                                         /* tp_methods */
    nullptr,                                         /* tp_members */
    BPy_UnaryFunction1DEdgeNature_getseters,         /* tp_getset */
    &UnaryFunction1D_Type,                           /* tp_base */
    nullptr,                                         /* tp_dict */
    nullptr,                                         /* tp_descr_get */
    nullptr,                                         /* tp_descr_set */
    0,                                               /* tp_dictoffset */
    (initproc)UnaryFunction1DEdgeNature___init__,    /* tp_init */
    nullptr,                                         /* tp_alloc */
    nullptr,                                         /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
