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

#include "BPy_BBox.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int BBox_Init(PyObject *module)
{
  if (module == NULL) {
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
  static const char *kwlist[] = {NULL};

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
    PyVarObject_HEAD_INIT(NULL, 0) "BBox",    /* tp_name */
    sizeof(BPy_BBox),                         /* tp_basicsize */
    0,                                        /* tp_itemsize */
    (destructor)BBox_dealloc,                 /* tp_dealloc */
    0,                                        /* tp_print */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_reserved */
    (reprfunc)BBox_repr,                      /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash  */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    BBox_doc,                                 /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    0,                                        /* tp_methods */
    0,                                        /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)BBox_init,                      /* tp_init */
    0,                                        /* tp_alloc */
    PyType_GenericNew,                        /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
