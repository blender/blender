/* SPDX-FileCopyrightText: 2004-2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 */

#include "BPy_BBox.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;
using namespace Freestyle::Geometry;

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int BBox_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&BBox_Type) < 0) {
    return -1;
  }
  Py_INCREF(&BBox_Type);
  PyModule_AddObject(module, "BBox", (PyObject *)&BBox_Type);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

PyDoc_STRVAR(BBox_doc,
             "Class for representing a bounding box.\n"
             "\n"
             ".. method:: __init__()\n"
             "\n"
             "   Default constructor.");

static int BBox_init(BPy_BBox *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->bb = new BBox<Vec3r>();
  return 0;
}

static void BBox_dealloc(BPy_BBox *self)
{
  delete self->bb;
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *BBox_repr(BPy_BBox *self)
{
  return PyUnicode_FromFormat("BBox - address: %p", self->bb);
}

/*-----------------------BPy_BBox type definition ------------------------------*/

PyTypeObject BBox_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "BBox",
    /*tp_basicsize*/ sizeof(BPy_BBox),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)BBox_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ (reprfunc)BBox_repr,
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
    /*tp_doc*/ BBox_doc,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ nullptr,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ (initproc)BBox_init,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ PyType_GenericNew,
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
