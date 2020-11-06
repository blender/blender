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

#include "BPy_UnaryFunction1D.h"

#include "UnaryFunction1D/BPy_UnaryFunction1DDouble.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DEdgeNature.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DFloat.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DUnsigned.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVec2f.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVec3f.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVectorViewShape.h"
#include "UnaryFunction1D/BPy_UnaryFunction1DVoid.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//-------------------MODULE INITIALIZATION--------------------------------
int UnaryFunction1D_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&UnaryFunction1D_Type) < 0) {
    return -1;
  }
  Py_INCREF(&UnaryFunction1D_Type);
  PyModule_AddObject(module, "UnaryFunction1D", (PyObject *)&UnaryFunction1D_Type);

  UnaryFunction1DDouble_Init(module);
  UnaryFunction1DEdgeNature_Init(module);
  UnaryFunction1DFloat_Init(module);
  UnaryFunction1DUnsigned_Init(module);
  UnaryFunction1DVec2f_Init(module);
  UnaryFunction1DVec3f_Init(module);
  UnaryFunction1DVectorViewShape_Init(module);
  UnaryFunction1DVoid_Init(module);

  return 0;
}

//------------------------INSTANCE METHODS ----------------------------------

static char UnaryFunction1D___doc__[] =
    "Base class for Unary Functions (functors) working on\n"
    ":class:`Interface1D`.  A unary function will be used by invoking\n"
    "__call__() on an Interface1D.  In Python, several different subclasses\n"
    "of UnaryFunction1D are used depending on the types of functors' return\n"
    "values.  For example, you would inherit from a\n"
    ":class:`UnaryFunction1DDouble` if you wish to define a function that\n"
    "returns a double value.  Available UnaryFunction1D subclasses are:\n"
    "\n"
    "* :class:`UnaryFunction1DDouble`\n"
    "* :class:`UnaryFunction1DEdgeNature`\n"
    "* :class:`UnaryFunction1DFloat`\n"
    "* :class:`UnaryFunction1DUnsigned`\n"
    "* :class:`UnaryFunction1DVec2f`\n"
    "* :class:`UnaryFunction1DVec3f`\n"
    "* :class:`UnaryFunction1DVectorViewShape`\n"
    "* :class:`UnaryFunction1DVoid`\n";

static void UnaryFunction1D___dealloc__(BPy_UnaryFunction1D *self)
{
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *UnaryFunction1D___repr__(BPy_UnaryFunction1D * /*self*/)
{
  return PyUnicode_FromString("UnaryFunction1D");
}

/*----------------------UnaryFunction1D get/setters ----------------------------*/

PyDoc_STRVAR(UnaryFunction1D_name_doc,
             "The name of the unary 1D function.\n"
             "\n"
             ":type: str");

static PyObject *UnaryFunction1D_name_get(BPy_UnaryFunction1D *self, void *UNUSED(closure))
{
  return PyUnicode_FromString(Py_TYPE(self)->tp_name);
}

static PyGetSetDef BPy_UnaryFunction1D_getseters[] = {
    {"name", (getter)UnaryFunction1D_name_get, (setter)nullptr, UnaryFunction1D_name_doc, nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr} /* Sentinel */
};

/*-----------------------BPy_UnaryFunction1D type definition ------------------------------*/

PyTypeObject UnaryFunction1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "UnaryFunction1D", /* tp_name */
    sizeof(BPy_UnaryFunction1D),                      /* tp_basicsize */
    0,                                                /* tp_itemsize */
    (destructor)UnaryFunction1D___dealloc__,          /* tp_dealloc */
    nullptr,                                                /* tp_print */
    nullptr,                                                /* tp_getattr */
    nullptr,                                                /* tp_setattr */
    nullptr,                                                /* tp_reserved */
    (reprfunc)UnaryFunction1D___repr__,               /* tp_repr */
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
    UnaryFunction1D___doc__,                          /* tp_doc */
    nullptr,                                                /* tp_traverse */
    nullptr,                                                /* tp_clear */
    nullptr,                                                /* tp_richcompare */
    0,                                                /* tp_weaklistoffset */
    nullptr,                                                /* tp_iter */
    nullptr,                                                /* tp_iternext */
    nullptr,                                                /* tp_methods */
    nullptr,                                                /* tp_members */
    BPy_UnaryFunction1D_getseters,                    /* tp_getset */
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
