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

#include "BPy_UnaryFunction0D.h"

#include "UnaryFunction0D/BPy_UnaryFunction0DDouble.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DEdgeNature.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DFloat.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DId.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DMaterial.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DUnsigned.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVec2f.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVec3f.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DVectorViewShape.h"
#include "UnaryFunction0D/BPy_UnaryFunction0DViewShape.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int UnaryFunction0D_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction0D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction0D_Type);
  PyModule_AddObject(module, "UnaryFunction0D", (PyObject *)&UnaryFunction0D_Type);

  UnaryFunction0DDouble_Init(module);
  UnaryFunction0DEdgeNature_Init(module);
  UnaryFunction0DFloat_Init(module);
  UnaryFunction0DId_Init(module);
  UnaryFunction0DMaterial_Init(module);
  UnaryFunction0DUnsigned_Init(module);
  UnaryFunction0DVec2f_Init(module);
  UnaryFunction0DVec3f_Init(module);
  UnaryFunction0DVectorViewShape_Init(module);
  UnaryFunction0DViewShape_Init(module);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction0D___doc__[] =
    "Base class for Unary Functions (functors) working on\n"
    ":class:`Interface0DIterator`.  A unary function will be used by\n"
    "invoking __call__() on an Interface0DIterator.  In Python, several\n"
    "different subclasses of UnaryFunction0D are used depending on the\n"
    "types of functors' return values.  For example, you would inherit from\n"
    "a :class:`UnaryFunction0DDouble` if you wish to define a function that\n"
    "returns a double value.  Available UnaryFunction0D subclasses are:\n"
    "\n"
    "* :class:`UnaryFunction0DDouble`\n"
    "* :class:`UnaryFunction0DEdgeNature`\n"
    "* :class:`UnaryFunction0DFloat`\n"
    "* :class:`UnaryFunction0DId`\n"
    "* :class:`UnaryFunction0DMaterial`\n"
    "* :class:`UnaryFunction0DUnsigned`\n"
    "* :class:`UnaryFunction0DVec2f`\n"
    "* :class:`UnaryFunction0DVec3f`\n"
    "* :class:`UnaryFunction0DVectorViewShape`\n"
    "* :class:`UnaryFunction0DViewShape`\n";

static void UnaryFunction0D___dealloc__(BPy_UnaryFunction0D *self)
{
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *UnaryFunction0D___repr__(BPy_UnaryFunction0D * /*self*/)
{
  return PyUnicode_FromString("UnaryFunction0D");
}

/*----------------------UnaryFunction0D get/setters ----------------------------*/

PyDoc_STRVAR(UnaryFunction0D_name_doc,
             "The name of the unary 0D function.\n"
             "\n"
             ":type: str");

static PyObject *UnaryFunction0D_name_get(BPy_UnaryFunction0D *self, void *UNUSED(closure))
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

static PyGetSetDef BPy_UnaryFunction0D_getseters[] = {
    {"name", (getter)UnaryFunction0D_name_get, (setter)nullptr, UnaryFunction0D_name_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_UnaryFunction0D type definition ------------------------------*/

PyTypeObject UnaryFunction0D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "UnaryFunction0D", /* tp_name */
    sizeof(BPy_UnaryFunction0D),                      /* tp_basicsize */
    0,                                                /* tp_itemsize */
    (destructor)UnaryFunction0D___dealloc__,          /* tp_dealloc */
    nullptr,                                                /* tp_print */
    nullptr,                                                /* tp_getattr */
    nullptr,                                                /* tp_setattr */
    nullptr,                                                /* tp_reserved */
    (reprfunc)UnaryFunction0D___repr__,               /* tp_repr */
    nullptr,                                                /* tp_as_number */
    nullptr,                                                /* tp_as_sequence */
    nullptr,                                                /* tp_as_mapping */
    nullptr,                                                /* tp_hash  */
    nullptr,                                                /* tp_call */
    nullptr,                                                /* tp_str */
    nullptr,                                                /* tp_getattro */
    nullptr,                                                /* tp_setattro */
    nullptr,                                                /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* tp_flags */
    UnaryFunction0D___doc__,                          /* tp_doc */
    nullptr,                                                /* tp_traverse */
    nullptr,                                                /* tp_clear */
    nullptr,                                                /* tp_richcompare */
    0,                                                /* tp_weaklistoffset */
    nullptr,                                                /* tp_iter */
    nullptr,                                                /* tp_iternext */
    nullptr,                                                /* tp_methods */
    nullptr,                                                /* tp_members */
    BPy_UnaryFunction0D_getseters,                    /* tp_getset */
    nullptr,                                                /* tp_base */
    nullptr,                                                /* tp_dict */
    nullptr,                                                /* tp_descr_get */
    nullptr,                                                /* tp_descr_set */
    0,                                                /* tp_dictoffset */
    nullptr,                                                /* tp_init */
    nullptr,                                                /* tp_alloc */
    PyType_GenericNew,                                /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
