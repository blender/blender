/* SPDX-FileCopyrightText: 2004-2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_Id.h"

#include "BPy_Convert.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int Id_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&Id_Type) < 0) {
    return -1;
  }

  Py_INCREF(&Id_Type);
  PyModule_AddObject(module, "Id", (PyObject *)&Id_Type);
  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(
    Id_doc,
    "Class for representing an object Id.\n"
    "\n"
    ".. method:: __init__(brother)\n"
    "            __init__(first=0, second=0)\n"
    "\n"
    "   Build the Id from two numbers or another :class:`Id` using the copy constructor.\n"
    "\n"
    "   :arg brother: An Id object.\n"
    "   :type brother: :class:`Id`"
    "   :arg first: The first number.\n"
    "   :type first: int\n"
    "   :arg second: The second number.\n"
    "   :type second: int\n");

static int Id_init(BPy_Id *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist_1[] = {"brother", nullptr};
  static const char *kwlist_2[] = {"first", "second", nullptr};
  PyObject *brother;
  int first = 0, second = 0;

  if (PyArg_ParseTupleAndKeywords(args, kwds, "O!", (char **)kwlist_1, &Id_Type, &brother)) {
    self->id = new Id(*(((BPy_Id *)brother)->id));
  }
  else if ((void)PyErr_Clear(),
           PyArg_ParseTupleAndKeywords(args, kwds, "|ii", (char **)kwlist_2, &first, &second))
  {
    self->id = new Id(first, second);
  }
  else {
    PyErr_SetString(PyExc_TypeError, "invalid argument(s)");
    return -1;
  }
  return 0;
}

static void Id_dealloc(BPy_Id *self)
{
  delete self->id;
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Id_repr(BPy_Id *self)
{
  return PyUnicode_FromFormat(
      "[ first: %i, second: %i ](BPy_Id)", self->id->getFirst(), self->id->getSecond());
}

static PyObject *Id_RichCompare(BPy_Id *o1, BPy_Id *o2, int opid)
{
  switch (opid) {
    case Py_LT:
      return PyBool_from_bool(o1->id->operator<(*(o2->id)));
    case Py_LE:
      return PyBool_from_bool(o1->id->operator<(*(o2->id)) || o1->id->operator==(*(o2->id)));
    case Py_EQ:
      return PyBool_from_bool(o1->id->operator==(*(o2->id)));
    case Py_NE:
      return PyBool_from_bool(o1->id->operator!=(*(o2->id)));
    case Py_GT:
      return PyBool_from_bool(!(o1->id->operator<(*(o2->id)) || o1->id->operator==(*(o2->id))));
    case Py_GE:
      return PyBool_from_bool(!o1->id->operator<(*(o2->id)));
  }
  Py_RETURN_NONE;
}

/*----------------------Id get/setters ----------------------------*/

PyDoc_STRVAR(Id_first_doc,
             "The first number constituting the Id.\n"
             "\n"
             ":type: int");

static PyObject *Id_first_get(BPy_Id *self, void * /*closure*/)
{
  return PyLong_FromLong(self->id->getFirst());
}

static int Id_first_set(BPy_Id *self, PyObject *value, void * /*closure*/)
{
  int scalar;
  if ((scalar = PyLong_AsLong(value)) == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "value must be an integer");
    return -1;
  }
  self->id->setFirst(scalar);
  return 0;
}

PyDoc_STRVAR(Id_second_doc,
             "The second number constituting the Id.\n"
             "\n"
             ":type: int");

static PyObject *Id_second_get(BPy_Id *self, void * /*closure*/)
{
  return PyLong_FromLong(self->id->getSecond());
}

static int Id_second_set(BPy_Id *self, PyObject *value, void * /*closure*/)
{
  int scalar;
  if ((scalar = PyLong_AsLong(value)) == -1 && PyErr_Occurred()) {
    PyErr_SetString(PyExc_TypeError, "value must be an integer");
    return -1;
  }
  self->id->setSecond(scalar);
  return 0;
}

static PyGetSetDef BPy_Id_getseters[] = {
    {"first", (getter)Id_first_get, (setter)Id_first_set, Id_first_doc, nullptr},
    {"second", (getter)Id_second_get, (setter)Id_second_set, Id_second_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_Id type definition ------------------------------*/

PyTypeObject Id_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Id",
    /*tp_basicsize*/ sizeof(BPy_Id),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)Id_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)Id_repr,
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
    /*tp_doc*/ Id_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ (richcmpfunc)Id_RichCompare,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ BPy_Id_getseters,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)Id_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
