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

#include "BPy_VertexOrientation2DF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char VertexOrientation2DF0D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > "
    ":class:`freestyle.types.UnaryFunction0DVec2f` > :class:`VertexOrientation2DF0D`\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Builds a VertexOrientation2DF0D object.\n"
    "\n"
    ".. method:: __call__(it)\n"
    "\n"
    "   Returns a two-dimensional vector giving the 2D oriented tangent to the\n"
    "   1D element to which the :class:`freestyle.types.Interface0D` pointed\n"
    "   by the Interface0DIterator belongs.  The 2D oriented tangent is\n"
    "   evaluated at the pointed Interface0D.\n"
    "\n"
    "   :arg it: An Interface0DIterator object.\n"
    "   :type it: :class:`freestyle.types.Interface0DIterator`\n"
    "   :return: The 2D oriented tangent to the 1D element evaluated at the\n"
    "      pointed Interface0D.\n"
    "   :rtype: :class:`mathutils.Vector`\n";

static int VertexOrientation2DF0D___init__(BPy_VertexOrientation2DF0D *self,
                                           PyObject *args,
                                           PyObject *kwds)
{
  static const char *kwlist[] = {nullptr};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->py_uf0D_vec2f.uf0D_vec2f = new Functions0D::VertexOrientation2DF0D();
  self->py_uf0D_vec2f.uf0D_vec2f->py_uf0D = (PyObject *)self;
  return 0;
}

/*-----------------------BPy_VertexOrientation2DF0D type definition -----------------------------*/

PyTypeObject VertexOrientation2DF0D_Type = {
    PyVarObject_HEAD_INIT(nullptr, 0) "VertexOrientation2DF0D", /* tp_name */
    sizeof(BPy_VertexOrientation2DF0D),                         /* tp_basicsize */
    0,                                                          /* tp_itemsize */
    nullptr,                                                    /* tp_dealloc */
#if PY_VERSION_HEX >= 0x03080000
    0, /* tp_vectorcall_offset */
#else
    nullptr, /* tp_print */
#endif
    nullptr,                                   /* tp_getattr */
    nullptr,                                   /* tp_setattr */
    nullptr,                                   /* tp_reserved */
    nullptr,                                   /* tp_repr */
    nullptr,                                   /* tp_as_number */
    nullptr,                                   /* tp_as_sequence */
    nullptr,                                   /* tp_as_mapping */
    nullptr,                                   /* tp_hash  */
    nullptr,                                   /* tp_call */
    nullptr,                                   /* tp_str */
    nullptr,                                   /* tp_getattro */
    nullptr,                                   /* tp_setattro */
    nullptr,                                   /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /* tp_flags */
    VertexOrientation2DF0D___doc__,            /* tp_doc */
    nullptr,                                   /* tp_traverse */
    nullptr,                                   /* tp_clear */
    nullptr,                                   /* tp_richcompare */
    0,                                         /* tp_weaklistoffset */
    nullptr,                                   /* tp_iter */
    nullptr,                                   /* tp_iternext */
    nullptr,                                   /* tp_methods */
    nullptr,                                   /* tp_members */
    nullptr,                                   /* tp_getset */
    &UnaryFunction0DVec2f_Type,                /* tp_base */
    nullptr,                                   /* tp_dict */
    nullptr,                                   /* tp_descr_get */
    nullptr,                                   /* tp_descr_set */
    0,                                         /* tp_dictoffset */
    (initproc)VertexOrientation2DF0D___init__, /* tp_init */
    nullptr,                                   /* tp_alloc */
    nullptr,                                   /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
