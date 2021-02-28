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

#include "BPy_Length2DBP1D.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace Freestyle;

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char Length2DBP1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.BinaryPredicate1D` > :class:`Length2DBP1D`\n"
    "\n"
    ".. method:: __call__(inter1, inter2)\n"
    "\n"
    "   Returns true if the 2D length of inter1 is less than the 2D length\n"
    "   of inter2.\n"
    "\n"
    "   :arg inter1: The first Interface1D object.\n"
    "   :type inter1: :class:`freestyle.types.Interface1D`\n"
    "   :arg inter2: The second Interface1D object.\n"
    "   :type inter2: :class:`freestyle.types.Interface1D`\n"
    "   :return: True or false.\n"
    "   :rtype: bool\n";

static int Length2DBP1D___init__(BPy_Length2DBP1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->py_bp1D.bp1D = new Predicates1D::Length2DBP1D();
  return 0;
}

/*-----------------------BPy_Length2DBP1D type definition ------------------------------*/

PyTypeObject Length2DBP1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "Length2DBP1D", /* tp_name */
    sizeof(BPy_Length2DBP1D),                         /* tp_basicsize */
    0,                                                /* tp_itemsize */
    nullptr,                                          /* tp_dealloc */
    0,                                                /* tp_vectorcall_offset */
    nullptr,                                          /* tp_getattr */
    nullptr,                                          /* tp_setattr */
    nullptr,                                          /* tp_reserved */
    nullptr,                                          /* tp_repr */
    nullptr,                                          /* tp_as_number */
    nullptr,                                          /* tp_as_sequence */
    nullptr,                                          /* tp_as_mapping */
    nullptr,                                          /* tp_hash  */
    nullptr,                                          /* tp_call */
    nullptr,                                          /* tp_str */
    nullptr,                                          /* tp_getattro */
    nullptr,                                          /* tp_setattro */
    nullptr,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,         /* tp_flags */
    Length2DBP1D___doc__,                             /* tp_doc */
    nullptr,                                          /* tp_traverse */
    nullptr,                                          /* tp_clear */
    nullptr,                                          /* tp_richcompare */
    0,                                                /* tp_weaklistoffset */
    nullptr,                                          /* tp_iter */
    nullptr,                                          /* tp_iternext */
    nullptr,                                          /* tp_methods */
    nullptr,                                          /* tp_members */
    nullptr,                                          /* tp_getset */
    &BinaryPredicate1D_Type,                          /* tp_base */
    nullptr,                                          /* tp_dict */
    nullptr,                                          /* tp_descr_get */
    nullptr,                                          /* tp_descr_set */
    0,                                                /* tp_dictoffset */
    (initproc)Length2DBP1D___init__,                  /* tp_init */
    nullptr,                                          /* tp_alloc */
    nullptr,                                          /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
