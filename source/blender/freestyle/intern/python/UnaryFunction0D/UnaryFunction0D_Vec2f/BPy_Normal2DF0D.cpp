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

#include "BPy_Normal2DF0D.h"

#include "../../../view_map/Functions0D.h"

#ifdef __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////////////////////////////////

//------------------------INSTANCE METHODS ----------------------------------

static char Normal2DF0D___doc__[] =
    "Class hierarchy: :class:`freestyle.types.UnaryFunction0D` > "
    ":class:`freestyle.types.UnaryFunction0DVec2f` > :class:`Normal2DF0D`\n"
    "\n"
    ".. method:: __init__()\n"
    "\n"
    "   Builds a Normal2DF0D object.\n"
    "\n"
    ".. method:: __call__(it)\n"
    "\n"
    "   Returns a two-dimensional vector giving the normalized 2D normal to\n"
    "   the 1D element to which the :class:`freestyle.types.Interface0D`\n"
    "   pointed by the Interface0DIterator belongs.  The normal is evaluated\n"
    "   at the pointed Interface0D.\n"
    "\n"
    "   :arg it: An Interface0DIterator object.\n"
    "   :type it: :class:`freestyle.types.Interface0DIterator`\n"
    "   :return: The 2D normal of the 1D element evaluated at the pointed\n"
    "      Interface0D.\n"
    "   :rtype: :class:`mathutils.Vector`\n";

static int Normal2DF0D___init__(BPy_Normal2DF0D *self, PyObject *args, PyObject *kwds)
{
  static const char *kwlist[] = {NULL};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "", (char **)kwlist)) {
    return -1;
  }
  self->py_uf0D_vec2f.uf0D_vec2f = new Functions0D::Normal2DF0D();
  self->py_uf0D_vec2f.uf0D_vec2f->py_uf0D = (PyObject *)self;
  return 0;
}

/*-----------------------BPy_Normal2DF0D type definition ------------------------------*/

PyTypeObject Normal2DF0D_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "Normal2DF0D", /* tp_name */
    sizeof(BPy_Normal2DF0D),                      /* tp_basicsize */
    0,                                            /* tp_itemsize */
    0,                                            /* tp_dealloc */
    0,                                            /* tp_print */
    0,                                            /* tp_getattr */
    0,                                            /* tp_setattr */
    0,                                            /* tp_reserved */
    0,                                            /* tp_repr */
    0,                                            /* tp_as_number */
    0,                                            /* tp_as_sequence */
    0,                                            /* tp_as_mapping */
    0,                                            /* tp_hash  */
    0,                                            /* tp_call */
    0,                                            /* tp_str */
    0,                                            /* tp_getattro */
    0,                                            /* tp_setattro */
    0,                                            /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,     /* tp_flags */
    Normal2DF0D___doc__,                          /* tp_doc */
    0,                                            /* tp_traverse */
    0,                                            /* tp_clear */
    0,                                            /* tp_richcompare */
    0,                                            /* tp_weaklistoffset */
    0,                                            /* tp_iter */
    0,                                            /* tp_iternext */
    0,                                            /* tp_methods */
    0,                                            /* tp_members */
    0,                                            /* tp_getset */
    &UnaryFunction0DVec2f_Type,                   /* tp_base */
    0,                                            /* tp_dict */
    0,                                            /* tp_descr_get */
    0,                                            /* tp_descr_set */
    0,                                            /* tp_dictoffset */
    (initproc)Normal2DF0D___init__,               /* tp_init */
    0,                                            /* tp_alloc */
    0,                                            /* tp_new */
};

///////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
