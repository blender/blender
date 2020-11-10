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

#include "BPy_ShapeUP1D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char ShapeUP1D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryPredicate1D` > :class:`ShapeUP1D`\n"
    "\n"
    ".. method:: __init__(first, second=0)\n"
    "\n"
    "   Builds a ShapeUP1D object.\n"
    "\n"
    "   :arg first: The first Id component.\n"
    "   :type first: int\n"
    "   :arg second: The second Id component.\n"
    "   :type second: int\n"
    "\n"
    ".. method:: __call__(inter)\n"
    "\n"
    "   Returns true if the shape to which the Interface1D belongs to has the\n"
    "   same :class:`freestyle.types.Id` as the one specified by the user.\n"
    "\n"
    "   :arg inter: An Interface1D object.\n"
    "   :type inter: :class:`freestyle.types.Interface1D`\n"
    "   :return: True if Interface1D belongs to the shape of the\n"
    "      user-specified Id.\n"
    "   :rtype: bool\n";

static int ShapeUP1D___init__(BPy_ShapeUP1D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {"first", "second", nullptr};
  unsigned u1, u2 = 0;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "I|I", (char **)kwlist, &u1, &u2)) {
    return -1;
  }
  self->py_up1D.up1D = new Predicates1D::ShapeUP1D(u1, u2);
  return 0;
}

/*-----------------------BPy_ShapeUP1D type definition ------------------------------*/

PyTypeObject ShapeUP1D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "ShapeUP1D", /* tp_name */
    sizeof(BPy_ShapeUP1D),                         /* tp_basicsize */
    0,                                             /* tp_itemsize */
    nullptr,                                       /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                  /* tp_getattr */
    nullptr,                                  /* tp_setattr */
    nullptr,                                  /* tp_reserved */
    nullptr,                                  /* tp_repr */
    nullptr,                                  /* tp_as_number */
    nullptr,                                  /* tp_as_sequence */
    nullptr,                                  /* tp_as_mapping */
    nullptr,                                  /* tp_hash  */
    nullptr,                                  /* tp_call */
    nullptr,                                  /* tp_str */
    nullptr,                                  /* tp_getattro */
    nullptr,                                  /* tp_setattro */
    nullptr,                                  /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    ShapeUP1D___doc__,                        /* tp_doc */
    nullptr,                                  /* tp_traverse */
    nullptr,                                  /* tp_clear */
    nullptr,                                  /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    nullptr,                                  /* tp_iter */
    nullptr,                                  /* tp_iternext */
    nullptr,                                  /* tp_methods */
    nullptr,                                  /* tp_members */
    nullptr,                                  /* tp_getset */
    &UnaryPredicate1D_Type,                   /* tp_base */
    nullptr,                                  /* tp_dict */
    nullptr,                                  /* tp_descr_get */
    nullptr,                                  /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    (initproc)ShapeUP1D___init__,             /* tp_init */
    nullptr,                                  /* tp_alloc */
    nullptr,                                  /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
